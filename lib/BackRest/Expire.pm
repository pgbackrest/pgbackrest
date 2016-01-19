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
use BackRest::Common::Log;
use BackRest::BackupCommon;
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

            system("rm -rf ${strBackupClusterPath}/${strBackup}") == 0
                or confess &log(ERROR, "unable to delete backup ${strBackup}");
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
            @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
        {
            @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true, true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
        {
            @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true, true, true), 'reverse');
        }
        else
        {
            confess &log(ERROR, "unknown archive_retention_type '$strArchiveRetentionType'");
        }

        # Make sure that iArchiveRetention is set and valid
        if (!defined($iArchiveRetention))
        {
            confess &log(ERROR, 'archive_retention must be set if archive_retention_type is set');
        }

        if (!looks_like_number($iArchiveRetention) || $iArchiveRetention < 1)
        {
            confess &log(ERROR, 'archive_retention must be a number >= 1');
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

            if (defined($strArchiveRetentionBackup))
            {
                my $strArchiveExpireStart;
                my $strArchiveExpireStop;

                # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this
                # backup even though they are also in the pg_xlog directory (since they have been copied more than once).
                my $oManifest = new BackRest::Manifest($oFile->pathGet(PATH_BACKUP_CLUSTER) .
                                                       "/${strArchiveRetentionBackup}/backup.manifest");

                if ($oManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START))
                {
                    my $strArchiveLast = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START);

                    &log(INFO, "archive retention from backup ${strArchiveRetentionBackup}, start = ${strArchiveLast}");

                    # Get archive info
                    my $oArchive = new BackRest::Archive();
                    my $strArchiveId = $oArchive->getCheck($oFile);

                    # Remove any archive directories or files that are out of date
                    foreach $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, "^[0-F]{16}\$"))
                    {
                        logDebugMisc($strOperation, "found major WAL path: ${strPath}");

                        # If less than first 16 characters of current archive file, then remove the directory
                        if ($strPath lt substr($strArchiveLast, 0, 16))
                        {
                            my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

                            remove_tree($strFullPath) > 0 or confess &log(ERROR, "unable to remove ${strFullPath}");

                            logDebugMisc($strOperation, "remove major WAL path: ${strFullPath}");

                            # Record expire start and stop location for info
                            if (!defined($strArchiveExpireStart))
                            {
                                $strArchiveExpireStart = $strPath;
                            }

                            $strArchiveExpireStop = $strPath;
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
                                    unlink($oFile->pathGet(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSubPath}"))
                                        or confess &log(ERROR, 'unable to remove ' . $strSubPath);

                                    logDebugMisc($strOperation, "remove expired WAL segment: ${strSubPath}");

                                    # Record expire start and stop location for info
                                    if (!defined($strArchiveExpireStart))
                                    {
                                        $strArchiveExpireStart = $strSubPath;
                                    }

                                    $strArchiveExpireStop = $strSubPath;
                                }
                            }
                        }
                    }

                    if (!defined($strArchiveExpireStart))
                    {
                        &log(INFO, 'no WAL segments to expire');
                    }
                    else
                    {
                        &log(INFO, 'expire WAL segments: start = ' . substr($strArchiveExpireStart, 0, 24) .
                                   ', stop = ' . substr($strArchiveExpireStop, 0, 24));
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
