####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package BackRest::Backup;

use threads;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use Fcntl 'SEEK_CUR';
use File::Basename;
use File::Path qw(remove_tree);
use Scalar::Util qw(looks_like_number);
use Thread::Queue;

use lib dirname($0);
use BackRest::Archive;
use BackRest::BackupCommon;
use BackRest::BackupFile;
use BackRest::BackupInfo;
use BackRest::Config;
use BackRest::Db;
use BackRest::Exception;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::ThreadGroup;
use BackRest::Utility;

our @EXPORT = qw(backup_init backup_cleanup backup backup_expire archive_list_get);

my $oDb;
my $oFile;
my $strType;        # Type of backup: full, differential (diff), incremental (incr)
my $bCompress;
my $bHardLink;
my $bNoStartStop;
my $bForce;
my $iThreadMax;
my $iThreadTimeout;

####################################################################################################################################
# BACKUP_INIT
####################################################################################################################################
sub backup_init
{
    my $oDbParam = shift;
    my $oFileParam = shift;
    my $strTypeParam = shift;
    my $bCompressParam = shift;
    my $bHardLinkParam = shift;
    my $iThreadMaxParam = shift;
    my $iThreadTimeoutParam = shift;
    my $bNoStartStopParam = shift;
    my $bForceParam = shift;

    $oDb = $oDbParam;
    $oFile = $oFileParam;
    $strType = $strTypeParam;
    $bCompress = $bCompressParam;
    $bHardLink = $bHardLinkParam;
    $iThreadMax = $iThreadMaxParam;
    $iThreadTimeout = $iThreadTimeoutParam;
    $bNoStartStop = $bNoStartStopParam;
    $bForce = $bForceParam;
}

####################################################################################################################################
# BACKUP_CLEANUP
####################################################################################################################################
sub backup_cleanup
{
    undef($oFile);
}

####################################################################################################################################
# BACKUP_TYPE_FIND - Find the last backup depending on the type
####################################################################################################################################
sub backup_type_find
{
    my $strType = shift;
    my $strBackupClusterPath = shift;

    my $strDirectory;

    if ($strType eq BACKUP_TYPE_INCR)
    {
        $strDirectory = ($oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 1, 1), 'reverse'))[0];
    }

    if (!defined($strDirectory) && $strType ne BACKUP_TYPE_FULL)
    {
        $strDirectory = ($oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 0, 0), 'reverse'))[0];
    }

    return $strDirectory;
}

