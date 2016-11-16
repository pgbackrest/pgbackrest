####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package pgBackRest::Backup;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
use Fcntl 'SEEK_CUR';
use File::Basename;
use File::Path qw(remove_tree);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Exit;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Archive;
use pgBackRest::ArchiveCommon;
use pgBackRest::BackupCommon;
use pgBackRest::BackupFile;
use pgBackRest::BackupInfo;
use pgBackRest::BackupProcess;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;
use pgBackRest::Version;

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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
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
        $oFileLocal,
        $strPathType,
        $oManifest,
        $oAbortedManifest
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->fileNotInManifest', \@_,
            {name => 'oFileLocal', trace => true},
            {name => 'strPathType', trace => true},
            {name => 'oManifest', trace => true},
            {name => 'oAbortedManifest', trace => true}
        );

    # Build manifest for aborted temp path
    my %oFileHash;
    $oFileLocal->manifest($strPathType, undef, \%oFileHash);

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

        # Get the file type (all links will be deleted since they are easy to recreate)
        my $cType = $oFileHash{name}{$strName}{type};

        # If a directory check if it exists in the new manifest
        if ($cType eq 'd')
        {
            if ($oManifest->test(MANIFEST_SECTION_TARGET_PATH, $strName))
            {
                next;
            }
        }
        # Else if a file
        elsif ($cType eq 'f')
        {
            # If the original backup was compressed the remove the extension before checking the manifest
            my $strFile = $strName;

            if ($bCompressed)
            {
                $strFile = substr($strFile, 0, length($strFile) - 3);
            }

            # To be preserved the file must exist in the new manifest and not be a reference to a previous backup
            if ($oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFile) &&
                !$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_REFERENCE))
            {
                # To be preserved the checksum must be defined
                my $strChecksum = $oAbortedManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, false);

                # The timestamp should also match and the size if the file is not compressed.  If the file is compressed it's
                # not worth extracting the size - it will be hashed later to verify its authenticity.
                if (defined($strChecksum) &&
                    ($bCompressed || ($oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE) ==
                        $oFileHash{name}{$strName}{size})) &&
                    $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_TIMESTAMP) ==
                        $oFileHash{name}{$strName}{modification_time})
                {
                    $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
                    next;
                }
            }
        }

        # Push the file/path/link to be deleted into the result array
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
        $oFileLocal,
        $oManifest,
        $oAbortedManifest
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->tmpClean', \@_,
        {name => 'oFileLocal', trace => true},
        {name => 'oManifest', trace => true},
        {name => 'oAbortedManifest', trace => true}
    );

    &log(DETAIL, 'clean backup temp path: ' . $oFileLocal->pathGet(PATH_BACKUP_TMP));

    # Get the list of files that should be deleted from temp
    my @stryFile = $self->fileNotInManifest($oFileLocal, PATH_BACKUP_TMP, $oManifest, $oAbortedManifest);

    foreach my $strFile (sort {$b cmp $a} @stryFile)
    {
        my $strDelete = $oFileLocal->pathGet(PATH_BACKUP_TMP, $strFile);

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
            fileRemove($strDelete);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $oFileMaster,
        $strDbMasterPath,
        $strDbCopyPath,
        $strType,
        $strDbVersion,
        $bCompress,
        $bHardLink,
        $oBackupManifest                            # Manifest for the current backup
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->processManifest', \@_,
        {name => 'oFileMaster'},
        {name => 'strDbMasterPath'},
        {name => 'strDbCopyPath'},
        {name => 'strType'},
        {name => 'strDbVersion'},
        {name => 'bCompress'},
        {name => 'bHardLink'},
        {name => 'oBackupManifest'},
    );

    # Start backup test point
    &log(TEST, TEST_BACKUP_START);

    # Get the master protocol for keep-alive
    my $oProtocolMaster = protocolGet(DB, $self->{iMasterRemoteIdx});
    $oProtocolMaster->noOp();

    # Initialize the backup process
    my $oBackupProcess = new pgBackRest::BackupProcess();

    if ($self->{iCopyRemoteIdx} != $self->{iMasterRemoteIdx})
    {
        $oBackupProcess->hostAdd($self->{iMasterRemoteIdx}, 1);
    }

    $oBackupProcess->hostAdd($self->{iCopyRemoteIdx}, optionGet(OPTION_PROCESS_MAX));

    # Variables used for parallel copy
    my $hFileControl = undef;
    my $lFileTotal = 0;
    my $lSizeTotal = 0;

    # If this is a full backup or hard-linked then create all paths
    if ($bHardLink || $strType eq BACKUP_TYPE_FULL)
    {
        foreach my $strPath ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_PATH))
        {
            $oFileMaster->pathCreate(PATH_BACKUP_TMP, $strPath);
        }
    }

    # Iterate all files in the manifest
    foreach my $strFile (
        sort {sprintf("%016d-${b}", $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $b, MANIFEST_SUBKEY_SIZE)) cmp
              sprintf("%016d-${a}", $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $a, MANIFEST_SUBKEY_SIZE))}
        ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_FILE, 'none')))
    {
        # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
        # However, if hard-linking is turned on the link will need to be created
        my $bProcess = true;
        my $strReference = $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_REFERENCE, false);

        if (defined($strReference))
        {
            # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
            if ($bHardLink)
            {
                logDebugMisc($strOperation, "hardlink ${strFile} to ${strReference}");

                $oFileMaster->linkCreate(
                    PATH_BACKUP_CLUSTER, "${strReference}/${strFile}", PATH_BACKUP_TMP, "${strFile}", true, false, true);
            }
            # Else log the reference
            else
            {
                logDebugMisc($strOperation, "reference ${strFile} to ${strReference}");
            }

            # This file will not need to be copied
            $bProcess = false;
        }

        # If the file must be copied
        if ($bProcess)
        {
            # By default put everything into a single queue
            my $strQueueKey = MANIFEST_TARGET_PGDATA;

            # If the file belongs in a tablespace then put in a tablespace-specific queue
            if (index($strFile, DB_PATH_PGTBLSPC . '/') == 0)
            {
                $strQueueKey = DB_PATH_PGTBLSPC . '/' . (split('\/', $strFile))[1];
            }

            # Create the file hash
            my $hFile = {bIgnoreMissing => undef};
            my $iHostConfigIdx = $self->{iCopyRemoteIdx};

            # Certain files must be copied from the master
            if ($oBackupManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_MASTER))
            {
                $hFile->{db_file} = $oBackupManifest->dbPathGet($strDbMasterPath, $strFile);
                $iHostConfigIdx = $self->{iMasterRemoteIdx};
            }
            # Else continue normally
            else
            {
                $hFile->{db_file} = $oBackupManifest->dbPathGet($strDbCopyPath, $strFile);
            }

            # Make sure that pg_data/PG_VERSION does go away as a sanity check
            if ($strFile eq MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGVERSION)
            {
                $hFile->{bIgnoreMissing} = false;
            }

            $hFile->{queue} = $strQueueKey;
            $hFile->{repo_file} = $strFile;
            $hFile->{size} =
                $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE);
            $hFile->{modification_time} =
                $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_TIMESTAMP, false);
            $hFile->{checksum} =
                $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, false);

            # Size and checksum will be removed and then verified later as a sanity check
            $oBackupManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE);
            $oBackupManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM);

            # Increment file total and size
            $lFileTotal++;
            $lSizeTotal += $hFile->{size};

            # pg_control will be copied last so assign it to a special variable
            if ($strFile eq MANIFEST_FILE_PGCONTROL)
            {
                $hFileControl = $hFile;
            }
            # Else queue for parallel backup
            else
            {
                $oBackupProcess->queueBackup(
                    $iHostConfigIdx, $hFile->{queue}, $hFile->{repo_file}, $hFile->{db_file}, $hFile->{repo_file},
                    $bCompress, $hFile->{modification_time}, $hFile->{size}, $hFile->{checksum}, $hFile->{bIgnoreMissing});
            }
        }
    }

    # pg_control should always be in the backup (unless this is an offline backup)
    if (!defined($hFileControl) && optionGet(OPTION_ONLINE))
    {
        confess &log(ERROR, DB_FILE_PGCONTROL . " must be present in all online backups\n" .
                     'HINT: is something wrong with the clock or filesystem timestamps?', ERROR_FILE_MISSING);
    }

    # If there are no files to backup then we'll exit with an error unless in test mode.  The other way this could happen is if
    # the database is down and backup is called with --no-online twice in a row.
    if ($lFileTotal == 0)
    {
        if (!optionGet(OPTION_TEST))
        {
            confess &log(ERROR, "no files have changed since the last backup - this seems unlikely", ERROR_FILE_MISSING);
        }
    }
    else
    {
        # Running total of bytes copied
        my $lSizeCurrent = 0;

        # Determine how often the manifest will be saved
        my $lManifestSaveCurrent = 0;
        my $lManifestSaveSize = int($lSizeTotal / 100);

        if (optionSource(OPTION_MANIFEST_SAVE_THRESHOLD) ne SOURCE_DEFAULT ||
            $lManifestSaveSize < optionGet(OPTION_MANIFEST_SAVE_THRESHOLD))
        {
            $lManifestSaveSize = optionGet(OPTION_MANIFEST_SAVE_THRESHOLD);
        }

        # Run the backup jobs and process results
        while (my $hyResult = $oBackupProcess->process())
        {
            foreach my $hResult (@{$hyResult})
            {
                my $hFile = $hResult->{hPayload};

                ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
                    $oBackupManifest, optionGet(optionIndex(OPTION_DB_HOST, $hResult->{iHostConfigIdx}), false),
                    $hResult->{iProcessId}, $$hFile{strRepoFile}, $$hFile{strDbFile}, $$hResult{iCopyResult}, $$hFile{lSize},
                    $$hResult{lCopySize}, $$hResult{lRepoSize}, $lSizeTotal, $lSizeCurrent, $$hFile{strChecksum},
                    $$hResult{strCopyChecksum}, $lManifestSaveSize, $lManifestSaveCurrent);
            }

            # A keep-alive is required here because if there are a large number of resumed files that need to be checksummed
            # then the remote might timeout while waiting for a command.
            $oProtocolMaster->keepAlive();
        }

        # Copy pg_control last - this is required for backups taken during recovery
        if (defined($hFileControl))
        {
            my ($iCopyResult, $lCopySize, $lRepoSize, $strCopyChecksum) = backupFile(
                $oFileMaster, $$hFileControl{db_file}, $$hFileControl{repo_file}, $bCompress, $$hFileControl{checksum},
                $$hFileControl{modification_time}, $$hFileControl{size}, false);

            backupManifestUpdate(
                $oBackupManifest, optionGet(optionIndex(OPTION_DB_HOST, $self->{iMasterRemoteIdx}), false), undef,
                $$hFileControl{repo_file}, $$hFileControl{db_file}, $iCopyResult, $$hFileControl{size}, $lCopySize, $lRepoSize,
                $lSizeTotal, $lSizeCurrent, $$hFileControl{checksum}, $strCopyChecksum, $lManifestSaveSize, $lManifestSaveCurrent);
        }
    }

    # Validate the manifest
    $oBackupManifest->validate();

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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Record timestamp start
    my $lTimestampStart = time();

    # Initialize the local file object
    my $oFileLocal = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(NONE)
    );

    # Store local type, compress, and hardlink options since they can be modified by the process
    my $strType = optionGet(OPTION_TYPE);
    my $bCompress = optionGet(OPTION_COMPRESS);
    my $bHardLink = optionGet(OPTION_HARDLINK);

    # Create the cluster backup and history path
    $oFileLocal->pathCreate(PATH_BACKUP_CLUSTER, PATH_BACKUP_HISTORY, undef, true, true);

    # Load or build backup.info
    my $oBackupInfo = new pgBackRest::BackupInfo($oFileLocal->pathGet(PATH_BACKUP_CLUSTER));

    # Build backup tmp and config
    my $strBackupTmpPath = $oFileLocal->pathGet(PATH_BACKUP_TMP);
    my $strBackupConfFile = $oFileLocal->pathGet(PATH_BACKUP_TMP, 'backup.manifest');

    # Declare the backup manifest
    my $oBackupManifest = new pgBackRest::Manifest($strBackupConfFile, false);

    # Find the previous backup based on the type
    my $oLastManifest;
    my $strBackupLastPath;

    if ($strType ne BACKUP_TYPE_FULL)
    {
        $strBackupLastPath = $oBackupInfo->last($strType eq BACKUP_TYPE_DIFF ? BACKUP_TYPE_FULL : BACKUP_TYPE_INCR);

        if (defined($strBackupLastPath))
        {
            $oLastManifest = new pgBackRest::Manifest(
                $oFileLocal->pathGet(PATH_BACKUP_CLUSTER, "${strBackupLastPath}/" . FILE_MANIFEST));

            &log(INFO, 'last backup label = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) .
                       ', version = ' . $oLastManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));

            # If this is incr or diff warn if certain options have changed
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
        else
        {
            &log(WARN, "no prior backup exists, ${strType} backup has been changed to full");
            $strType = BACKUP_TYPE_FULL;
        }
    }

    # Backup settings
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestampStart);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef, optionGet(OPTION_BACKUP_STANDBY));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, optionGet(OPTION_ONLINE));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                              !optionGet(OPTION_ONLINE) ||
                              (optionGet(OPTION_BACKUP_ARCHIVE_CHECK) && optionGet(OPTION_BACKUP_ARCHIVE_COPY)));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                              !optionGet(OPTION_ONLINE) || optionGet(OPTION_BACKUP_ARCHIVE_CHECK));

    # Initialize database objects
    my $oDbMaster = undef;
    my $oDbStandby = undef;

