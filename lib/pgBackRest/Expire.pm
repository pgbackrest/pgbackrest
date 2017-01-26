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
use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveInfo;
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
        my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true);
        my $hDbListArchive = $oArchiveInfo->dbHistoryList();
        my @stryListArchiveDisk = fileList($oFile->pathGet(PATH_BACKUP_ARCHIVE), REGEX_ARCHIVE_DIR_DB_VERSION, 'forward', true);
        my %hDbIdArchiveIdMap;

        # Make sure the current database versions match between the two files
        if (!($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION))) ||
            $oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef,
                ($oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID)))))
        {
            confess &log(ERROR, "archive and backup database versions do not match\n" .
                "HINT: has a stanza-upgrade been performed?", ERROR_FILE_INVALID);
        }

        # Make sure major WAL archive directories are present in the archive info file before proceeding
        foreach my $strArchiveDisk (@stryListArchiveDisk)
        {
            # Get the db-version and db-id (history id) from the directory name
            my ($strDbVersionDisk, $iDbIdDisk) = split("-", $strArchiveDisk);

            # If this directory version/db-id does not have a corresponding entry in the archive info file then error
            if (!defined($$hDbListArchive{$iDbIdDisk}{&INFO_SYSTEM_ID}) ||
                (defined($$hDbListArchive{$iDbIdDisk}{&INFO_SYSTEM_ID}) && $$hDbListArchive{$iDbIdDisk}{&INFO_DB_VERSION} ne $strDbVersionDisk))
            {
                confess &log(ERROR, "archive info history does not match with the directories on disk\n" .
                    "HINT: has a stanza-upgrade been performed?", ERROR_FILE_INVALID);
            }
        }

        # Clean up any orphaned directories
        foreach my $strArchiveDir (@stryListArchiveDisk)
        {
            # Get the db-version and db-id (history id) from the directory name
            my ($strDbVersionArchive, $iDbIdArchive) = split("-", $strArchiveDir);

            # Get the DB system ID to map back to the backup info
            my $ullDbSysIdArchive = $$hDbListArchive{$iDbIdArchive}{&INFO_SYSTEM_ID};

            # Determine if this is the current database
            my $bCurrentDb =
                ($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersionArchive) &&
                 $oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysIdArchive))
                 ? true : false;

            my $iDbId = undef;

            # Get the db-id from backup info history that corresponds to the archive db-version and db-system-id
            foreach my $iDbIdBackup (keys %{$hDbListBackup})
            {
                if ($$hDbListBackup{$iDbIdBackup}{&INFO_SYSTEM_ID} == $ullDbSysIdArchive &&
                    $$hDbListBackup{$iDbIdBackup}{&INFO_DB_VERSION} eq $strDbVersionArchive)
                {
                    $iDbId = $iDbIdBackup;
                    last;
                }
            }

            my $bArchiveHasBackup = false;
            # If backup db-id corresponding to the archive db-version and db-system-id was found then check to see if there is a
            # current backup associated with this db-version and system-id.
            if (defined($iDbId))
            {
                foreach my $strBackup ($oBackupInfo->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
                {
                    if ($oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID, $iDbId))
                    {
                        $bArchiveHasBackup = true;
                        last;
                    }
                }
            }