####################################################################################################################################
# BACKUP_FILE_NOT_IN_MANIFEST - Find all files in a backup path that are not in the supplied manifest
####################################################################################################################################
sub backup_file_not_in_manifest
{
    my $strPathType = shift;
    my $oManifest = shift;
    my $oAbortedManifest = shift;

    my %oFileHash;
    $oFile->manifest($strPathType, undef, \%oFileHash);

    my @stryFile;

    foreach my $strName (sort(keys(%{$oFileHash{name}})))
    {
        # Ignore certain files that will never be in the manifest
        if ($strName eq FILE_MANIFEST ||
            $strName eq '.')
        {
            next;
        }

        if ($strName eq MANIFEST_KEY_BASE)
        {
            if ($oManifest->test(MANIFEST_SECTION_BACKUP_PATH, $strName))
            {
                next;
            }
        }
        elsif ($strName eq MANIFEST_TABLESPACE)
        {
            my $bFound = false;

            foreach my $strPath ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
            {
                if ($strPath =~ /^$strName\//)
                {
                    $bFound = true;
                    last;
                }
            }

            next if $bFound;
        }
        else
        {
            my $strBasePath = (split('/', $strName))[0];
            my $strPath = substr($strName, length($strBasePath) + 1);

            # Create the section from the base path
            my $strSection = $strBasePath;

            if ($strSection eq 'tablespace')
            {
                my $strTablespace = (split('/', $strPath))[0];

                $strSection = $strSection . '/' . $strTablespace;

                if ($strTablespace eq $strPath)
                {
                    if ($oManifest->test("${strSection}:path"))
                    {
                        next;
                    }
                }

                $strPath = substr($strPath, length($strTablespace) + 1);
            }

            my $cType = $oFileHash{name}{"${strName}"}{type};

            if ($cType eq 'd')
            {
                if ($oManifest->test("${strSection}:path", "${strPath}"))
                {
                    next;
                }
            }
            elsif ($cType eq 'f')
            {
                if ($oManifest->test("${strSection}:file", "${strPath}") &&
                    !$oManifest->test("${strSection}:file", $strPath, MANIFEST_SUBKEY_REFERENCE))
                {
                    my $strChecksum = $oAbortedManifest->get("${strSection}:file", $strPath, MANIFEST_SUBKEY_CHECKSUM, false);

                    if (defined($strChecksum) &&
                        $oManifest->getNumeric("${strSection}:file", $strPath, MANIFEST_SUBKEY_SIZE) ==
                            $oFileHash{name}{$strName}{size} &&
                        $oManifest->getNumeric("${strSection}:file", $strPath, MANIFEST_SUBKEY_TIMESTAMP) ==
                            $oFileHash{name}{$strName}{modification_time})
                    {
                        $oManifest->set("${strSection}:file", $strPath, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
                        next;
                    }
                }
            }
        }

        push @stryFile, $strName;
    }

    return @stryFile;
}

####################################################################################################################################
# BACKUP_TMP_CLEAN
#
# Cleans the temp directory from a previous failed backup so it can be reused
####################################################################################################################################
sub backup_tmp_clean
{
    my $oManifest = shift;
    my $oAbortedManifest = shift;

    &log(INFO, 'cleaning backup tmp path');

    # Remove the pg_xlog directory since it contains nothing useful for the new backup
    if (-e $oFile->path_get(PATH_BACKUP_TMP, 'base/pg_xlog'))
    {
        remove_tree($oFile->path_get(PATH_BACKUP_TMP, 'base/pg_xlog')) or confess &log(ERROR, 'unable to delete tmp pg_xlog path');
    }

    # Remove the pg_tblspc directory since it is trivial to rebuild, but hard to compare
    if (-e $oFile->path_get(PATH_BACKUP_TMP, 'base/pg_tblspc'))
    {
        remove_tree($oFile->path_get(PATH_BACKUP_TMP, 'base/pg_tblspc')) or confess &log(ERROR, 'unable to delete tmp pg_tblspc path');
    }

    # Get the list of files that should be deleted from temp
    my @stryFile = backup_file_not_in_manifest(PATH_BACKUP_TMP, $oManifest, $oAbortedManifest);

    foreach my $strFile (sort {$b cmp $a} @stryFile)
    {
        my $strDelete = $oFile->path_get(PATH_BACKUP_TMP, $strFile);

        # If a path then delete it, all the files should have already been deleted since we are going in reverse order
        if (-d $strDelete)
        {
            &log(DEBUG, "remove path ${strDelete}");
            rmdir($strDelete) or confess &log(ERROR, "unable to delete path ${strDelete}, is it empty?");
        }
        # Else delete a file
        else
        {
            &log(DEBUG, "remove file ${strDelete}");
            unlink($strDelete) or confess &log(ERROR, "unable to delete file ${strDelete}");
        }
    }
}

####################################################################################################################################
# BACKUP_FILE - Performs the file level backup
#
# Uses the information in the manifest to determine which files need to be copied.  Directories and tablespace links are only
# created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub backup_file
{
    my $strDbClusterPath = shift;   # Database base data path
    my $oBackupManifest = shift;    # Manifest for the current backup

    # Variables used for parallel copy
    my %oFileCopyMap;
    my $lFileTotal = 0;
    my $lSizeTotal = 0;

    # Determine whether all paths and links will be created
    my $bFullCreate = $bHardLink || $strType eq BACKUP_TYPE_FULL;

    # Iterate through the path sections of the manifest to backup
    foreach my $strPathKey ($oBackupManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        # Determine the source and destination backup paths
        my $strBackupSourcePath;        # Absolute path to the database base directory or tablespace to backup
        my $strBackupDestinationPath;   # Relative path to the backup directory where the data will be stored

        $strBackupSourcePath = $oBackupManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);
        $strBackupDestinationPath = $strPathKey;

        # Create links for tablespaces
        if ($oBackupManifest->test(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_LINK) && $bFullCreate)
        {
            $oFile->link_create(PATH_BACKUP_TMP, $strBackupDestinationPath,
                                PATH_BACKUP_TMP,
                                'base/pg_tblspc/' . $oBackupManifest->get(MANIFEST_SECTION_BACKUP_PATH,
                                                                          $strPathKey, MANIFEST_SUBKEY_LINK),
                                false, true, true);
        }

        # If this is a full backup or hard-linked then create all paths and links
        if ($bFullCreate)
        {
            # Create paths
            my $strSectionPath = "$strPathKey:path";

            if ($oBackupManifest->test($strSectionPath))
            {
                foreach my $strPath ($oBackupManifest->keys($strSectionPath))
                {
                    if ($strPath ne '.')
                    {
                        $oFile->path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}");
                    }
                }
            }

            # Create links
            my $strSectionLink = "$strPathKey:link";

            if ($oBackupManifest->test($strSectionLink))
            {
                foreach my $strLink ($oBackupManifest->keys($strSectionLink))
                {
                    # Create links except in pg_tblspc because they have already been created
                    if (!($strPathKey eq 'base' && $strLink =~ /^pg_tblspc\/.*/))
                    {
                        $oFile->link_create(PATH_BACKUP_ABSOLUTE,
                                            $oBackupManifest->get($strSectionLink, $strLink, MANIFEST_SUBKEY_DESTINATION),
                                            PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strLink}",
                                            false, false, false);
                    }
                }
            }
        }


        # Possible for the file section to exist with no files (i.e. empty tablespace)
        my $strSectionFile = "$strPathKey:file";

        # Iterate through the files for each backup source path
        foreach my $strFile ($oBackupManifest->keys($strSectionFile))
        {
            my $strBackupSourceFile = "${strBackupSourcePath}/${strFile}";

            # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
            # However, if hard-linking is turned on the link will need to be created
            my $bProcess = true;
            my $strReference = $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_REFERENCE, false);

            if (defined($strReference))
            {
                # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
                if ($bHardLink)
                {
                    &log(DEBUG, "hardlink ${strBackupSourceFile} to ${strReference}");

                    $oFile->link_create(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                        PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true, false, true);
                }
                else
                {
                    &log(DEBUG, "reference ${strBackupSourceFile} to ${strReference}");
                }

                $bProcess = false;
            }

            if ($bProcess)
            {
                my $lFileSize = $oBackupManifest->getNumeric($strSectionFile, $strFile, MANIFEST_SUBKEY_SIZE);

                # Setup variables needed for threaded copy
                $lFileTotal++;
                $lSizeTotal += $lFileSize;

                my $strFileKey = sprintf("%016d-${strFile}", $lFileSize);

                $oFileCopyMap{$strPathKey}{$strFileKey}{db_file} = $strBackupSourceFile;
                $oFileCopyMap{$strPathKey}{$strFileKey}{file_section} = $strSectionFile;
                $oFileCopyMap{$strPathKey}{$strFileKey}{file} = ${strFile};
                $oFileCopyMap{$strPathKey}{$strFileKey}{backup_file} = "${strBackupDestinationPath}/${strFile}";
                $oFileCopyMap{$strPathKey}{$strFileKey}{size} = $lFileSize;
                $oFileCopyMap{$strPathKey}{$strFileKey}{modification_time} =
                    $oBackupManifest->getNumeric($strSectionFile, $strFile, MANIFEST_SUBKEY_TIMESTAMP, false);
                $oFileCopyMap{$strPathKey}{$strFileKey}{checksum} =
                    $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_CHECKSUM, false);
            }
        }
    }

    # If there are no files to backup then we'll exit with a warning unless in test mode.  The other way this could happen is if
    # the database is down and backup is called with --no-start-stop twice in a row.
    if ($lFileTotal == 0)
    {
        if (!optionGet(OPTION_TEST))
        {
            confess &log(ERROR, "no files have changed since the last backup - this seems unlikely");
        }

        return;
    }

    # Create backup and result queues
    my $oResultQueue = Thread::Queue->new();
    my @oyBackupQueue;

    # Variables used for local copy
    my $lSizeCurrent = 0;       # Running total of bytes copied
    my $bCopied;                # Was the file copied?
    my $lCopySize;              # Size reported by copy
    my $strCopyChecksum;        # Checksum reported by copy

    # Determine how often the manifest will be saved
    my $lManifestSaveCurrent = 0;
    my $lManifestSaveSize = int($lSizeTotal / 100);

    if ($lManifestSaveSize < optionGet(OPTION_MANIFEST_SAVE_THRESHOLD))
    {
        $lManifestSaveSize = optionGet(OPTION_MANIFEST_SAVE_THRESHOLD);
    }

    # Iterate all backup files
    foreach my $strPathKey (sort (keys %oFileCopyMap))
    {
        if ($iThreadMax > 1)
        {
            $oyBackupQueue[@oyBackupQueue] = Thread::Queue->new();
        }

        foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oFileCopyMap{$strPathKey}})))
        {
            my $oFileCopy = $oFileCopyMap{$strPathKey}{$strFileKey};

            if ($iThreadMax > 1)
            {
                $oyBackupQueue[@oyBackupQueue - 1]->enqueue($oFileCopy);
            }
            else
            {
                # Backup the file
                ($bCopied, $lSizeCurrent, $lCopySize, $strCopyChecksum) =
                    backupFile($oFile, $$oFileCopy{db_file}, $$oFileCopy{backup_file}, $bCompress,
                               $$oFileCopy{checksum}, $$oFileCopy{modification_time},
                               $$oFileCopy{size}, $lSizeTotal, $lSizeCurrent);

                $lManifestSaveCurrent = backupManifestUpdate($oBackupManifest, $$oFileCopy{file_section}, $$oFileCopy{file},
                                                             $bCopied, $lCopySize, $strCopyChecksum, $lManifestSaveSize,
                                                             $lManifestSaveCurrent);
            }
        }
    }

    # If multi-threaded then create threads to copy files
    if ($iThreadMax > 1)
    {
        for (my $iThreadIdx = 0; $iThreadIdx < $iThreadMax; $iThreadIdx++)
        {
            my %oParam;

            $oParam{compress} = $bCompress;
            $oParam{size_total} = $lSizeTotal;
            $oParam{queue} = \@oyBackupQueue;
            $oParam{result_queue} = $oResultQueue;

            threadGroupRun($iThreadIdx, 'backup', \%oParam);
        }

        # Complete thread queues
        my $bDone = false;

        do
        {
            $bDone = threadGroupComplete();

            # Read the messages that are passed back from the backup threads
            while (my $oMessage = $oResultQueue->dequeue_nb())
            {
                &log(TRACE, "message received in master queue: section = $$oMessage{file_section}, file = $$oMessage{file}" .
                            ", copied = $$oMessage{copied}");

                $lManifestSaveCurrent = backupManifestUpdate($oBackupManifest, $$oMessage{file_section}, $$oMessage{file},
                                                      $$oMessage{copied}, $$oMessage{size}, $$oMessage{checksum},
                                                      $lManifestSaveSize, $lManifestSaveCurrent);
            }
        }
        while (!$bDone);
    }

    &log(INFO, 'total backup size: ' . file_size_format($lSizeTotal));
}