#CSHANG it might be nice to have this through the IF statement after "# If master db is not already defined then set to default" so that we don't have to raise errors all over the place and so that we always return a masterDB. BUT for stanza create, we shouldn't force them to bring up postgres before doing a stanza create - obviously that results in an error from the archive push command (which ALSO causes a problem with the check command if they try to perform that before the stanza create).

    ($oDbMaster, $self->{iMasterRemoteIdx}, $oDbStandby, $self->{iCopyRemoteIdx}) = dbObjectGet();

    # If remote copy was not explicitly set then set it equal to master
    if (!defined($self->{iCopyRemoteIdx}))
    {
        $self->{iCopyRemoteIdx} = $self->{iMasterRemoteIdx};
    }

    # Initialize the master file object
    my $oFileMaster = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(DB, $self->{iMasterRemoteIdx})
    );

    # Determine the database paths
    my $strDbMasterPath = optionGet(optionIndex(OPTION_DB_PATH, $self->{iMasterRemoteIdx}));
    my $strDbCopyPath = optionGet(optionIndex(OPTION_DB_PATH, $self->{iCopyRemoteIdx}));

    # Database info
    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDbMaster->info();

    my $iDbHistoryId = $oBackupInfo->check($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);

    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef, $iDbHistoryId);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, $strDbVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iControlVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iCatalogVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $ullDbSysId);

    # Backup from standby can only be used on PostgreSQL >= 9.1
    if (optionGet(OPTION_ONLINE) && optionGet(OPTION_BACKUP_STANDBY) && $strDbVersion < PG_VERSION_BACKUP_STANDBY)
    {
        confess &log(
            ERROR,
            'option \'' . OPTION_BACKUP_STANDBY . '\' not valid for PostgreSQL < ' . PG_VERSION_BACKUP_STANDBY, ERROR_CONFIG);
    }

    # Start backup (unless --no-online is set)
    my $strArchiveStart = undef;
    my $strLsnStart = undef;
    my $oTablespaceMap = undef;
	my $oDatabaseMap = undef;

    # Don't start the backup but do check if PostgreSQL is running
    if (!optionGet(OPTION_ONLINE))
    {
        if ($oFileMaster->exists(PATH_DB_ABSOLUTE, $strDbMasterPath . '/' . DB_FILE_POSTMASTERPID))
        {
            if (optionGet(OPTION_FORCE))
            {
                &log(WARN, '--no-online passed and ' . DB_FILE_POSTMASTERPID . ' exists but --force was passed so backup will ' .
                           'continue though it looks like the postmaster is running and the backup will probably not be ' .
                           'consistent');
            }
            else
            {
                confess &log(ERROR, '--no-online passed but ' . DB_FILE_POSTMASTERPID . ' exists - looks like the postmaster is ' .
                            'running. Shutdown the postmaster and try again, or use --force.', ERROR_POSTMASTER_RUNNING);
            }
        }
    }
    # Else start the backup normally
    else
    {
        # Start the backup
        ($strArchiveStart, $strLsnStart) =
            $oDbMaster->backupStart(
                BACKREST_NAME . ' backup started at ' . timestampFormat(undef, $lTimestampStart), optionGet(OPTION_START_FAST));

        # Record the archive start location
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_START, undef, $strLsnStart);
        &log(INFO, "backup start archive = ${strArchiveStart}, lsn = ${strLsnStart}");

        # Get tablespace map
        $oTablespaceMap = $oDbMaster->tablespaceMapGet();

        # Get database map
        $oDatabaseMap = $oDbMaster->databaseMapGet();

        # Wait for replay on the standby to catch up
        if (optionGet(OPTION_BACKUP_STANDBY))
        {
            my ($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId) = $oDbStandby->info();
            $oBackupInfo->check($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId);

            $oDbStandby->configValidate();

            &log(INFO, "wait for replay on the standby to reach ${strLsnStart}");

            my ($strReplayedLSN, $strCheckpointLSN) = $oDbStandby->replayWait($strLsnStart);

            &log(
                INFO,
                "replay on the standby reached ${strReplayedLSN}" .
                    (defined($strCheckpointLSN) ? ", checkpoint ${strCheckpointLSN}" : ''));

            # The standby db object won't be used anymore so undef it to catch any subsequent references
            undef($oDbStandby);
            protocolDestroy(DB, $self->{iCopyRemoteIdx}, true);
        }
    }

    # Build the manifest
    $oBackupManifest->build($oFileMaster, $strDbVersion, $strDbMasterPath, $oLastManifest, optionGet(OPTION_ONLINE),
                            $oTablespaceMap, $oDatabaseMap);
    &log(TEST, TEST_MANIFEST_BUILD);

    # Check if an aborted backup exists for this stanza
    if (-e $strBackupTmpPath)
    {
        my $bUsable;
        my $strReason = "resume is disabled";
        my $oAbortedManifest;

        # Attempt to read the manifest file in the aborted backup to see if it can be used.  If any error at all occurs then the
        # backup will be considered unusable and a resume will not be attempted.
        if (optionGet(OPTION_RESUME))
        {
            $strReason = "unable to read ${strBackupTmpPath}/backup.manifest";

            eval
            {
                # Load the aborted manifest
                $oAbortedManifest = new pgBackRest::Manifest("${strBackupTmpPath}/backup.manifest");

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

                return true;
            }
            or do
            {
                $bUsable = false;
            }
        }

        # If the aborted backup is usable then clean it
        if ($bUsable)
        {
            &log(WARN, 'aborted backup of same type exists, will be cleaned to remove invalid files and resumed');
            &log(TEST, TEST_BACKUP_RESUME);

            # Clean the old backup tmp path
            $self->tmpClean($oFileLocal, $oBackupManifest, $oAbortedManifest);
        }
        # Else remove it
        else
        {
            &log(WARN, "aborted backup exists, but cannot be resumed (${strReason}) - will be dropped and recreated");
            &log(TEST, TEST_BACKUP_NORESUME);

            remove_tree($oFileLocal->pathGet(PATH_BACKUP_TMP))
                or confess &log(ERROR, "unable to delete tmp path: ${strBackupTmpPath}");
            $oFileLocal->pathCreate(PATH_BACKUP_TMP, undef, undef, false, true);
        }
    }
    # Else create the backup tmp path
    else
    {
        logDebugMisc($strOperation, "create temp backup path ${strBackupTmpPath}");
        $oFileLocal->pathCreate(PATH_BACKUP_TMP, undef, undef, false, true);
    }

    # Save the backup manifest
    $oBackupManifest->save();

    # Perform the backup
    my $lBackupSizeTotal =
        $self->processManifest(
            $oFileMaster, $strDbMasterPath, $strDbCopyPath, $strType, $strDbVersion, $bCompress, $bHardLink, $oBackupManifest);
    &log(INFO, "${strType} backup size = " . fileSizeFormat($lBackupSizeTotal));

    # Master file object no longer needed
    undef($oFileMaster);

    # Stop backup (unless --no-online is set)
    my $strArchiveStop = undef;
    my $strLsnStop = undef;

    if (optionGet(OPTION_ONLINE))
    {
        ($strArchiveStop, $strLsnStop, my $strTimestampDbStop, my $oFileHash) = $oDbMaster->backupStop();

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_STOP, undef, $strLsnStop);
        &log(INFO, "backup stop archive = ${strArchiveStop}, lsn = ${strLsnStop}");

        # Write out files returned from stop backup
        foreach my $strFile (sort(keys(%{$oFileHash})))
        {
            # Only save the file if it has content
            if (defined($$oFileHash{$strFile}))
            {
                my $strFileName = $oFileLocal->pathGet(PATH_BACKUP_TMP, $strFile);

                # Write content out to a file
                fileStringWrite($strFileName, $$oFileHash{$strFile});

                # Compress if required
                if ($bCompress)
                {
                    $oFileLocal->compress(PATH_BACKUP_ABSOLUTE, $strFileName);
                    $strFileName .= '.' . $oFileLocal->{strCompressExtension};
                }

                # Add file to manifest
                $oBackupManifest->fileAdd(
                    $strFile,
                    (fileStat($strFileName))->mtime,
                    length($$oFileHash{$strFile}),
                    $oFileLocal->hash(PATH_BACKUP_ABSOLUTE, $strFileName, $bCompress), true);

                &log(DETAIL, "wrote '${strFile}' file returned from pg_stop_backup()");
            }
        }
    }

    # Remotes no longer needed (destroy them here so they don't timeout)
    &log(TEST, TEST_BACKUP_STOP);

    undef($oDbMaster);
    protocolDestroy(undef, undef, true);

    # If archive logs are required to complete the backup, then check them.  This is the default, but can be overridden if the
    # archive logs are going to a different server.  Be careful of this option because there is no way to verify that the backup
    # will be consistent - at least not here.
    if (optionGet(OPTION_ONLINE) && optionGet(OPTION_BACKUP_ARCHIVE_CHECK))
    {
        # Save the backup manifest a second time - before getting archive logs in case that fails
        $oBackupManifest->save();

        # Create the modification time for the archive logs
        my $lModificationTime = time();

        # After the backup has been stopped, need to make a copy of the archive logs to make the db consistent
        logDebugMisc($strOperation, "retrieve archive logs ${strArchiveStart}:${strArchiveStop}");
        my $oArchive = new pgBackRest::Archive();
        my $strArchiveId = $oArchive->getArchiveId($oFileLocal);
        my @stryArchive = lsnFileRange($strLsnStart, $strLsnStop, $strDbVersion);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchiveFile =
                $oArchive->walFileName($oFileLocal, $strArchiveId, $strArchive, false, optionGet(OPTION_ARCHIVE_TIMEOUT));
            $strArchive = substr($strArchiveFile, 0, 24);

            if (optionGet(OPTION_BACKUP_ARCHIVE_COPY))
            {
                logDebugMisc($strOperation, "archive: ${strArchive} (${strArchiveFile})");

                # Copy the log file from the archive repo to the backup
                my $strDestinationFile = MANIFEST_TARGET_PGDATA . "/pg_xlog/${strArchive}" .
                                         ($bCompress ? ".$oFileLocal->{strCompressExtension}" : '');
                my $bArchiveCompressed = $strArchiveFile =~ "^.*\.$oFileLocal->{strCompressExtension}\$";

                my ($bCopyResult, $strCopyChecksum, $lCopySize) =
                    $oFileLocal->copy(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strArchiveFile}",
                                 PATH_BACKUP_TMP, $strDestinationFile,
                                 $bArchiveCompressed, $bCompress,
                                 undef, $lModificationTime, undef, true);

                # Add the archive file to the manifest so it can be part of the restore and checked in validation
                my $strPathLog = MANIFEST_TARGET_PGDATA . '/pg_xlog';
                my $strFileLog = "${strPathLog}/${strArchive}";

                # Compare the checksum against the one already in the archive log name
                if ($strArchiveFile !~ "^${strArchive}-${strCopyChecksum}(\\.$oFileLocal->{strCompressExtension}){0,1}\$")
                {
                    confess &log(ERROR, "error copying WAL segment '${strArchiveFile}' to backup - checksum recorded with " .
                                        "file does not match actual checksum of '${strCopyChecksum}'", ERROR_CHECKSUM);
                }

                # Add file to manifest
                $oBackupManifest->fileAdd($strFileLog, $lModificationTime, $lCopySize, $strCopyChecksum, true);
            }
        }
    }

    # Create the path for the new backup
    my $lTimestampStop = time();
    my $strBackupLabel = backupLabelFormat($strType, $strBackupLastPath, $lTimestampStop);

    # Make sure that the timestamp has not already been used by a prior backup.  This is unlikely for online backups since there is
    # already a wait after the manifest is built but it's still possible if the remote and local systems don't have synchronized
    # clocks.  In practice this is most useful for making offline testing faster since it allows the wait after manifest build to
    # be skipped by dealing with any backup label collisions here.
