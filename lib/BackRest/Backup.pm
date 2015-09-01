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
use Thread::Queue;

use lib dirname($0);
use BackRest::Common::Exception;
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Archive;
use BackRest::BackupCommon;
use BackRest::BackupFile;
use BackRest::BackupInfo;
use BackRest::Common::String;
use BackRest::Config;
use BackRest::Db;
use BackRest::File;
use BackRest::Manifest;
use BackRest::Protocol::ThreadGroup;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_BACKUP                                              => 'Backup';

use constant OP_BACKUP_DESTROY                                      => OP_BACKUP . '->DESTROY';
use constant OP_BACKUP_FILE_NOT_IN_MANIFEST                         => OP_BACKUP . '->fileNotInManifest';
use constant OP_BACKUP_NEW                                          => OP_BACKUP . '->new';
use constant OP_BACKUP_PROCESS                                      => OP_BACKUP . '->process';
use constant OP_BACKUP_PROCESS_MANIFEST                             => OP_BACKUP . '->processManifest';
use constant OP_BACKUP_TMP_CLEAN                                    => OP_BACKUP . '->tmpClean';
use constant OP_BACKUP_TYPE_FIND                                    => OP_BACKUP . '->typeFind';

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oFile}
    ) =
        logDebugParam
        (
            OP_BACKUP_NEW, \@_,
            {name => 'oFile', trace => true}
        );

    $self->{oDb} = new BackRest::Db();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_BACKUP_DESTROY
    );

    undef($self->{oFile});
    undef($self->{oDb});


    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# typeFind
#
# Find the last backup depending on the type.
####################################################################################################################################
sub typeFind
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strBackupClusterPath
    ) =
        logDebugParam
        (
            OP_BACKUP_TYPE_FIND, \@_,
            {name => 'strType'},
            {name => 'strBackupClusterPath'}
        );

    my $strLabel;

    if ($strType eq BACKUP_TYPE_INCR)
    {
        $strLabel = ($self->{oFile}->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true, true, true), 'reverse'))[0];
    }

    if (!defined($strLabel) && $strType ne BACKUP_TYPE_FULL)
    {
        $strLabel = ($self->{oFile}->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true), 'reverse'))[0];
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strLabel', value => $strLabel}
    );
}