####################################################################################################################################
# BACKUP
#
# Performs the entire database backup.
####################################################################################################################################
sub backup
{
    my $strDbClusterPath = shift;
    my $bStartFast = shift;

    # Record timestamp start
    my $lTimestampStart = time();

    # Backup start
    &log(INFO, "backup start: type = ${strType}");

    # Not supporting remote backup hosts yet
    if ($oFile->is_remote(PATH_BACKUP))
    {
        confess &log(ERROR, 'remote backup host not currently supported');
    }

    if (!defined($strDbClusterPath))
    {
        confess &log(ERROR, 'cluster data path is not defined');
    }

    &log(DEBUG, "cluster path is $strDbClusterPath");

    # Create the cluster backup path
    $oFile->path_create(PATH_BACKUP_CLUSTER, undef, undef, true);

    # Load or build backup.info
    my $oBackupInfo = new BackRest::BackupInfo($oFile->path_get(PATH_BACKUP_CLUSTER));

    # Build backup tmp and config
    my $strBackupTmpPath = $oFile->path_get(PATH_BACKUP_TMP);
    my $strBackupConfFile = $oFile->path_get(PATH_BACKUP_TMP, 'backup.manifest');

    # Declare the backup manifest
    my $oBackupManifest = new BackRest::Manifest($strBackupConfFile, false);

    # Find the previous backup based on the type
    my $oLastManifest = undef;

    my $strBackupLastPath = backup_type_find($strType, $oFile->path_get(PATH_BACKUP_CLUSTER));

    if (defined($strBackupLastPath))
    {
        $oLastManifest = new BackRest::Manifest($oFile->path_get(PATH_BACKUP_CLUSTER) . "/${strBackupLastPath}/backup.manifest");

        &log(INFO, 'last backup label = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) .
                   ', version = ' . $oLastManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));
    }
    else
    {
        if ($strType eq BACKUP_TYPE_DIFF)
        {
            &log(WARN, 'No full backup exists, differential backup has been changed to full');
        }
        elsif ($strType eq BACKUP_TYPE_INCR)
        {
            &log(WARN, 'No prior backup exists, incremental backup has been changed to full');
        }

        $strType = BACKUP_TYPE_FULL;
    }

    # Backup settings
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oBackupManifest->setNumeric(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestampStart);
    $oBackupManifest->setBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress);
    $oBackupManifest->setBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink);
    $oBackupManifest->setBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_START_STOP, undef, !$bNoStartStop);
    $oBackupManifest->setBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                              $bNoStartStop || optionGet(OPTION_BACKUP_ARCHIVE_COPY));
    $oBackupManifest->setBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                              $bNoStartStop || optionGet(OPTION_BACKUP_ARCHIVE_CHECK));

    # Database info
    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDb->info($oFile, $strDbClusterPath);

    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, $strDbVersion);
    $oBackupManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iControlVersion);
    $oBackupManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iCatalogVersion);
    $oBackupManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $ullDbSysId);

    $oBackupInfo->check($oBackupManifest);

    # Start backup (unless no-start-stop is set)
    my $strArchiveStart;

    if ($bNoStartStop)
    {
        if ($oFile->exists(PATH_DB_ABSOLUTE, $strDbClusterPath . '/' . FILE_POSTMASTER_PID))
        {
            if ($bForce)
            {
                &log(WARN, '--no-start-stop passed and ' . FILE_POSTMASTER_PID . ' exists but --force was passed so backup will ' .
                           'continue though it looks like the postmaster is running and the backup will probably not be ' .
                           'consistent');
            }
            else
            {
                &log(ERROR, '--no-start-stop passed but ' . FILE_POSTMASTER_PID . ' exists - looks like the postmaster is ' .
                            'running. Shutdown the postmaster and try again, or use --force.');
                exit 1;
            }
        }
    }
    else
    {
        my $strTimestampDbStart;

        ($strArchiveStart, $strTimestampDbStart) =
            $oDb->backup_start(BACKREST_EXE . ' backup started ' . timestamp_string_get(undef, $lTimestampStart), $bStartFast);

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        &log(INFO, "archive start: ${strArchiveStart}");
    }

    # Build the backup manifest
    my $oTablespaceMap = $bNoStartStop ? undef : $oDb->tablespace_map_get();

    $oBackupManifest->build($oFile, $strDbClusterPath, $oLastManifest, $bNoStartStop, $oTablespaceMap);
    &log(TEST, TEST_MANIFEST_BUILD);

    # Check if an aborted backup exists for this stanza
    if (-e $strBackupTmpPath)
    {
        my $bUsable = false;

        my $strType = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
        my $strPrior = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
        my $strVersion = $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);

        my $strAbortedType = '<undef>';
        my $strAbortedPrior = '<undef>';
        my $strAbortedVersion = '<undef>';
        my $oAbortedManifest;

        # Attempt to read the manifest file in the aborted backup to see if the backup type and prior backup are the same as the
        # new backup that is being started.  If any error at all occurs then the backup will be considered unusable and a resume
        # will not be attempted.
        eval
        {
            # Load the aborted manifest
            $oAbortedManifest = new BackRest::Manifest("${strBackupTmpPath}/backup.manifest");

            # Default values if they are not set
            $strAbortedType = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
            $strAbortedPrior = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
            $strAbortedVersion = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);

            # The backup is usable if between the current backup and the aborted backup:
            # 1) The version matches
            # 2) The type of both is full or the types match and prior matches
            if ($strAbortedVersion eq $strVersion)
            {
                if ($strAbortedType eq BACKUP_TYPE_FULL
                    && $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) eq BACKUP_TYPE_FULL)
                {
                    $bUsable = true;
                }
                elsif ($strAbortedType eq $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) &&
                       $strAbortedPrior eq $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR))
                {
                    $bUsable = true;
                }
            }
        };

        # If the aborted backup is usable then clean it
        if ($bUsable && optionGet(OPTION_RESUME))
        {
            &log(WARN, 'aborted backup of same type exists, will be cleaned to remove invalid files and resumed');
            &log(TEST, TEST_BACKUP_RESUME);

            # Clean the old backup tmp path
            backup_tmp_clean($oBackupManifest, $oAbortedManifest);
        }
        # Else remove it
        else
        {
            my $strReason = "resume is disabled";

            if (optionGet(OPTION_RESUME))
            {
                if ($strVersion eq $strAbortedVersion)
                {
                    if ($strType ne $strAbortedType)
                    {
                        $strReason = "new type '${strType}' does not match aborted type '${strAbortedType}'";
                    }
                    else
                    {
                        $strReason = "new prior '${strPrior}' does not match aborted prior '${strAbortedPrior}'";
                    }
                }
                else
                {
                    $strReason = "new version '${strVersion}' does not match aborted version '${strVersion}'";
                }
            }

            &log(WARN, "aborted backup exists, but cannot be resumed (${strReason}) - will be dropped and recreated");
            &log(TEST, TEST_BACKUP_NORESUME);

            remove_tree($oFile->path_get(PATH_BACKUP_TMP))
                or confess &log(ERROR, "unable to delete tmp path: ${strBackupTmpPath}");
            $oFile->path_create(PATH_BACKUP_TMP);
        }
    }
    # Else create the backup tmp path
    else
    {
        &log(DEBUG, "creating backup path ${strBackupTmpPath}");
        $oFile->path_create(PATH_BACKUP_TMP);
    }

    # Save the backup manifest
    $oBackupManifest->save();

    # Perform the backup
    backup_file($strDbClusterPath, $oBackupManifest);

    # Stop backup (unless no-start-stop is set)
    my $strArchiveStop;

    if (!$bNoStartStop)
    {
        my $strTimestampDbStop;
        ($strArchiveStop, $strTimestampDbStop) = $oDb->backup_stop();

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);

        &log(INFO, 'archive stop: ' . $strArchiveStop);
    }

    # If archive logs are required to complete the backup, then check them.  This is the default, but can be overridden if the
    # archive logs are going to a different server.  Be careful here because there is no way to verify that the backup will be
    # consistent - at least not here.
    if (!optionGet(OPTION_NO_START_STOP) && optionGet(OPTION_BACKUP_ARCHIVE_CHECK))
    {
        # Save the backup manifest a second time - before getting archive logs in case that fails
        $oBackupManifest->save();

        # Create the modification time for the archive logs
        my $lModificationTime = time();

        # After the backup has been stopped, need to make a copy of the archive logs need to make the db consistent
        &log(DEBUG, "retrieving archive logs ${strArchiveStart}:${strArchiveStop}");
        my $oArchive = new BackRest::Archive();
        my $strArchiveId = $oArchive->getCheck($oFile);
        my @stryArchive = $oArchive->range($strArchiveStart, $strArchiveStop, $oDb->versionGet() < 9.3);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchiveFile = $oArchive->walFileName($oFile, $strArchiveId, $strArchive, 600);

            if (optionGet(OPTION_BACKUP_ARCHIVE_COPY))
            {
                &log(DEBUG, "archiving: ${strArchive} (${strArchiveFile})");

                # Copy the log file from the archive repo to the backup
                my $strDestinationFile = "base/pg_xlog/${strArchive}" . ($bCompress ? ".$oFile->{strCompressExtension}" : '');
                my $bArchiveCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$";

                my ($bCopyResult, $strCopyChecksum, $lCopySize) =
                    $oFile->copy(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strArchiveFile}",
                                 PATH_BACKUP_TMP, $strDestinationFile,
                                 $bArchiveCompressed, $bCompress,
                                 undef, $lModificationTime, undef, true);

                # Add the archive file to the manifest so it can be part of the restore and checked in validation
                my $strPathSection = 'base:path';
                my $strPathLog = 'pg_xlog';
                my $strFileSection = 'base:file';
                my $strFileLog = "pg_xlog/${strArchive}";

                # Compare the checksum against the one already in the archive log name
                if ($strArchiveFile !~ "^${strArchive}-${strCopyChecksum}(\\.$oFile->{strCompressExtension}){0,1}\$")
                {
                    confess &log(ERROR, "error copying WAL segment '${strArchiveFile}' to backup - checksum recorded with " .
                                        "file does not match actual checksum of '${strCopyChecksum}'", ERROR_CHECKSUM);
                }

                # Set manifest values
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_USER,
                                      $oBackupManifest->get($strPathSection, $strPathLog, MANIFEST_SUBKEY_USER));
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_GROUP,
                                      $oBackupManifest->get($strPathSection, $strPathLog, MANIFEST_SUBKEY_GROUP));
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_MODE, '0700');
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_TIMESTAMP, $lModificationTime);
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_SIZE, $lCopySize);
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_CHECKSUM, $strCopyChecksum);
            }
        }
    }

    # Create the path for the new backup
    my $lTimestampStop = time();
    my $strBackupPath;

    if ($strType eq BACKUP_TYPE_FULL || !defined($strBackupLastPath))
    {
        $strBackupPath = timestamp_file_string_get() . 'F';
        $strType = BACKUP_TYPE_FULL;
    }
    else
    {
        $strBackupPath = substr($strBackupLastPath, 0, 16);

        $strBackupPath .= '_' . timestamp_file_string_get(undef, $lTimestampStop);

        if ($strType eq BACKUP_TYPE_DIFF)
        {
            $strBackupPath .= 'D';
        }
        else
        {
            $strBackupPath .= 'I';
        }
    }

    # Record timestamp stop in the config
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestampStop + 0);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupPath);

    # Save the backup manifest final time
    $oBackupManifest->save();

    &log(INFO, "new backup label: ${strBackupPath}");

    # Rename the backup tmp path to complete the backup
    &log(DEBUG, "moving ${strBackupTmpPath} to " . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackupPath));
    $oFile->move(PATH_BACKUP_TMP, undef, PATH_BACKUP_CLUSTER, $strBackupPath);

    # Create a link to the most recent backup
    $oFile->remove(PATH_BACKUP_CLUSTER, "latest");
    $oFile->link_create(PATH_BACKUP_CLUSTER, $strBackupPath, PATH_BACKUP_CLUSTER, "latest", undef, true);

    # Save backup info
    $oBackupInfo->backupAdd($oFile, $oBackupManifest);

    # Backup stop
    &log(INFO, 'backup stop');
}

