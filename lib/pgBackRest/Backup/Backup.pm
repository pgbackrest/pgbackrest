####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package pgBackRest::Backup::Backup;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
use File::Basename;
use JSON::PP;

use pgBackRest::Archive::Common;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Cipher;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Local::Process;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Helper;
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
# process
#
# Process the database backup.
####################################################################################################################################
sub process
{
    my $self = shift;
    my @stryCommandArg = @_;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Get parameters passed from C backup code
    if (@stryCommandArg != 1)
    {
        confess &log(ERROR, "missing command parameter from C");
    }

    my $rhParam = (JSON::PP->new()->allow_nonref())->decode($stryCommandArg[0]);

    # Load backup.info
    my $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP));
    my $strCipherPassManifest = $oBackupInfo->cipherPassSub();

    # Load manifest passed from C
    my $strBackupPath = storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}");

    my $oBackupManifest = new pgBackRest::Manifest(
        STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}/" . FILE_MANIFEST, {oStorage => storageRepo(),
        strCipherPass => defined($oBackupInfo->cipherPassSub()) ? $oBackupInfo->cipherPassSub() : undef});
    my $strCipherPassBackupSet = $oBackupManifest->cipherPassSub();

    # Start backup (unless --no-online is set)
    my $oDbMaster = undef;
    my $oDbStandby = undef;

    ($oDbMaster, $self->{iMasterRemoteIdx}, $oDbStandby, $self->{iCopyRemoteIdx}) = dbObjectGet();

    if (!defined($self->{iCopyRemoteIdx}))
    {
        $self->{iCopyRemoteIdx} = $self->{iMasterRemoteIdx};
    }

    my $oStorageDbMaster = storageDb({iRemoteIdx => $self->{iMasterRemoteIdx}});

    my $strDbMasterPath = cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $self->{iMasterRemoteIdx}));
    my $strDbCopyPath = cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $self->{iCopyRemoteIdx}));

    &log(TEST, TEST_MANIFEST_BUILD);

    ################################################################################################################################
    # ALL THE ABOVE EXISTS ONLY FOR MIGRATION
    ################################################################################################################################
    # Set the delta option in the manifest
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA, undef, cfgOption(CFGOPT_DELTA));

    # Master file object no longer needed
    undef($oStorageDbMaster);

    # Stop backup (unless --no-online is set)
    my $strArchiveStop = undef;
    my $strLsnStop = undef;

    if (cfgOption(CFGOPT_ONLINE))
    {
        ($strArchiveStop, $strLsnStop, my $strTimestampDbStop, my $oFileHash) = $oDbMaster->backupStop();

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_STOP, undef, $strLsnStop);
        &log(INFO, "backup stop archive = ${strArchiveStop}, lsn = ${strLsnStop}");

        # Write out files returned from stop backup
        foreach my $strFile (sort(keys(%{$oFileHash})))
        {
            # Only save the file if it has content
            if (defined($oFileHash->{$strFile}))
            {
                my $rhyFilter = [{strClass => STORAGE_FILTER_SHA}];

                # Add compression filter
                if (cfgOption(CFGOPT_COMPRESS))
                {
                    push(
                        @{$rhyFilter},
                        {strClass => STORAGE_FILTER_GZIP, rxyParam => [STORAGE_COMPRESS, false, cfgOption(CFGOPT_COMPRESS_LEVEL)]});
                }

                # If the backups are encrypted, then the passphrase for the backup set from the manifest file is required to access
                # the file in the repo
                my $oDestinationFileIo = storageRepo()->openWrite(
                    STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}/${strFile}" .
                        (cfgOption(CFGOPT_COMPRESS) ? qw{.} . COMPRESS_EXT : ''),
                    {rhyFilter => $rhyFilter,
                        strCipherPass => defined($strCipherPassBackupSet) ? $strCipherPassBackupSet : undef});

                # Write content out to a file
                storageRepo()->put($oDestinationFileIo, $oFileHash->{$strFile});

                # Add file to manifest
                $oBackupManifest->fileAdd(
                    $strFile, time(), length($oFileHash->{$strFile}), $oDestinationFileIo->result(STORAGE_FILTER_SHA), true);

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
    if (cfgOption(CFGOPT_ONLINE) && cfgOption(CFGOPT_ARCHIVE_CHECK))
    {
        # Save the backup manifest before getting archive logs in case of failure
        $oBackupManifest->saveCopy();

        # Create the modification time for the archive logs
        my $lModificationTime = time();

        # After the backup has been stopped, need to make a copy of the archive logs to make the db consistent
        logDebugMisc($strOperation, "retrieve archive logs !!!START!!!:!!!STOP!!!");

        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), true);
        my $strArchiveId = $oArchiveInfo->archiveId();
        my @stryArchive = lsnFileRange('START', $strLsnStop, $rhParam->{pgVersion}, 16);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchiveFile = walSegmentFind(
                storageRepo(), $strArchiveId, substr($strArchiveStop, 0, 8) . $strArchive, cfgOption(CFGOPT_ARCHIVE_TIMEOUT));

            $strArchive = substr($strArchiveFile, 0, 24);

            if (cfgOption(CFGOPT_ARCHIVE_COPY))
            {
                logDebugMisc($strOperation, "archive: ${strArchive} (${strArchiveFile})");

                # Copy the log file from the archive repo to the backup
                my $bArchiveCompressed = $strArchiveFile =~ ('^.*\.' . COMPRESS_EXT . '\$');

                storageRepo()->copy(
                    storageRepo()->openRead(STORAGE_REPO_ARCHIVE . "/${strArchiveId}/${strArchiveFile}",
                        {strCipherPass => $oArchiveInfo->cipherPassSub()}),
                    storageRepo()->openWrite(STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}/" . MANIFEST_TARGET_PGDATA . qw{/} .
                        $oBackupManifest->walPath() . "/${strArchive}" . (cfgOption(CFGOPT_COMPRESS) ? qw{.} . COMPRESS_EXT : ''),
                        {bPathCreate => true, strCipherPass => $strCipherPassBackupSet})
                    );

                # Add the archive file to the manifest so it can be part of the restore and checked in validation
                my $strPathLog = MANIFEST_TARGET_PGDATA . qw{/} . $oBackupManifest->walPath();
                my $strFileLog = "${strPathLog}/${strArchive}";

                # Add file to manifest
                $oBackupManifest->fileAdd(
                    $strFileLog, $lModificationTime, PG_WAL_SEGMENT_SIZE, substr($strArchiveFile, 25, 40), true);
            }
        }
    }

    # Record timestamp stop in the config
    my $lTimestampStop = time();
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestampStop + 0);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $rhParam->{backupLabel});

    # Sync backup path if supported
    if (storageRepo()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        # Sync all paths in the backup
        storageRepo()->pathSync(STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}");

        foreach my $strPath ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_PATH))
        {
            my $strPathSync = storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}/$strPath");

            # Not all paths are created for diff/incr backups, so only sync if this is a full backup or the path exists
            if (cfgOption(CFGOPT_TYPE) eq CFGOPTVAL_BACKUP_TYPE_FULL || storageRepo()->pathExists($strPathSync))
            {
                storageRepo()->pathSync($strPathSync);
            }
        }
    }

    # Final save of the backup manifest
    $oBackupManifest->save();

    &log(INFO, "new backup label = $rhParam->{backupLabel}");

    # Copy a compressed version of the manifest to history. If the repo is encrypted then the passphrase to open the manifest is
    # required.
    my $strHistoryPath = storageRepo()->pathGet(
        STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . qw{/} . substr($rhParam->{backupLabel}, 0, 4));

    storageRepo()->copy(
        storageRepo()->openRead(STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}/" . FILE_MANIFEST,
            {'strCipherPass' => $oBackupInfo->cipherPassSub()}),
        storageRepo()->openWrite(
            "${strHistoryPath}/$rhParam->{backupLabel}.manifest." . COMPRESS_EXT,
            {rhyFilter => [{strClass => STORAGE_FILTER_GZIP, rxyParam => [STORAGE_COMPRESS, false, 9]}],
                bPathCreate => true, bAtomic => true,
                strCipherPass => defined($oBackupInfo->cipherPassSub()) ? $oBackupInfo->cipherPassSub() : undef}));

    # Sync history path if supported
    if (storageRepo()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        storageRepo()->pathSync(STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY);
        storageRepo()->pathSync($strHistoryPath);
    }

    # Create a link to the most recent backup
    storageRepo()->remove(STORAGE_REPO_BACKUP . qw(/) . LINK_LATEST);

    if (storageRepo()->capability(STORAGE_CAPABILITY_LINK))
    {
        storageRepo()->linkCreate(
            STORAGE_REPO_BACKUP . "/$rhParam->{backupLabel}", STORAGE_REPO_BACKUP . qw{/} . LINK_LATEST, {bRelative => true});
    }

    # Save backup info
    $oBackupInfo->add($oBackupManifest);

    # Sync backup root path if supported
    if (storageRepo()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        storageRepo()->pathSync(STORAGE_REPO_BACKUP);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