#CSHANG why is "gz" hardcoded in this IF statement?
# CHANGE
    if (fileList($oFileLocal->pathGet(PATH_BACKUP_CLUSTER),
                 ($strType eq BACKUP_TYPE_FULL ? '^' : '_') .
                 timestampFileFormat(undef, $lTimestampStop) .
                 ($strType eq BACKUP_TYPE_FULL ? 'F' : '(D|I)$')) ||
        fileList($oFileLocal->pathGet(PATH_BACKUP_CLUSTER, PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTimestampStop)),
                 ($strType eq BACKUP_TYPE_FULL ? '^' : '_') .
                 timestampFileFormat(undef, $lTimestampStop) .
                 ($strType eq BACKUP_TYPE_FULL ? 'F' : '(D|I)\.manifest\.gz$'), undef, true))
    {
        waitRemainder();
        $strBackupLabel = backupLabelFormat($strType, $strBackupLastPath, time());
    }

    # Record timestamp stop in the config
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestampStop + 0);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);

    # Final save of the backup manifest
    $oBackupManifest->save();

    &log(INFO, "new backup label = ${strBackupLabel}");

    # Make a compressed copy of the manifest for history
    $oFileLocal->copy(PATH_BACKUP_TMP, FILE_MANIFEST,
                         PATH_BACKUP_TMP, FILE_MANIFEST . '.gz',
                         undef, true);

    # Move the backup tmp path to complete the backup
    logDebugMisc($strOperation, "move ${strBackupTmpPath} to " . $oFileLocal->pathGet(PATH_BACKUP_CLUSTER, $strBackupLabel));
    $oFileLocal->move(PATH_BACKUP_TMP, undef, PATH_BACKUP_CLUSTER, $strBackupLabel);

    # Copy manifest to history
    $oFileLocal->move(PATH_BACKUP_CLUSTER, "${strBackupLabel}/" . FILE_MANIFEST . '.gz',
                         PATH_BACKUP_CLUSTER, PATH_BACKUP_HISTORY . qw{/} . substr($strBackupLabel, 0, 4) .
                         "/${strBackupLabel}.manifest.gz", true);

    # Create a link to the most recent backup
    $oFileLocal->remove(PATH_BACKUP_CLUSTER, "latest");
    $oFileLocal->linkCreate(PATH_BACKUP_CLUSTER, $strBackupLabel, PATH_BACKUP_CLUSTER, "latest", undef, true);

    # Save backup info
    $oBackupInfo->add($oBackupManifest);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
