####################################################################################################################################
# EXPIRE MODULE
####################################################################################################################################
package BackRest::Expire;

use threads;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);
use File::Path qw(remove_tree);
use Scalar::Util qw(looks_like_number);

use lib dirname($0);
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::BackupCommon;
use BackRest::BackupInfo;
use BackRest::Config::Config;
use BackRest::File;
use BackRest::Manifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_EXPIRE                                              => 'Expire';

use constant OP_EXPIRE_DESTROY                                      => OP_EXPIRE . '->DESTROY';
use constant OP_EXPIRE_NEW                                          => OP_EXPIRE . '->new';
use constant OP_EXPIRE_PROCESS                                      => OP_EXPIRE . '->process';

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
    my ($strOperation) = logDebugParam(OP_EXPIRE_NEW);

    # Initialize file object
    $self->{oFile} = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        protocolGet()
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
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_EXPIRE_DESTROY
    );

    undef($self->{oFile});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
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
            &log(INFO, 'remove archive: start = ' . substr($self->{strArchiveExpireStart}, 0, 24) .
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
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_EXPIRE_PROCESS
        );

    my $strPath;
    my @stryPath;

    my $oFile = $self->{oFile};
    my $strBackupClusterPath = $oFile->pathGet(PATH_BACKUP_CLUSTER);
    my $iFullRetention = optionGet(OPTION_RETENTION_FULL, false);
    my $iDifferentialRetention = optionGet(OPTION_RETENTION_DIFF, false);
    my $strArchiveRetentionType = optionGet(OPTION_RETENTION_ARCHIVE_TYPE, false);
    my $iArchiveRetention = optionGet(OPTION_RETENTION_ARCHIVE, false);

    # Load or build backup.info
    my $oBackupInfo = new BackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));

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

                foreach $strPath ($oBackupInfo->list('^' . $stryPath[$iFullIdx] . '.*'))
                {
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

        @stryPath = $oBackupInfo->list(backupRegExpGet(false, true));

        if (@stryPath > $iDifferentialRetention)
        {
            for (my $iDiffIdx = 0; $iDiffIdx < @stryPath - $iDifferentialRetention; $iDiffIdx++)
            {
                # Get a list of all differential and incremental backups
                my @stryRemoveList;

                foreach $strPath ($oBackupInfo->list(backupRegExpGet(false, true, true)))
                {
                    logDebugMisc($strOperation, "checking ${strPath} for differential expiration");

                    # Remove all differential and incremental backups before the oldest valid differential
                    if ($strPath lt $stryPath[$iDiffIdx + 1])
                    {
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

    # Default archive retention if not explicily set
    if (defined($iFullRetention))
    {
        if (!defined($iArchiveRetention))
        {
            $iArchiveRetention = $iFullRetention;
        }

        if (!defined($strArchiveRetentionType))
        {
            $strArchiveRetentionType = BACKUP_TYPE_FULL;
        }
    }

    # If no archive retention type is set then exit
    if (!defined($strArchiveRetentionType))
    {
        &log(INFO, 'archive retention type not set - archive logs will not be expired');
    }
    else
    {
        # Determine which backup type to use for archive retention (full, differential, incremental)
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

        # if no backups were found then preserve current archive logs - too soon to expire them
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
                my $bRemove;

                # Only expire if the selected backup has archive info - backups performed with --no-online will
                # not have archive info and cannot be used for expiration.
                if ($oBackupInfo->test(INFO_BACKUP_SECTION_BACKUP_CURRENT,
                                       $strArchiveRetentionBackup, INFO_BACKUP_KEY_ARCHIVE_START))
                {
                    # Get archive info
                    my $oArchive = new BackRest::Archive();
                    my $strArchiveId = $oArchive->getCheck($oFile);

                    my $strArchiveExpireMax;

                    # Get archive ranges to preserve.  Because archive retention can be less than total retention it is
                    # important to preserve older archive that is required to make the older backups consistent even though
                    # they cannot be played any further forward with PITR.
                    my @oyArchiveRange;

                    foreach my $strBackup ($oBackupInfo->list())
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

                            &log(INFO, "archive retention on backup ${strBackup}, start = $$oArchiveRange{start}" .
                                 (defined($$oArchiveRange{stop}) ? ", stop = $$oArchiveRange{stop}" : ''));

                            push(@oyArchiveRange, $oArchiveRange);
                        }
                    }

                    # Get all major archive paths (timeline and first 64 bits of LSN)
                    foreach $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, "^[0-F]{16}\$"))
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
                            my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

                            remove_tree($strFullPath) > 0
                                or confess &log(ERROR, "unable to remove ${strFullPath}", ERROR_PATH_REMOVE);

                            # Log expire info
                            logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");
                            $self->logExpire($strPath);
                        }
                        # Else delete individual files instead if the major path is less than or equal to the most recent retention
                        # backup.  This optimization prevents scanning though major paths that could not possibly have anything to
                        # expire.
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
                                    unlink($oFile->pathGet(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSubPath}"))
                                        or confess &log(ERROR, 'unable to remove ' . $strSubPath, ERROR_FILE_REMOVE);

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

                    # Log if no archive was expired
                    if ($self->{iArchiveExpireTotal} == 0)
                    {
                        &log(INFO, 'no archive to remove');
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

1;
