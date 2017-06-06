####################################################################################################################################
# EXPIRE MODULE
####################################################################################################################################
package pgBackRest::Expire;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);
use Scalar::Util qw(looks_like_number);

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveGet;
use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::Info;
use pgBackRest::Config::Config;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Initialize total archive expired
    $self->{iArchiveExpireTotal} = 0;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# logExpire
#
# Tracks which archive logs have been removed and provides log messages when needed.
####################################################################################################################################
sub logExpire
{
    my $self = shift;
    my $strArchiveId = shift;
    my $strArchiveFile = shift;

    if (defined($strArchiveFile))
    {
        if (!defined($self->{strArchiveExpireStart}))
        {
            $self->{strArchiveExpireStart} = $strArchiveFile;
            $self->{strArchiveExpireStop} = $strArchiveFile;
        }
        else
        {
            $self->{strArchiveExpireStop} = $strArchiveFile;
        }

        $self->{iArchiveExpireTotal}++;
    }
    else
    {
        if (defined($self->{strArchiveExpireStart}))
        {
            &log(DETAIL, "remove archive: archiveId = ${strArchiveId}, start = " . substr($self->{strArchiveExpireStart}, 0, 24) .
                ", stop = " . substr($self->{strArchiveExpireStop}, 0, 24));
        }

        undef($self->{strArchiveExpireStart});
    }
}