####################################################################################################################################
# fileNotInManifest
#
# Find all files in a backup path that are not in the supplied manifest.
####################################################################################################################################
sub fileNotInManifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $oManifest,
        $oAbortedManifest
    ) =
        logDebugParam
        (
            OP_BACKUP_FILE_NOT_IN_MANIFEST, \@_,
            {name => 'strPathType', trace => true},
            {name => 'oManifest', trace => true},
            {name => 'oAbortedManifest', trace => true}
        );

    # Build manifest for aborted temp path
    my %oFileHash;
    $self->{oFile}->manifest($strPathType, undef, \%oFileHash);

    # Get compress flag
    my $bCompressed = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);

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
            # Else if a file
            elsif ($cType eq 'f')
            {
                # If the original backup was compressed the remove the extension before checking the manifest
                if ($bCompressed)
                {
                    $strPath = substr($strPath, 0, length($strPath) - 3);
                }

                # To be preserved the file must exist in the new manifest and not be a reference to a previous backup
                if ($oManifest->test("${strSection}:file", $strPath) &&
                    !$oManifest->test("${strSection}:file", $strPath, MANIFEST_SUBKEY_REFERENCE))
                {
                    # To be preserved the checksum must be defined
                    my $strChecksum = $oAbortedManifest->get("${strSection}:file", $strPath, MANIFEST_SUBKEY_CHECKSUM, false);

                    # The timestamp should also match and the size if the file is not compressed.  If the file is compressed it's
                    # not worth extracting the size - it will be hashed later to verify its authenticity.
                    if (defined($strChecksum) &&
                        ($bCompressed || ($oManifest->numericGet("${strSection}:file", $strPath, MANIFEST_SUBKEY_SIZE) ==
                            $oFileHash{name}{$strName}{size})) &&
                        $oManifest->numericGet("${strSection}:file", $strPath, MANIFEST_SUBKEY_TIMESTAMP) ==
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFile', value => \@stryFile}
    );
}

####################################################################################################################################
# tmpClean
#
# Cleans the temp directory from a previous failed backup so it can be reused
####################################################################################################################################
sub tmpClean
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest,
        $oAbortedManifest
    ) =
        logDebugParam
    (
        OP_BACKUP_TMP_CLEAN, \@_,
        {name => 'oManifest', trace => true},
        {name => 'oAbortedManifest', trace => true}
    );

    &log(INFO, 'clean backup temp path: ' . $self->{oFile}->pathGet(PATH_BACKUP_TMP));

    # Remove the pg_xlog directory since it contains nothing useful for the new backup
    if (-e $self->{oFile}->pathGet(PATH_BACKUP_TMP, 'base/pg_xlog'))
    {
        remove_tree($self->{oFile}->pathGet(PATH_BACKUP_TMP, 'base/pg_xlog')) or confess &log(ERROR, 'unable to delete tmp pg_xlog path');
    }

    # Remove the pg_tblspc directory since it is trivial to rebuild, but hard to compare
    if (-e $self->{oFile}->pathGet(PATH_BACKUP_TMP, 'base/pg_tblspc'))
    {
        remove_tree($self->{oFile}->pathGet(PATH_BACKUP_TMP, 'base/pg_tblspc')) or confess &log(ERROR, 'unable to delete tmp pg_tblspc path');
    }

    # Get the list of files that should be deleted from temp
    my @stryFile = $self->fileNotInManifest(PATH_BACKUP_TMP, $oManifest, $oAbortedManifest);

    foreach my $strFile (sort {$b cmp $a} @stryFile)
    {
        my $strDelete = $self->{oFile}->pathGet(PATH_BACKUP_TMP, $strFile);

        # If a path then delete it, all the files should have already been deleted since we are going in reverse order
        if (-d $strDelete)
        {
            logDebugMisc($strOperation, "remove path ${strDelete}");

            rmdir($strDelete)
                or confess &log(ERROR, "unable to delete path ${strDelete}, is it empty?", ERROR_PATH_REMOVE);
        }
        # Else delete a file
        else
        {
            logDebugMisc($strOperation, "remove file ${strDelete}");

            unlink($strDelete)
                or confess &log(ERROR, "unable to delete file ${strDelete}", ERROR_FILE_REMOVE);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# processManifest
#
# Process the file level backup.  Uses the information in the manifest to determine which files need to be copied.  Directories
# and tablespace links are only created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub processManifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $bCompress,
        $bHardLink,
        $oBackupManifest                            # Manifest for the current backup
    ) =
        logDebugParam
    (
        OP_BACKUP_PROCESS_MANIFEST, \@_,
        {name => 'strType'},
        {name => 'bCompress'},
        {name => 'bHardLink'},
        {name => 'oBackupManifest'},
    );

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
            $self->{oFile}->linkCreate(PATH_BACKUP_TMP, $strBackupDestinationPath,
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
                        $self->{oFile}->pathCreate(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}");
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
                        $self->{oFile}->linkCreate(PATH_BACKUP_ABSOLUTE,
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
                    logDebugMisc($strOperation, "hardlink ${strBackupSourceFile} to ${strReference}");

                    $self->{oFile}->linkCreate(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                        PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true, false, true);
                }
                else
                {
                    logDebugMisc($strOperation, "reference ${strBackupSourceFile} to ${strReference}");
                }

                $bProcess = false;
            }

            if ($bProcess)
            {
                my $lFileSize = $oBackupManifest->numericGet($strSectionFile, $strFile, MANIFEST_SUBKEY_SIZE);

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
                    $oBackupManifest->numericGet($strSectionFile, $strFile, MANIFEST_SUBKEY_TIMESTAMP, false);
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
    }
    else
    {
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
            if (optionGet(OPTION_THREAD_MAX) > 1)
            {
                $oyBackupQueue[@oyBackupQueue] = Thread::Queue->new();
            }

            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oFileCopyMap{$strPathKey}})))
            {
                my $oFileCopy = $oFileCopyMap{$strPathKey}{$strFileKey};

                if (optionGet(OPTION_THREAD_MAX) > 1)
                {
                    $oyBackupQueue[@oyBackupQueue - 1]->enqueue($oFileCopy);
                }
                else
                {
                    # Backup the file
                    ($bCopied, $lSizeCurrent, $lCopySize, $strCopyChecksum) =
                        backupFile($self->{oFile}, $$oFileCopy{db_file}, $$oFileCopy{backup_file}, $bCompress,
                                   $$oFileCopy{checksum}, $$oFileCopy{modification_time},
                                   $$oFileCopy{size}, $lSizeTotal, $lSizeCurrent);

                    $lManifestSaveCurrent = backupManifestUpdate($oBackupManifest, $$oFileCopy{file_section}, $$oFileCopy{file},
                                                                 $bCopied, $lCopySize, $strCopyChecksum, $lManifestSaveSize,
                                                                 $lManifestSaveCurrent);
                }
            }
        }

        # If multi-threaded then create threads to copy files
        if (optionGet(OPTION_THREAD_MAX) > 1)
        {
            for (my $iThreadIdx = 0; $iThreadIdx < optionGet(OPTION_THREAD_MAX); $iThreadIdx++)
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
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSizeTotal', value => $lSizeTotal}
    );
}