####################################################################################################################################
# BACKUP_EXPIRE
#
# Removes expired backups and archive logs from the backup directory.  Partial backups are not counted for expiration, so if full
# or differential retention is set to 2, there must be three complete backups before the oldest one can be deleted.
#
# iFullRetention - Optional, must be greater than 0 when supplied.
# iDifferentialRetention - Optional, must be greater than 0 when supplied.
# strArchiveRetention - Optional, must be (full,differential/diff,incremental/incr) when supplied
# iArchiveRetention - Required when strArchiveRetention is supplied.  Must be greater than 0.
####################################################################################################################################
sub backup_expire
{
    my $strBackupClusterPath = shift;       # Base path to cluster backup
    my $iFullRetention = shift;             # Number of full backups to keep
    my $iDifferentialRetention = shift;     # Number of differential backups to keep
    my $strArchiveRetentionType = shift;    # Type of backup to base archive retention on
    my $iArchiveRetention = shift;          # Number of backups worth of archive to keep

    my $strPath;
    my @stryPath;

    # Load or build backup.info
    my $oBackupInfo = new BackRest::BackupInfo($oFile->path_get(PATH_BACKUP_CLUSTER));

    # Find all the expired full backups
    if (defined($iFullRetention))
    {
        # Make sure iFullRetention is valid
        if (!looks_like_number($iFullRetention) || $iFullRetention < 1)
        {
            confess &log(ERROR, 'full_rentention must be a number >= 1');
        }

        my $iIndex = $iFullRetention;
        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 0, 0), 'reverse');

        while (defined($stryPath[$iIndex]))
        {
            # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
            # be consistent if the process dies
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, '^' . $stryPath[$iIndex] . '.*', 'reverse'))
            {
                system("rm -rf ${strBackupClusterPath}/${strPath}") == 0
                    or confess &log(ERROR, "unable to delete backup ${strPath}");

                $oBackupInfo->backupRemove($strPath);
            }

            &log(INFO, 'remove expired full backup: ' . $stryPath[$iIndex]);

            $iIndex++;
        }
    }

    # Find all the expired differential backups
    if (defined($iDifferentialRetention))
    {
        # Make sure iDifferentialRetention is valid
        if (!looks_like_number($iDifferentialRetention) || $iDifferentialRetention < 1)
        {
            confess &log(ERROR, 'differential_rentention must be a number >= 1');
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(0, 1, 0), 'reverse');

        if (defined($stryPath[$iDifferentialRetention - 1]))
        {
            &log(DEBUG, 'differential expiration based on ' . $stryPath[$iDifferentialRetention - 1]);

            # Get a list of all differential and incremental backups
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(0, 1, 1), 'reverse'))
            {
                &log(DEBUG, "checking ${strPath} for differential expiration");

                # Remove all differential and incremental backups before the oldest valid differential
                if ($strPath lt $stryPath[$iDifferentialRetention - 1])
                {
                    system("rm -rf ${strBackupClusterPath}/${strPath}") == 0
                        or confess &log(ERROR, "unable to delete backup ${strPath}");
                    $oBackupInfo->backupRemove($strPath);

                    &log(INFO, "remove expired diff/incr backup ${strPath}");
                }
            }
        }
    }

    # If no archive retention type is set then exit
    if (!defined($strArchiveRetentionType))
    {
        &log(INFO, 'archive rentention type not set - archive logs will not be expired');
        return;
    }

    # Determine which backup type to use for archive retention (full, differential, incremental)
    if ($strArchiveRetentionType eq BACKUP_TYPE_FULL)
    {
        if (!defined($iArchiveRetention))
        {
            $iArchiveRetention = $iFullRetention;
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 0, 0), 'reverse');
    }
    elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
    {
        if (!defined($iArchiveRetention))
        {
            $iArchiveRetention = $iDifferentialRetention;
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 1, 0), 'reverse');
    }
    elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
    {
        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(1, 1, 1), 'reverse');
    }
    else
    {
        confess &log(ERROR, "unknown archive_retention_type '${strArchiveRetentionType}'");
    }

    # Make sure that iArchiveRetention is set and valid
    if (!defined($iArchiveRetention))
    {
        confess &log(ERROR, 'archive_rentention must be set if archive_retention_type is set');
        return;
    }

    if (!looks_like_number($iArchiveRetention) || $iArchiveRetention < 1)
    {
        confess &log(ERROR, 'archive_rentention must be a number >= 1');
    }

    # if no backups were found then preserve current archive logs - too scary to delete them!
    my $iBackupTotal = scalar @stryPath;

    if ($iBackupTotal == 0)
    {
        return;
    }

    # See if enough backups exist for expiration to start
    my $strArchiveRetentionBackup = $stryPath[$iArchiveRetention - 1];

    if (!defined($strArchiveRetentionBackup))
    {
        if ($strArchiveRetentionType eq BACKUP_TYPE_FULL && scalar @stryPath > 0)
        {
            &log(INFO, 'fewer than required backups for retention, but since archive_retention_type = full using oldest full backup');
            $strArchiveRetentionBackup = $stryPath[scalar @stryPath - 1];
        }

        if (!defined($strArchiveRetentionBackup))
        {
            return;
        }
    }

    # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this backup
    # even though they are also in the pg_xlog directory (since they have been copied more than once).
    &log(INFO, 'archive retention based on backup ' . $strArchiveRetentionBackup);

    my $oManifest = new BackRest::Manifest($oFile->path_get(PATH_BACKUP_CLUSTER) . "/${strArchiveRetentionBackup}/backup.manifest");
    my $strArchiveLast = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START);

    if (!defined($strArchiveLast))
    {
        confess &log(ERROR, "invalid archive location retrieved ${strArchiveRetentionBackup}");
    }

    &log(INFO, 'archive retention starts at ' . $strArchiveLast);

    # Get archive info
    my $oArchive = new BackRest::Archive();
    my $strArchiveId = $oArchive->getCheck($oFile);

    # Remove any archive directories or files that are out of date
    foreach $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, "^[0-F]{16}\$"))
    {
        &log(DEBUG, 'found major archive path ' . $strPath);

        # If less than first 16 characters of current archive file, then remove the directory
        if ($strPath lt substr($strArchiveLast, 0, 16))
        {
            my $strFullPath = $oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

            remove_tree($strFullPath) > 0 or confess &log(ERROR, "unable to remove ${strFullPath}");

            &log(DEBUG, 'remove major archive path ' . $strFullPath);
        }
        # If equals the first 16 characters of the current archive file, then delete individual files instead
        elsif ($strPath eq substr($strArchiveLast, 0, 16))
        {
            my $strSubPath;

            # Look for archive files in the archive directory
            foreach $strSubPath ($oFile->list(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strPath}", "^[0-F]{24}.*\$"))
            {
                # Delete if the first 24 characters less than the current archive file
                if ($strSubPath lt substr($strArchiveLast, 0, 24))
                {
                    unlink($oFile->path_get(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSubPath}"))
                        or confess &log(ERROR, 'unable to remove ' . $strSubPath);
                    &log(DEBUG, 'remove expired archive file ' . $strSubPath);
                }
            }
        }
    }
}

1;
