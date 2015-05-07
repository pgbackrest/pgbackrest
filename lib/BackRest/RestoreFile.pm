####################################################################################################################################
# RESTORE FILE MODULE
####################################################################################################################################
package BackRest::RestoreFile;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use File::stat qw(lstat);
use Exporter qw(import);

use lib dirname($0);
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;
use BackRest::Manifest;
use BackRest::File;

####################################################################################################################################
# restoreFile
#
# Restores a single file.
####################################################################################################################################
sub restoreFile
{
    my $oFileHash = shift;          # File to restore
    my $lCopyTimeBegin = shift;     # Time that the backup begain - used for size/timestamp deltas
    my $bDelta = shift;             # Is restore a delta?
    my $bForce = shift;             # Force flag
    my $strBackupPath = shift;      # Backup path
    my $bSourceCompression = shift; # Is the source compressed?
    my $strCurrentUser = shift;     # Current OS user
    my $strCurrentGroup = shift;    # Current OS group
    my $oFile = shift;              # File object
    my $lSizeTotal = shift;         # Total size of files to be restored
    my $lSizeCurrent = shift;       # Current size of files restored

    # Generate destination file name
    my $strDestinationFile = $oFile->path_get(PATH_DB_ABSOLUTE, "$$oFileHash{destination_path}/$$oFileHash{file}");

    # Copy flag and log message
    my $bCopy = true;
    my $strLog;
    $lSizeCurrent += $$oFileHash{size};

    if ($oFile->exists(PATH_DB_ABSOLUTE, $strDestinationFile))
    {
        # Perform delta if requested
        if ($bDelta)
        {
            # If force then use size/timestamp delta
            if ($bForce)
            {
                my $oStat = lstat($strDestinationFile);

                # Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
                if (defined($oStat) && $oStat->size == $$oFileHash{size} &&
                    $oStat->mtime == $$oFileHash{modification_time} && $oStat->mtime < $lCopyTimeBegin)
                {
                    $strLog =  "${strDestinationFile} exists and matches size " . $oStat->size .
                               " and modification time " . $oStat->mtime;
                    $bCopy = false;
                }
            }
            else
            {
                my ($strChecksum, $lSize) = $oFile->hash_size(PATH_DB_ABSOLUTE, $strDestinationFile);

                if ($lSize == $$oFileHash{size} && ($lSize == 0 || $strChecksum eq $$oFileHash{checksum}))
                {
                    $strLog =  "exists and " . ($lSize == 0 ? 'is zero size' : "matches backup");

                    # Even if hash is the same set the time back to backup time.  This helps with unit testing, but also
                    # presents a pristine version of the database after restore.
                    utime($$oFileHash{modification_time}, $$oFileHash{modification_time}, $strDestinationFile)
                        or confess &log(ERROR, "unable to set time for ${strDestinationFile}");

                    $bCopy = false;
                }
            }
        }
    }

    # Copy the file from the backup to the database
    if ($bCopy)
    {
        my ($bCopyResult, $strCopyChecksum, $lCopySize) =
            $oFile->copy(PATH_BACKUP_CLUSTER, (defined($$oFileHash{reference}) ? $$oFileHash{reference} : $strBackupPath) .
                         "/$$oFileHash{source_path}/$$oFileHash{file}" .
                         ($bSourceCompression ? '.' . $oFile->{strCompressExtension} : ''),
                         PATH_DB_ABSOLUTE, $strDestinationFile,
                         $bSourceCompression,   # Source is compressed based on backup settings
                         undef, undef,
                         $$oFileHash{modification_time},
                         $$oFileHash{mode},
                         undef,
                         $$oFileHash{user},
                         $$oFileHash{group});

        if ($lCopySize != 0 && $strCopyChecksum ne $$oFileHash{checksum})
        {
            confess &log(ERROR, "error restoring ${strDestinationFile}: actual checksum ${strCopyChecksum} " .
                                "does not match expected checksum $$oFileHash{checksum}", ERROR_CHECKSUM);
        }

        $strLog = "restore";
    }

    &log(INFO, "${strDestinationFile} ${strLog} (" . file_size_format($$oFileHash{size}) .
               ($lSizeTotal > 0 ? ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%' : '') . ')' .
               ($$oFileHash{size} != 0 ? " checksum $$oFileHash{checksum}" : ''));

    return $lSizeCurrent;
}

our @EXPORT = qw(restoreFile);

1;
