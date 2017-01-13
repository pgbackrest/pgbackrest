####################################################################################################################################
# EXPIRE MODULE
####################################################################################################################################
package pgBackRest::Expire;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);
use File::Path qw(remove_tree);
use Scalar::Util qw(looks_like_number);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::ArchiveCommon;
use pgBackRest::BackupCommon;
use pgBackRest::BackupInfo;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

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

    # Initialize file object
    $self->{oFile} = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(NONE)
    );

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
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->DESTROY');

    undef($self->{oFile});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# logExpire
#
# Tracks which archive logs have been removed and provides log messages when needed.
####################################################################################################################################
sub logExpire
{
    my $self = shift;
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
            &log(DETAIL, 'remove archive: start = ' . substr($self->{strArchiveExpireStart}, 0, 24) .
                       ', stop = ' . substr($self->{strArchiveExpireStop}, 0, 24));
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

    my $oFile = $self->{oFile};
    my $strBackupClusterPath = $oFile->pathGet(PATH_BACKUP_CLUSTER);
    my $iFullRetention = optionGet(OPTION_RETENTION_FULL, false);
    my $iDifferentialRetention = optionGet(OPTION_RETENTION_DIFF, false);
    my $strArchiveRetentionType = optionGet(OPTION_RETENTION_ARCHIVE_TYPE, false);
    my $iArchiveRetention = optionGet(OPTION_RETENTION_ARCHIVE, false);

    # Load the backup.info
    my $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));

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
                    $oFile->remove(PATH_BACKUP_CLUSTER, "${strPath}/" . FILE_MANIFEST);
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
                        $oFile->remove(PATH_BACKUP_CLUSTER, "/${strPath}" . FILE_MANIFEST);
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
    foreach my $strBackup ($oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true, true, true), 'reverse'))
    {
        if (!$oBackupInfo->current($strBackup))
        {
            &log(INFO, "remove expired backup ${strBackup}");

            remove_tree("${strBackupClusterPath}/${strBackup}") > 0
                or confess &log(ERROR, "unable to remove backup ${strBackup}", ERROR_PATH_REMOVE);
        }
    }

    # If archive retention is still undefined, then ignore archiving
    if  (!defined($iArchiveRetention))
    {
         &log(INFO, "option '" . &OPTION_RETENTION_ARCHIVE . "' is not set - archive logs will not be expired");
    }
    else
    {
# Get a list of upper directories in the archive dir (Dave says should do this in a loop even if always redundant
# Get the history from the archive info file
# Find the system-id in the history list - if does not exist, then error
# Get the history from the backup.info
# Map the system-id and version from the archive file to the backup id
# Get a list of all backups in backup::current to determin retention
# Parse the list to indicate which DB-ID (or archiveid?) the backup belongs to
# When processing, do each archive dir in isolation.

        # Build the db list from the history in the backup info and archive info file
        my $hDbListBackup = $oBackupInfo->dbHistoryList();
        my $hDbListArchive = (new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->dbHistoryList();

        # my %hArchiveDbIdMap;
        #
        # # Create a mapping table for the archive id to the backup DB id
        # foreach my $oDbListBackup (@oyDbListBackup)
        # {
        #     foreach my $oDbListArchive (@oyDbListArchive)
        #     {
        #         if ($$oDbListBackup{&INFO_SYSTEM_ID} eq $$oDbListArchive{&INFO_SYSTEM_ID} &&
        #             $$oDbListBackup{&INFO_DB_VERSION} eq $$oDbListArchive{&INFO_DB_VERSION})
        #         {
        #             # $hDbIdArchiveIdMap->{$$oDbListBackup{&INFO_HISTORY_ID}} = $$oDbListArchive{&INFO_DB_VERSION}  . "-" .
        #             #     $$oDbListArchive{&INFO_HISTORY_ID};
        #
                    # $hArchiveDbIdMap{$$oDbListBackup{&INFO_HISTORY_ID}}{&INFO_HISTORY_ID} =  $$oDbListArchive{&INFO_HISTORY_ID};
                    # $hArchiveDbIdMap{$$oDbListBackup{&INFO_HISTORY_ID}}{&INFO_DB_VERSION} =  $$oDbListArchive{&INFO_DB_VERSION};
        #         }
        #     }
        # }


    foreach my $strArchiveId (fileList($oFile->pathGet(PATH_BACKUP_ARCHIVE), REGEX_ARCHIVE_DIR_DB_VERSION, 'forward', true))
    {
        # Get the db-version and db-id (history id) from the directory name
        my ($strDbVersionArchive, $iDbIdArchive) = split("-", $strArchiveId);

        if (!defined($$hDbListArchive{$iDbIdArchive}{&INFO_SYSTEM_ID}))
        {
            confess &log(ERROR, "archive info history does not match with the directories on disk\n" .
                "HINT: has a stanza-create been performed?", ERROR_FILE_INVALID);
        }

        my $strDbSystemIdArchive = $$hDbListArchive{$iDbIdArchive}{&INFO_SYSTEM_ID};

        # Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
        # remaining non-expired backups based on the type.
        if ($strArchiveRetentionType eq BACKUP_TYPE_FULL)
        {
            @stryPath = $oBackupInfo->list(backupRegExpGet(true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
        {
            @stryPath = $oBackupInfo->list(backupRegExpGet(true, true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
        {
            @stryPath = $oBackupInfo->list(backupRegExpGet(true, true, true), 'reverse');
        }

        # If no backups were found then preserve current archive logs - too soon to expire them
        my $iBackupTotal = scalar @stryPath;

        if ($iBackupTotal > 0)
        {
            # See if enough backups exist for expiration to start
            my $strArchiveRetentionBackup = $stryPath[$iArchiveRetention - 1];

            if (!defined($strArchiveRetentionBackup))
            {
                if ($strArchiveRetentionType eq BACKUP_TYPE_FULL && scalar @stryPath > 0)
                {
                    &log(INFO, "full backup total < ${iArchiveRetention} - using oldest full backup for archive retention");
                    $strArchiveRetentionBackup = $stryPath[scalar @stryPath - 1];
                }
            }

#CSHANG!!! - The archive to retain can ONLY Be in one cluster. So only deal with that directory? And at the end, if there is a directory that is empty (or the beginning) and it is not the current cluster than delete it. But how to deal with orphaned directories? Say I get the first dir, and this one is not empty and I check that it is the current cluster, and it is not, then I need to remove it if it doe not have a backup in the list. so if I'm looping through backups and I don't get to know for this archive dir, then I can remove.

#             # If a backup has been selected for retention then continue
#             if (defined($strArchiveRetentionBackup))
#             {
#                 logDebugMisc($strOperation, "backup selected for retention ${strArchiveRetentionBackup}");
#                 my $bRemove;
#
#                 # Only expire if the selected backup has archive info - backups performed with --no-online will
#                 # not have archive info and cannot be used for expiration.
#                 if ($oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT,
#                                        $strArchiveRetentionBackup, INFO_BACKUP_KEY_ARCHIVE_START))
#                 {
#                     # Get archive info
#                     # my $oArchive = new pgBackRest::Archive();
# # CSHANG This is where we need to get the correct dir and do in a loop. The ArchiveId is the data from DB section db-id and version,
# # instead we need to get the directories - make sure they are represented in the archive info file history section - and then map
# # the db-system-id and db-version back to the backup info (the $oBackupInfo->get needs to get the DB-ID from the current section below
# # in addition to the start and stop and then make sure we're going to the correct directory in archive to remove the WAL)
# # So maybe listArchive() would return a list of archiveIds $oBackupInfo->get from the archive info dir and then we get the system-id from the archive info history given the db-id from the archiveIds returned. If any are missing any, we need to abort?
#                     # my $strArchiveId = $oArchive->getArchiveId($oFile);
#
#                     my $strArchiveExpireMax;
#
#                     # Get archive ranges to preserve.  Because archive retention can be less than total retention it is important
#                     # to preserve archive that is required to make the older backups consistent even though they cannot be played
#                     # any further forward with PITR.
#                     my @oyArchiveRange;
#
#
#                     my $iArchiveRetentionBackupDbId =
#                         $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID);
#
# # CSHANG The list function get from the INFO_BACKUP_SECTION_BACKUP_CURRENT so for each backup still listed in current section of backup info the array returned is (default) forward. For these, the archive range start will be set and stop will only be set if $strBackup ne $strArchiveRetentionBackup.
#                     foreach my $strBackup ($oBackupInfo->list())  # CSHANG 20170111-205044F, 20170111-205319F
#                     {
# # CSHANG - why LE? I can understand equal, but wouldn't we want to expire Less than? Or does this have to do with dependencies - e.g. if retention-archive is set to diff then it must keep the WAL for the prior FULL backup.
#                         if ($strBackup le $strArchiveRetentionBackup &&  #20170111-205044F is equal so get archive range
#                             $oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_ARCHIVE_START))
#                         {
#                             my $oArchiveRange = {};
#
#                             $$oArchiveRange{start} = $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT,
#                                                                        $strBackup, INFO_BACKUP_KEY_ARCHIVE_START);
#
#                             if ($strBackup ne $strArchiveRetentionBackup)
#                             {
#                                 $$oArchiveRange{stop} = $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT,
#                                                                            $strBackup, INFO_BACKUP_KEY_ARCHIVE_STOP);
#                             }
#                             else
#                             {
#                                 $strArchiveExpireMax = $$oArchiveRange{start};
#                             }
#
#                             # Get the database id of this backup so it can be mapped to retrieve the archiveId
#                             my $iDbId =
#                                 $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID);
#
#                             # Store the mapped archive id
#                             $$oArchiveRange{archiveId} = $hDbIdArchiveIdMap{$iDbId}{&INFO_DB_VERSION} . "-" . $hDbIdArchiveIdMap{$iDbId}{&INFO_HISTORY_ID};
#
#                             &log(DETAIL, "archive retention on backup ${strBackup}, archiveId = $$oArchiveRange{archiveId}, " .
#                                 "start = $$oArchiveRange{start}" . (defined($$oArchiveRange{stop}) ? ", stop = " .
#                                 "$$oArchiveRange{stop}" : ''));
#
#                             push(@oyArchiveRange, $oArchiveRange);
#                         }
#                     }
# # CSHANG Problem is when upgrade (or revert) we can't rely on the WAL IDs only - AND we can't rely on any sequence of the DB versions (e.g could have reverted). But we should be able to rely on the timestamp (no? - only if they did not change timezones or something)
#
#                     foreach my $oDbList (@oyDbListBackup)
#                     {
#                         my $strArchiveId = $hDbIdArchiveIdMap{$$oDbList{&INFO_HISTORY_ID}}{&INFO_DB_VERSION} . "-" . $hDbIdArchiveIdMap{$$oDbList{&INFO_HISTORY_ID}}{&INFO_HISTORY_ID};
#
# #CSHANG This section sets the list of all files on disk in the archive dir. Since we had only one db version before then this would just get all the dirs in the archive dir. e.g 0000000100000000
#                         # Get all major archive paths (timeline and first 64 bits of LSN)
#                         foreach my $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, REGEX_ARCHIVE_DIR_WAL))
#                         {
#                             logDebugMisc($strOperation, "found major WAL path: ${strArchiveId} / ${strPath}"); #CSHANG Added archive id
#                             $bRemove = true;
#
#                             # Keep the path if it falls in the range of any backup in retention
#                             foreach my $oArchiveRange (@oyArchiveRange)
#                             {
#                                 logDebugMisc($strOperation, "archive retention on range db id = $$oArchiveRange{archiveId}, strArchiveId ${strArchiveId}, start =  $$oArchiveRange{start}" . (defined($$oArchiveRange{stop}) ? ", stop = $$oArchiveRange{stop}" : ''));  #CSHANG
#                                 if ($strPath ge substr($$oArchiveRange{start}, 0, 16) &&
#                                     (!defined($$oArchiveRange{stop}) || $strPath le substr($$oArchiveRange{stop}, 0, 16)) &&
#                                     $$oArchiveRange{archiveId} eq $strArchiveId)
#                                 {
#                                     $bRemove = false;
#                                     &log(INFO, "NOT REMOVING entire dir");
#                                     last;
#                                 }
#                             }
#
#                             # Remove the entire directory if all archive is expired
#                             if ($bRemove)
#                             {
#                                 my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";
#
#                                 remove_tree($strFullPath) > 0
#                                     or confess &log(ERROR, "unable to remove ${strFullPath}", ERROR_PATH_REMOVE);
#
#                                 # Log expire info
#                                 logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");
#                                 $self->logExpire($strPath);
#                             }
#                             # Else delete individual files instead if the major path is less than or equal to the most recent retention
#                             # backup.  This optimization prevents scanning though major paths that could not possibly have anything to
#                             # expire.
#                             elsif ($strPath le substr($strArchiveExpireMax, 0, 16))
#                             {
#                                 # Look for files in the archive directory
#                                 foreach my $strSubPath ($oFile->list(PATH_BACKUP_ARCHIVE,
#                                                                      "${strArchiveId}/${strPath}", "^[0-F]{24}.*\$"))
#                                 {
#                                     $bRemove = true;
#
#                                     # Determine if the individual archive log is used in a backup
#                                     foreach my $oArchiveRange (@oyArchiveRange)
#                                     {
#                                         if (substr($strSubPath, 0, 24) ge $$oArchiveRange{start} &&
#                                             (!defined($$oArchiveRange{stop}) ||
#                                             substr($strSubPath, 0, 24) le $$oArchiveRange{stop}) &&
#                                             $$oArchiveRange{archiveId} eq $strArchiveId)
#                                         {
#                                             $bRemove = false;
#                                             &log(INFO, "NOT REMOVING individual");
#                                             last;
#                                         }
#                                     }
#
#                                     # Remove archive log if it is not used in a backup
#                                     if ($bRemove)
#                                     {
#                                         fileRemove($oFile->pathGet(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSubPath}"));
#
#                                         logDebugMisc($strOperation, "remove WAL segment: ${strSubPath}");
#
#                                         # Log expire info
#                                         $self->logExpire(substr($strSubPath, 0, 24));
#                                     }
#                                     else
#                                     {
#                                         # Log that the file was not expired
#                                         $self->logExpire();
#                                     }
#                                 }
#                             }
#                         }
#                     }
#
#                     # Log if no archive was expired
#                     if ($self->{iArchiveExpireTotal} == 0)
#                     {
#                         &log(DETAIL, 'no archive to remove');
#                     }
#                 }
            }
            # if no backups exist yet then remove the directory if it is not the current cluster
            else
            {
                my $iDbCurrentHistoryId = $oBackupInfo->dbHistoryIdGet();

                #
                if ($strDbSystemIdArchive ne $$hDbListBackup{$iDbCurrentHistoryId}{&INFO_SYSTEM_ID} &&
                    $strDbVersionArchive ne $$hDbListBackup{$iDbCurrentHistoryId}{&INFO_DB_VERSION})
                {
                    my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

                    remove_tree($strFullPath) > 0
                        or confess &log(ERROR, "unable to remove ${strFullPath}", ERROR_PATH_REMOVE);

                    # Log expire info
                    logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");
                    $self->logExpire($strPath);
                }
                else
                {
                    confess &log(ASSERT, "unable to retrieve current database system and version from backup info\n" .
                        "HINT: has a stanza-create been performed?");
                }
            }
        }

    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