####################################################################################################################################
# process
#
# Removes expired backups and archive logs from the backup directory.  Partial backups are not counted for expiration, so if full
# or differential retention is set to 2, there must be three complete backups before the oldest one can be deleted.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    my @stryPath;

    my $oStorageRepo = storageRepo();
    my $strBackupClusterPath = $oStorageRepo->pathGet(STORAGE_REPO_BACKUP);
    my $iFullRetention = optionGet(OPTION_RETENTION_FULL, false);
    my $iDifferentialRetention = optionGet(OPTION_RETENTION_DIFF, false);
    my $strArchiveRetentionType = optionGet(OPTION_RETENTION_ARCHIVE_TYPE, false);
    my $iArchiveRetention = optionGet(OPTION_RETENTION_ARCHIVE, false);

    # Load the backup.info
    my $oBackupInfo = new pgBackRest::Backup::Info($oStorageRepo->pathGet(STORAGE_REPO_BACKUP));

    # Find all the expired full backups
    if (defined($iFullRetention))
    {
        # Make sure iFullRetention is valid
        if (!looks_like_number($iFullRetention) || $iFullRetention < 1)
        {
            confess &log(ERROR, 'retention-full must be a number >= 1');
        }

        @stryPath = $oBackupInfo->list(backupRegExpGet(true));

        if (@stryPath > $iFullRetention)
        {
            # Expire all backups that depend on the full backup
            for (my $iFullIdx = 0; $iFullIdx < @stryPath - $iFullRetention; $iFullIdx++)
            {
                my @stryRemoveList;

                foreach my $strPath ($oBackupInfo->list('^' . $stryPath[$iFullIdx] . '.*'))
                {
                    $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strPath}/" . FILE_MANIFEST);
                    $oBackupInfo->delete($strPath);

                    if ($strPath ne $stryPath[$iFullIdx])
                    {
                        push(@stryRemoveList, $strPath);
                    }
                }

                &log(INFO, 'expire full backup ' . (@stryRemoveList > 0 ? 'set: ' : '')  . $stryPath[$iFullIdx] .
                           (@stryRemoveList > 0 ? ', ' . join(', ', @stryRemoveList) : ''));
            }
        }
    }

    # Find all the expired differential backups
    if (defined($iDifferentialRetention))
    {
        # Make sure iDifferentialRetention is valid
        if (!looks_like_number($iDifferentialRetention) || $iDifferentialRetention < 1)
        {
            confess &log(ERROR, 'retention-diff must be a number >= 1');
        }

        # Get a list of full and differential backups. Full are considered differential for the purpose of retention.
        # Example: F1, D1, D2, F2 and retention-diff=2, then F1,D2,F2 will be retained, not D2 and D1 as might be expected.
        @stryPath = $oBackupInfo->list(backupRegExpGet(true, true));

        if (@stryPath > $iDifferentialRetention)
        {
            for (my $iDiffIdx = 0; $iDiffIdx < @stryPath - $iDifferentialRetention; $iDiffIdx++)
            {
                # Skip if this is a full backup.  Full backups only count as differential when deciding which differential backups
                # to expire.
                next if ($stryPath[$iDiffIdx] =~ backupRegExpGet(true));

                # Get a list of all differential and incremental backups
                my @stryRemoveList;

                foreach my $strPath ($oBackupInfo->list(backupRegExpGet(false, true, true)))
                {
                    logDebugMisc($strOperation, "checking ${strPath} for differential expiration");

                    # Remove all differential and incremental backups before the oldest valid differential
                    if ($strPath lt $stryPath[$iDiffIdx + 1])
                    {
                        $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strPath}" . FILE_MANIFEST);
                        $oBackupInfo->delete($strPath);

                        if ($strPath ne $stryPath[$iDiffIdx])
                        {
                            push(@stryRemoveList, $strPath);
                        }
                    }
                }

                &log(INFO, 'expire diff backup ' . (@stryRemoveList > 0 ? 'set: ' : '')  . $stryPath[$iDiffIdx] .
                           (@stryRemoveList > 0 ? ', ' . join(', ', @stryRemoveList) : ''));
            }
        }
    }

    $oBackupInfo->save();

    # Remove backups from disk
    foreach my $strBackup ($oStorageRepo->list(
        STORAGE_REPO_BACKUP, {strExpression => backupRegExpGet(true, true, true), strSortOrder => 'reverse'}))
    {
        if (!$oBackupInfo->current($strBackup))
        {
            &log(INFO, "remove expired backup ${strBackup}");

            $oStorageRepo->remove("${strBackupClusterPath}/${strBackup}", {bRecurse => true});
        }
    }

    # If archive retention is still undefined, then ignore archiving
    if  (!defined($iArchiveRetention))
    {
         &log(INFO, "option '" . &OPTION_RETENTION_ARCHIVE . "' is not set - archive logs will not be expired");
    }
    else
    {
        my @stryGlobalBackupRetention;

        # Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
        # remaining non-expired backups based on the type.
        if ($strArchiveRetentionType eq BACKUP_TYPE_FULL)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true, true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true, true, true), 'reverse');
        }

        # If no backups were found then preserve current archive logs - too soon to expire them
        my $iBackupTotal = scalar @stryGlobalBackupRetention;

        if ($iBackupTotal > 0)
        {
            my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE), true);
            my @stryListArchiveDisk = $oStorageRepo->list(
                STORAGE_REPO_ARCHIVE, {strExpression => REGEX_ARCHIVE_DIR_DB_VERSION, bIgnoreMissing => true});

            # Make sure the current database versions match between the two files
            if (!($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION)))) ||
                !($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef,
                    ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID)))))
            {
                confess &log(ERROR, "archive and backup database versions do not match\n" .
                    "HINT: has a stanza-upgrade been performed?", ERROR_FILE_INVALID);
            }

            # Get the list of backups that are part of archive retention
            my @stryTmp = @stryGlobalBackupRetention;
            my @stryGlobalBackupArchiveRetention = splice(@stryTmp, 0, $iArchiveRetention);

            # For each archiveId, remove WAL that are not part of retention
            foreach my $strArchiveId (@stryListArchiveDisk)
            {
                # From the global list of backups to retain, create a list of backups, oldest to newest, associated with this
                # archiveId (e.g. 9.4-1)
                my @stryLocalBackupRetention = $oBackupInfo->listByArchiveId($strArchiveId,
                    $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE), \@stryGlobalBackupRetention, 'reverse');

                # If no backup to retain was found
                if (!@stryLocalBackupRetention)
                {
                    # Get the backup db-id corresponding to this archiveId
                    my $iDbHistoryId = $oBackupInfo->backupArchiveDbHistoryId(
                        $strArchiveId, $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE));

                    # If this is not the current database, then delete the archive directory else do nothing since the current
                    # DB archive directory must not be deleted
                    if (!defined($iDbHistoryId) || !$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID, undef,
                        $iDbHistoryId))
                    {
                        my $strFullPath = $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveId}");

                        $oStorageRepo->remove($strFullPath, {bRecurse => true});

                        &log(INFO, "remove archive path: ${strFullPath}");
                    }

                    # Continue to next directory
                    next;
                }

                my @stryLocalBackupArchiveRentention;

                # If the archive retention is less than or equal to the number of all backups, then perform selective expiration
                if (@stryGlobalBackupArchiveRetention && $iArchiveRetention <= scalar @stryGlobalBackupRetention)
                {
                    # From the full list of backups in archive retention, find the intersection of local backups to retain
                    foreach my $strGlobalBackupArchiveRetention (@stryGlobalBackupArchiveRetention)
                    {
                        foreach my $strLocalBackupRetention (@stryLocalBackupRetention)
                        {
                            if ($strLocalBackupRetention eq $strGlobalBackupArchiveRetention)
                            {
                                unshift(@stryLocalBackupArchiveRentention, $strLocalBackupRetention);
                            }
                        }
                    }
                }
                # Else if there are not enough backups yet globally to start archive expiration then set the archive retention
                # to the oldest backup so anything prior to that will be removed as it is not needed but everything else is
                # This is incase there are old archives left around so that they don't stay around forever
                else
                {
                    if ($strArchiveRetentionType eq BACKUP_TYPE_FULL && scalar @stryLocalBackupRetention > 0)
                    {
                        &log(INFO, "full backup total < ${iArchiveRetention} - using oldest full backup for ${strArchiveId} " .
                            "archive retention");
                        $stryLocalBackupArchiveRentention[0] = $stryLocalBackupRetention[0];
                    }
                }

                # If no local backups were found as part of retention then set the backup archive retention to the newest backup
                # so that the database is fully recoverable (can be recovered from the last backup through pitr)
                if (!@stryLocalBackupArchiveRentention)
                {
                    $stryLocalBackupArchiveRentention[0] = $stryLocalBackupRetention[-1];
                }

                my $strArchiveRetentionBackup = $stryLocalBackupArchiveRentention[0];

                # If a backup has been selected for retention then continue
                if (defined($strArchiveRetentionBackup))
                {
                    my $bRemove;

                    # Only expire if the selected backup has archive info - backups performed with --no-online will
                    # not have archive info and cannot be used for expiration.
                    if ($oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT,
                                           $strArchiveRetentionBackup, INFO_BACKUP_KEY_ARCHIVE_START))
                    {
                        # Get archive ranges to preserve.  Because archive retention can be less than total retention it is
                        # important to preserve archive that is required to make the older backups consistent even though they
                        # cannot be played any further forward with PITR.
                        my $strArchiveExpireMax;
                        my @oyArchiveRange;
                        my @stryBackupList = $oBackupInfo->list();

                        # With the full list of backups, loop through only those associated with this archiveId
                        foreach my $strBackup (
                            $oBackupInfo->listByArchiveId(
                                $strArchiveId, $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE), \@stryBackupList))
                        {
                            if ($strBackup le $strArchiveRetentionBackup &&
                                $oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_ARCHIVE_START))
                            {
                                my $oArchiveRange = {};

                                $$oArchiveRange{start} = $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT,
                                                                           $strBackup, INFO_BACKUP_KEY_ARCHIVE_START);

                                if ($strBackup ne $strArchiveRetentionBackup)
                                {
                                    $$oArchiveRange{stop} = $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT,
                                                                               $strBackup, INFO_BACKUP_KEY_ARCHIVE_STOP);
                                }
                                else
                                {
                                    $strArchiveExpireMax = $$oArchiveRange{start};
                                }

                                &log(DETAIL, "archive retention on backup ${strBackup}, archiveId = ${strArchiveId}, " .
                                    "start = $$oArchiveRange{start}" .
                                    (defined($$oArchiveRange{stop}) ? ", stop = $$oArchiveRange{stop}" : ''));

                                push(@oyArchiveRange, $oArchiveRange);
                            }
                        }

                        # Get all major archive paths (timeline and first 32 bits of LSN)
                        foreach my $strPath ($oStorageRepo->list(
                            STORAGE_REPO_ARCHIVE . "/${strArchiveId}", {strExpression => REGEX_ARCHIVE_DIR_WAL}))
                        {
                            logDebugMisc($strOperation, "found major WAL path: ${strPath}");
                            $bRemove = true;

                            # Keep the path if it falls in the range of any backup in retention
                            foreach my $oArchiveRange (@oyArchiveRange)
                            {
                                if ($strPath ge substr($$oArchiveRange{start}, 0, 16) &&
                                    (!defined($$oArchiveRange{stop}) || $strPath le substr($$oArchiveRange{stop}, 0, 16)))
                                {
                                    $bRemove = false;
                                    last;
                                }
                            }

                            # Remove the entire directory if all archive is expired
                            if ($bRemove)
                            {
                                my $strFullPath = $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveId}") . "/${strPath}";

                                $oStorageRepo->remove($strFullPath, {bRecurse => true});

                                # Log expire info
                                logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");
                                $self->logExpire($strArchiveId, $strPath);
                            }
                            # Else delete individual files instead if the major path is less than or equal to the most recent
                            # retention backup.  This optimization prevents scanning though major paths that could not possibly
                            # have anything to expire.
                            elsif ($strPath le substr($strArchiveExpireMax, 0, 16))
                            {
                                # Look for files in the archive directory
                                foreach my $strSubPath ($oStorageRepo->list(
                                    STORAGE_REPO_ARCHIVE . "/${strArchiveId}/${strPath}", {strExpression => "^[0-F]{24}.*\$"}))
                                {
                                    $bRemove = true;

                                    # Determine if the individual archive log is used in a backup
                                    foreach my $oArchiveRange (@oyArchiveRange)
                                    {
                                        if (substr($strSubPath, 0, 24) ge $$oArchiveRange{start} &&
                                            (!defined($$oArchiveRange{stop}) || substr($strSubPath, 0, 24) le $$oArchiveRange{stop}))
                                        {
                                            $bRemove = false;
                                            last;
                                        }
                                    }

                                    # Remove archive log if it is not used in a backup
                                    if ($bRemove)
                                    {
                                        $oStorageRepo->remove(STORAGE_REPO_ARCHIVE . "/${strArchiveId}/${strSubPath}");

                                        logDebugMisc($strOperation, "remove WAL segment: ${strArchiveId}/${strSubPath}");

                                        # Log expire info
                                        $self->logExpire($strArchiveId, substr($strSubPath, 0, 24));
                                    }
                                    else
                                    {
                                        # Log that the file was not expired
                                        $self->logExpire($strArchiveId);
                                    }
                                }
                            }
                        }

                        # Log if no archive was expired
                        if ($self->{iArchiveExpireTotal} == 0)
                        {
                            &log(DETAIL, "no archive to remove, archiveId = ${strArchiveId}");
                        }
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