# CSHANG Wait - what if an upgrade of the database was performed but a stanza-upgrade was not performed. If someone manually calls expire after a WAL has been pushed to the new archive dir (can this happen?) then would we delete the new WAL? We shouldn't since archive info is required to exist meaning they had to upgrade the stanza or it will error. and if they've upgraded the DB and not performed a stanza-upgrade, but did upgrade the pgbackrest.conf what happens then?
            # If the archive directory is not the current database version and it does not have an associated current backup
            # then this archive directory is no longer needed so delete it
            if (!($bCurrentDb || $bArchiveHasBackup))
            {
                my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveDir);

                remove_tree($strFullPath) > 0
                    or confess &log(ERROR, "unable to remove orphaned ${strFullPath}", ERROR_PATH_REMOVE);

                &log(INFO, "removed orphaned archive path: ${strFullPath}");

                # Delete this archive from the hash
                delete $$hDbListArchive{$iDbIdArchive};
            }
            else
            {
                if (defined($iDbId))
                {
                    # Map the backup db-id to the archiveId
                    $hDbIdArchiveIdMap{$iDbId} =  $strDbVersionArchive . "-" . $iDbIdArchive;
                }
                else
                {
# CSHANG This should never happen unless the backup.info file is corrupt so maybe make this a stronger message?
                    confess &log(ERROR, "the current database ${strDbVersionArchive} is not listed in the backup history\n" .
                        "HINT: has a stanza-upgrade been performed?", ERROR_FILE_INVALID);
                }
            }
        }

        # Regenerate the array list in the event an orphaned directory was removed
        @stryListArchiveDisk = fileList($oFile->pathGet(PATH_BACKUP_ARCHIVE), REGEX_ARCHIVE_DIR_DB_VERSION, 'forward', true);
        
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

            # If a backup has been selected for retention then continue
            if (defined($strArchiveRetentionBackup))
            {
                logDebugMisc($strOperation, "backup selected for retention ${strArchiveRetentionBackup}");
                my $bRemove;

                # Only expire if the selected backup has archive info - backups performed with --no-online will
                # not have archive info and cannot be used for expiration.
                if ($oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT,
                                       $strArchiveRetentionBackup, INFO_BACKUP_KEY_ARCHIVE_START))
                {

                    # For each archiveId, only remove WAL for a retained backup for that archiveId
                    foreach my $strArchiveId (@stryListArchiveDisk)
                    {
                        # Get archive ranges to preserve for this archiveID.  Because archive retention can be less than total
                        # retention it is important to preserve archive that is required to make the older backups consistent even
                        # though they cannot be played any further forward with PITR.
                        my $strArchiveExpireMax;
                        my @oyArchiveRange;

                        foreach my $strBackup ($oBackupInfo->list())
                        {
                            if ($strBackup le $strArchiveRetentionBackup &&
                                $oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_ARCHIVE_START))
                            {
                                # Get the db-id of this current backup
                                my $iBackupDbId =
                                    $oBackupInfo->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID);

                                # If the db-id is mapped to this archive database version directory then build a list of archive
                                # ranges for only the backups to retain that are associated with this archiveId.
                                if ($hDbIdArchiveIdMap{$iBackupDbId} eq $strArchiveId)
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

                                    &log(DETAIL, "archive retention on backup ${strBackup} for ${strArchiveId}, " .
                                        "start = $$oArchiveRange{start}" . (defined($$oArchiveRange{stop}) ? ", stop = " .
                                        "$$oArchiveRange{stop}" : ''));

                                    push(@oyArchiveRange, $oArchiveRange);
                                }
                            }
                        }

                        # Get all major archive paths (timeline and first 64 bits of LSN)
                        foreach my $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, REGEX_ARCHIVE_DIR_WAL))
                        {
                            logDebugMisc($strOperation, "found major WAL path: ${strArchiveId} / ${strPath}");
                            $bRemove = true;

                            # Keep the path if it falls in the range of any backup in retention
                            foreach my $oArchiveRange (@oyArchiveRange)
                            {
                                logDebugMisc($strOperation, "archive retention on range for ${strArchiveId}, start =  $$oArchiveRange{start}" . (defined($$oArchiveRange{stop}) ? ", stop = $$oArchiveRange{stop}" : ''));

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
                                my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

                                remove_tree($strFullPath) > 0
                                    or confess &log(ERROR, "unable to remove ${strFullPath}", ERROR_PATH_REMOVE);

                                # Log expire info
                                logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");
                                $self->logExpire($strPath);
                            }
                            # Else delete individual files instead if the major path is less than or equal to the most recent
                            # retention backup.  This optimization prevents scanning though major paths that could not possibly have
                            # anything to expire.
                            elsif ($strPath le substr($strArchiveExpireMax, 0, 16))
                            {
                                # Look for files in the archive directory
                                foreach my $strSubPath ($oFile->list(PATH_BACKUP_ARCHIVE,
                                                                     "${strArchiveId}/${strPath}", "^[0-F]{24}.*\$"))
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
                                        fileRemove($oFile->pathGet(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSubPath}"));

                                        logDebugMisc($strOperation, "remove WAL segment: ${strSubPath}");

                                        # Log expire info
                                        $self->logExpire(substr($strSubPath, 0, 24));
                                    }
                                    else
                                    {
                                        # Log that the file was not expired
                                        $self->logExpire();
                                    }
                                }
                            }
                        }
                    }

                    # Log if no archive was expired
                    if ($self->{iArchiveExpireTotal} == 0)
                    {
                        &log(DETAIL, 'no archive to remove');
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
