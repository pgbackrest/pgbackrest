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

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
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
# CSHANG Although not apparent here, the expire command requires the stanza option cfgOptionStr(cfgOptStanza)
    my $oStorageRepo = storageRepo();
    my $strBackupClusterPath = $oStorageRepo->pathGet(STORAGE_REPO_BACKUP); # CSHANG this is never used
    my $iFullRetention = cfgOption(CFGOPT_REPO_RETENTION_FULL, false);
    my $iDifferentialRetention = cfgOption(CFGOPT_REPO_RETENTION_DIFF, false);
    my $strArchiveRetentionType = cfgOption(CFGOPT_REPO_RETENTION_ARCHIVE_TYPE, false);
    my $iArchiveRetention = cfgOption(CFGOPT_REPO_RETENTION_ARCHIVE, false);

    # Load the backup.info
    my $oBackupInfo = new pgBackRest::Backup::Info($oStorageRepo->pathGet(STORAGE_REPO_BACKUP));
# CSHANG
# String *stanzaBackupPath = strNewFmt(STORAGE_PATH_BACKUP "/%s", strPtr(cfgOptionStr(cfgOptStanza)));
# info = infoBackupNew(
#     storageRepo(), strNewFmt("/%s/%s", strPtr(stanzaBackupPath), INFO_BACKUP_FILE), false,
#     cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

    # Find all the expired full backups
    if (defined($iFullRetention))
    {
        # Make sure iFullRetention is valid
        if (!looks_like_number($iFullRetention) || $iFullRetention < 1)
        {
            confess &log(ERROR, cfgOptionName(CFGOPT_REPO_RETENTION_FULL) . ' must be a number >= 1');
        }
# CSHANG backupRegExpGet returns a regex and list looks in the CURRENT section for backupinfo and returns a list matching that
# StringList *backupFullList = storageListP(storageRepo(), stanzaBackupPath, .errorOnMissing = false);
        @stryPath = $oBackupInfo->list(backupRegExpGet(true));

        if (@stryPath > $iFullRetention)
        {
            # Expire all backups that depend on the full backup
            for (my $iFullIdx = 0; $iFullIdx < @stryPath - $iFullRetention; $iFullIdx++)
            {
                my @stryRemoveList;
# CSHANG list here gets all the backups in the CURRENT section that start with the name of the full backup
# CSHANG fill=20190405-150606F  incr=20190405-150606F_20190408-140449I
                foreach my $strPath ($oBackupInfo->list('^' . $stryPath[$iFullIdx] . '.*'))
                {
# CSHANG we'll need the storageRemove (I think NP) for removing files
                    $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strPath}/" . FILE_MANIFEST . INI_COPY_EXT);
                    $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strPath}/" . FILE_MANIFEST);
# CSHANG this delete removes the backup from the CURRENT section of the info file, not the path from the disk (that is done later)
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
# CSHANG Do we need this check or does the parser enforce this?
        if (!looks_like_number($iDifferentialRetention) || $iDifferentialRetention < 1)
        {
            confess &log(ERROR, cfgOptionName(CFGOPT_REPO_RETENTION_DIFF) . ' must be a number >= 1');
        }

        # Get a list of full and differential backups. Full are considered differential for the purpose of retention.
        # Example: F1, D1, D2, F2 and repo-retention-diff=2, then F1,D2,F2 will be retained, not D2 and D1 as might be expected.
        @stryPath = $oBackupInfo->list(backupRegExpGet(true, true));
# CSHANG so we must know that 20190405-150606F_20190408-140449I does not depend on the DIFF backup because the number is less?
# drwxr-x--- 3 postgres postgres 4096 Apr  5 15:06 20190405-150606F
# drwxr-x--- 3 postgres postgres 4096 Apr  8 14:04 20190405-150606F_20190408-140449D
# drwxr-x--- 3 postgres postgres 4096 Apr  8 14:17 20190405-150606F_20190408-141709D
# drwxr-x--- 3 postgres postgres 4096 Apr  8 14:17 20190405-156666F

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
# CSHANG Wait?! Where is the removal of the COPY? AND, is this path even correct? Shouldn't there be a slash after /${strPath}
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
# CSHANG why is the word "still" here?  The real comment should be "keep archive logs for the number of backups defined by repo-retention-archive"
    # If archive retention is still undefined, then ignore archiving
    if  (!defined($iArchiveRetention))
    {
         &log(INFO, "option '" . cfgOptionName(CFGOPT_REPO_RETENTION_ARCHIVE) . "' is not set - archive logs will not be expired");
    }
    else
    {
        my @stryGlobalBackupRetention;

        # Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
        # remaining non-expired backups based on the type.
# CSHANG Maybe enhance this comment to "get the list of the remaining backups, from newest to oldest" - the CURRENT section has the backups from oldest to newest so we're asking for them in reverse
        if ($strArchiveRetentionType eq CFGOPTVAL_BACKUP_TYPE_FULL)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq CFGOPTVAL_BACKUP_TYPE_DIFF)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true, true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq CFGOPTVAL_BACKUP_TYPE_INCR)
        {
            @stryGlobalBackupRetention = $oBackupInfo->list(backupRegExpGet(true, true, true), 'reverse');
        }

        # If no backups were found then preserve current archive logs - too soon to expire them
        my $iBackupTotal = scalar @stryGlobalBackupRetention;

        if ($iBackupTotal > 0)
        {
            my $oArchiveInfo = new pgBackRest::Archive::Info($oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE), true);
            my @stryListArchiveDisk = sort {((split('-', $a))[1] + 0) cmp ((split('-', $b))[1] + 0)} $oStorageRepo->list(
                STORAGE_REPO_ARCHIVE, {strExpression => REGEX_ARCHIVE_DIR_DB_VERSION, bIgnoreMissing => true});
# CSHANG I think the HINT here doesn't make any sense. If an upgrade was performed, then both the archive and backup info would have been updated and if only one is updated, then there is some corruption so what does the hint really mean here?
            # Make sure the current database versions match between the two files
            if (!($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION)))) ||
                !($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef,
                    ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID)))))
            {
                confess &log(ERROR, "archive and backup database versions do not match\n" .
                    "HINT: has a stanza-upgrade been performed?", ERROR_FILE_INVALID);
            }
# CSHANG Since the list should be ordered from newest to oldest, then get only the backups from the array for the count of the retention. The globalBackupRetention will have, say 4F, 3F, 2F, 1F and if we're retaining 2, then 4F and 3F will be in the globalBackupArchiveRetention
            # Get the list of backups that are part of archive retention
            my @stryTmp = @stryGlobalBackupRetention; # CSHANG copied to tmp becase splice removes the elements 0 to whatever from the array
            my @stryGlobalBackupArchiveRetention = splice(@stryTmp, 0, $iArchiveRetention); # CSHANG splice returns an array

            # For each archiveId, remove WAL that are not part of retention
            foreach my $strArchiveId (@stryListArchiveDisk)
            {
                # From the global list of backups to retain, create a list of backups, oldest to newest, associated with this
                # archiveId (e.g. 9.4-1)
# CSHANG If globalBackupRetention has 4F, 3F, 2F, 1F then I'm returning 1F, 2F, 3F, 4F (assuming they all have same history id)
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
                    if ($strArchiveRetentionType eq CFGOPTVAL_BACKUP_TYPE_FULL && scalar @stryLocalBackupRetention > 0)
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
# CSHANG here we'll need storagePathRemove
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