####################################################################################################################################
# process
#
# Process the database backup.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_BACKUP_PROCESS
    );

    # Record timestamp start
    my $lTimestampStart = time();

    # Store local type, compress, and hardlink options since they can be modified by the process
    my $strType = optionGet(OPTION_TYPE);
    my $bCompress = optionGet(OPTION_COMPRESS);
    my $bHardLink = optionGet(OPTION_HARDLINK);

    # Not supporting remote backup hosts yet
    if ($self->{oFile}->isRemote(PATH_BACKUP))
    {
        confess &log(ERROR, 'remote backup host not currently supported');
    }

    # Create the cluster backup path
    $self->{oFile}->pathCreate(PATH_BACKUP_CLUSTER, undef, undef, true);

    # Load or build backup.info
    my $oBackupInfo = new BackRest::BackupInfo($self->{oFile}->pathGet(PATH_BACKUP_CLUSTER));

    # Build backup tmp and config
    my $strBackupTmpPath = $self->{oFile}->pathGet(PATH_BACKUP_TMP);
    my $strBackupConfFile = $self->{oFile}->pathGet(PATH_BACKUP_TMP, 'backup.manifest');

    # Declare the backup manifest
    my $oBackupManifest = new BackRest::Manifest($strBackupConfFile, false);

    # Find the previous backup based on the type
    my $oLastManifest = undef;

    my $strBackupLastPath = $self->typeFind($strType, $self->{oFile}->pathGet(PATH_BACKUP_CLUSTER));

    if (defined($strBackupLastPath))
    {
        $oLastManifest = new BackRest::Manifest($self->{oFile}->pathGet(PATH_BACKUP_CLUSTER) . "/${strBackupLastPath}/backup.manifest");

        &log(INFO, 'last backup label = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) .
                   ', version = ' . $oLastManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));

        # If this is incr or diff warn if certain options have changed
        if ($strType ne BACKUP_TYPE_FULL)
        {
            my $strKey;

            # Warn if compress option changed
            if (!$oLastManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress))
            {
                &log(WARN, "${strType} backup cannot alter compress option to '" . boolFormat($bCompress) .
                           "', reset to value in ${strBackupLastPath}");
                $bCompress = $oLastManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
            }

            # Warn if hardlink option changed
            if (!$oLastManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink))
            {
                &log(WARN, "${strType} backup cannot alter hardlink option to '" . boolFormat($bHardLink) .
                           "', reset to value in ${strBackupLastPath}");
                $bHardLink = $oLastManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
            }
        }
    }
    else
    {
        if ($strType eq BACKUP_TYPE_DIFF || $strType eq BACKUP_TYPE_INCR)
        {
            &log(WARN, "no prior backup exists, ${strType} backup has been changed to full");
        }

        $strType = BACKUP_TYPE_FULL;
    }

    # Backup settings
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestampStart);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_START_STOP, undef, !optionGet(OPTION_NO_START_STOP));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                              optionGet(OPTION_NO_START_STOP) || optionGet(OPTION_BACKUP_ARCHIVE_COPY));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                              optionGet(OPTION_NO_START_STOP) || optionGet(OPTION_BACKUP_ARCHIVE_CHECK));

    # Database info
    my ($fDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) =
        $self->{oDb}->info($self->{oFile}, optionGet(OPTION_DB_PATH));

    $oBackupInfo->check($fDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);

    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, $fDbVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iControlVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iCatalogVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $ullDbSysId);

    # Start backup (unless no-start-stop is set)
    my $strArchiveStart;
    my $oTablespaceMap;

    # Don't start the backup but do check if PostgreSQL is running
    if (optionGet(OPTION_NO_START_STOP))
    {
        if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, optionGet(OPTION_DB_PATH) . '/' . FILE_POSTMASTER_PID))
        {
            if (optionGet(OPTION_FORCE))
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
    # Else start the backup normally
    else
    {
        my $strTimestampDbStart;

        # Start the backup
        ($strArchiveStart, $strTimestampDbStart) =
            $self->{oDb}->backupStart($self->{oFile}, optionGet(OPTION_DB_PATH), BACKREST_EXE . ' backup started ' .
                                      timestampFormat(undef, $lTimestampStart), optionGet(OPTION_START_FAST));

        # Record the archive start location
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        &log(INFO, "archive start: ${strArchiveStart}");

        # Build the backup manifest
        $oTablespaceMap = optionGet(OPTION_NO_START_STOP) ? undef : $self->{oDb}->tablespaceMapGet();
    }

    # Buid the manifest
    $oBackupManifest->build($self->{oFile}, optionGet(OPTION_DB_PATH), $oLastManifest, optionGet(OPTION_NO_START_STOP),
                            $oTablespaceMap);
    &log(TEST, TEST_MANIFEST_BUILD);

    # Check if an aborted backup exists for this stanza
    if (-e $strBackupTmpPath)
    {
        my $bUsable = false;
        my $strReason = "resume is disabled";
        my $oAbortedManifest;

        # Attempt to read the manifest file in the aborted backup to seeif it can be used.  If any error at all occurs then the
        # backup will be considered unusable and a resume will not be attempted.
        if (optionGet(OPTION_RESUME))
        {
            $strReason = "unable to read ${strBackupTmpPath}/backup.manifest";

            eval
            {
                # Load the aborted manifest
                $oAbortedManifest = new BackRest::Manifest("${strBackupTmpPath}/backup.manifest");

                # Key and values that do not match
                my $strKey;
                my $strValueNew;
                my $strValueAborted;

                # Check version
                if ($oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION) ne
                    $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION))
                {
                    $strKey =  INI_KEY_VERSION;
                    $strValueNew = $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);
                    $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);
                }
                # Check format
                elsif ($oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT) ne
                       $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT))
                {
                    $strKey =  INI_KEY_FORMAT;
                    $strValueNew = $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT);
                    $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT);
                }
                # Check backup type
                elsif ($oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) ne
                       $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE))
                {
                    $strKey =  MANIFEST_KEY_TYPE;
                    $strValueNew = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
                    $strValueAborted = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
                }
                # Check prior label
                elsif ($oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>') ne
                       $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>'))
                {
                    $strKey =  MANIFEST_KEY_PRIOR;
                    $strValueNew = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
                    $strValueAborted = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
                }
                # Check compression
                elsif ($oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS) ne
                       $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS))
                {
                    $strKey = MANIFEST_KEY_COMPRESS;
                    $strValueNew = $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
                    $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
                }
                # Check hardlink
                elsif ($oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK) ne
                       $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK))
                {
                    $strKey = MANIFEST_KEY_HARDLINK;
                    $strValueNew = $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
                    $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
                }

                # If key is defined then something didn't match
                if (defined($strKey))
                {
                    $strReason = "new ${strKey} '${strValueNew}' does not match aborted ${strKey} '${strValueAborted}'";
                }
                # Else the backup can be resumed
                else
                {
                    $bUsable = true;
                }
            };
        }

        # If the aborted backup is usable then clean it
        if ($bUsable)
        {
            &log(WARN, 'aborted backup of same type exists, will be cleaned to remove invalid files and resumed');
            &log(TEST, TEST_BACKUP_RESUME);

            # Clean the old backup tmp path
            $self->tmpClean($oBackupManifest, $oAbortedManifest);
        }
        # Else remove it
        else
        {
            &log(WARN, "aborted backup exists, but cannot be resumed (${strReason}) - will be dropped and recreated");
            &log(TEST, TEST_BACKUP_NORESUME);

            remove_tree($self->{oFile}->pathGet(PATH_BACKUP_TMP))
                or confess &log(ERROR, "unable to delete tmp path: ${strBackupTmpPath}");
            $self->{oFile}->pathCreate(PATH_BACKUP_TMP);
        }
    }
    # Else create the backup tmp path
    else
    {
        logDebugMisc($strOperation, "create temp backup path ${strBackupTmpPath}");
        $self->{oFile}->pathCreate(PATH_BACKUP_TMP);
    }

    # Save the backup manifest
    $oBackupManifest->save();

    # Perform the backup
    my $lBackupSizeTotal = $self->processManifest($strType, $bCompress, $bHardLink, $oBackupManifest);
    &log(INFO, "${strType} backup size = " . fileSizeFormat($lBackupSizeTotal));

    # Stop backup (unless no-start-stop is set)
    my $strArchiveStop;

    if (!optionGet(OPTION_NO_START_STOP))
    {
        my $strTimestampDbStop;
        ($strArchiveStop, $strTimestampDbStop) = $self->{oDb}->backupStop();

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
        logDebugMisc($strOperation, "retrieve archive logs ${strArchiveStart}:${strArchiveStop}");
        my $oArchive = new BackRest::Archive();
        my $strArchiveId = $oArchive->getCheck($self->{oFile});
        my @stryArchive = $oArchive->range($strArchiveStart, $strArchiveStop, $fDbVersion < 9.3);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchiveFile = $oArchive->walFileName($self->{oFile}, $strArchiveId, $strArchive, 600);

            if (optionGet(OPTION_BACKUP_ARCHIVE_COPY))
            {
                logDebugMisc($strOperation, "archive: ${strArchive} (${strArchiveFile})");

                # Copy the log file from the archive repo to the backup
                my $strDestinationFile = "base/pg_xlog/${strArchive}" . ($bCompress ? ".$self->{oFile}->{strCompressExtension}" : '');
                my $bArchiveCompressed = $strArchiveFile =~ "^.*\.$self->{oFile}->{strCompressExtension}\$";

                my ($bCopyResult, $strCopyChecksum, $lCopySize) =
                    $self->{oFile}->copy(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strArchiveFile}",
                                 PATH_BACKUP_TMP, $strDestinationFile,
                                 $bArchiveCompressed, $bCompress,
                                 undef, $lModificationTime, undef, true);

                # Add the archive file to the manifest so it can be part of the restore and checked in validation
                my $strPathSection = 'base:path';
                my $strPathLog = 'pg_xlog';
                my $strFileSection = 'base:file';
                my $strFileLog = "pg_xlog/${strArchive}";

                # Compare the checksum against the one already in the archive log name
                if ($strArchiveFile !~ "^${strArchive}-${strCopyChecksum}(\\.$self->{oFile}->{strCompressExtension}){0,1}\$")
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
    my $strBackupLabel = backupLabelFormat($strType, $strBackupLastPath, $lTimestampStop);

    # Record timestamp stop in the config
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestampStop + 0);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);

    # Save the backup manifest final time
    $oBackupManifest->save();

    &log(INFO, "new backup label = ${strBackupLabel}");

    # Rename the backup tmp path to complete the backup
    logDebugMisc($strOperation, "move ${strBackupTmpPath} to " . $self->{oFile}->pathGet(PATH_BACKUP_CLUSTER, $strBackupLabel));
    $self->{oFile}->move(PATH_BACKUP_TMP, undef, PATH_BACKUP_CLUSTER, $strBackupLabel);

    # Create a link to the most recent backup
    $self->{oFile}->remove(PATH_BACKUP_CLUSTER, "latest");
    $self->{oFile}->linkCreate(PATH_BACKUP_CLUSTER, $strBackupLabel, PATH_BACKUP_CLUSTER, "latest", undef, true);

    # Save backup info
    $oBackupInfo->add($self->{oFile}, $oBackupManifest);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

1;
