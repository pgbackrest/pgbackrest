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
use BackRest::Config;
use BackRest::File;
use BackRest::Manifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_EXPIRE                                              => 'Expire';

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
    (
        my $strOperation,
        $self->{oFile}
    ) =
        logDebugParam
        (
            OP_EXPIRE_NEW, \@_,
            {name => 'oFile'}
        );

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
            confess &log(ERROR, 'full_retention must be a number >= 1');
        }

        my $iIndex = $iFullRetention;
        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true), 'reverse');

        while (defined($stryPath[$iIndex]))
        {
            # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
            # be consistent if the process dies
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, '^' . $stryPath[$iIndex] . '.*', 'reverse'))
            {
                system("rm -rf ${strBackupClusterPath}/${strPath}") == 0
                    or confess &log(ERROR, "unable to delete backup ${strPath}");

                $oBackupInfo->delete($strPath);
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
            confess &log(ERROR, 'differential_retention must be a number >= 1');
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(false, true), 'reverse');

        if (defined($stryPath[$iDifferentialRetention - 1]))
        {
            logDebugMisc($strOperation,  'differential expiration based on ' . $stryPath[$iDifferentialRetention - 1]);

            # Get a list of all differential and incremental backups
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(false, true, true), 'reverse'))
            {
                logDebugMisc($strOperation, "checking ${strPath} for differential expiration");

                # Remove all differential and incremental backups before the oldest valid differential
                if ($strPath lt $stryPath[$iDifferentialRetention - 1])
                {
                    system("rm -rf ${strBackupClusterPath}/${strPath}") == 0
                        or confess &log(ERROR, "unable to delete backup ${strPath}");
                    $oBackupInfo->delete($strPath);

                    &log(INFO, "remove expired diff/incr backup ${strPath}");
                }
            }
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
            if (!defined($iArchiveRetention))
            {
                $iArchiveRetention = $iFullRetention;
            }

            @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backupRegExpGet(true), 'reverse');
        }
        elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
        {
            if (!defined($iArchiveRetention))
            {
                $iArchiveRetention = $iDifferentialRetention;
            }

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

        # if no backups were found then preserve current archive logs - too scary to delete them!
        my $iBackupTotal = scalar @stryPath;

        if ($iBackupTotal > 0)
        {
            # See if enough backups exist for expiration to start
            my $strArchiveRetentionBackup = $stryPath[$iArchiveRetention - 1];

            if (!defined($strArchiveRetentionBackup))
            {
                if ($strArchiveRetentionType eq BACKUP_TYPE_FULL && scalar @stryPath > 0)
                {
                    &log(INFO, 'fewer than required backups for retention, ' .
                               'but since archive_retention_type = full using oldest full backup');
                    $strArchiveRetentionBackup = $stryPath[scalar @stryPath - 1];
                }
            }

            if (defined($strArchiveRetentionBackup))
            {
                # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this
                # backup even though they are also in the pg_xlog directory (since they have been copied more than once).
                &log(INFO, 'archive retention based on backup ' . $strArchiveRetentionBackup);

                my $oManifest = new BackRest::Manifest($oFile->pathGet(PATH_BACKUP_CLUSTER) .
                                                       "/${strArchiveRetentionBackup}/backup.manifest");
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
                    logDebugMisc($strOperation, "found major archive path: ${strPath}");

                    # If less than first 16 characters of current archive file, then remove the directory
                    if ($strPath lt substr($strArchiveLast, 0, 16))
                    {
                        my $strFullPath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId) . "/${strPath}";

                        remove_tree($strFullPath) > 0 or confess &log(ERROR, "unable to remove ${strFullPath}");

                        logDebugMisc($strOperation, "remove major archive path: ${strFullPath}");
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

                                logDebugMisc($strOperation, "remove expired archive file: ${strSubPath}");
                            }
                        }
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
