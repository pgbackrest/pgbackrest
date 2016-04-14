####################################################################################################################################
# RESTORE FILE MODULE
####################################################################################################################################
package pgBackRest::RestoreFile;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::stat qw(lstat);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Manifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_RESTORE_FILE                                        => 'RestoreFile';

use constant OP_RESTORE_FILE_RESTORE_FILE                           => OP_RESTORE_FILE . '::restoreFile';

####################################################################################################################################
# restoreFile
#
# Restores a single file.
####################################################################################################################################
sub restoreFile
{
    my $oFileHash = shift;          # File to restore

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $lCopyTimeBegin,                            # Time that the backup begain - used for size/timestamp deltas
        $bDelta,                                    # Is restore a delta?
        $bForce,                                    # Force flag
        $strBackupPath,                             # Backup path
        $bSourceCompression,                        # Is the source compressed?
        $strCurrentUser,                            # Current OS user
        $strCurrentGroup,                           # Current OS group
        $oFile,                                     # File object
        $lSizeTotal,                                # Total size of files to be restored
        $lSizeCurrent                               # Current size of files restored
    ) =
        logDebugParam
        (
            OP_RESTORE_FILE_RESTORE_FILE, \@_,
            {name => 'lCopyTimeBegin', trace => true},
            {name => 'bDelta', trace => true},
            {name => 'bForce', trace => true},
            {name => 'strBackupPath', trace => true},
            {name => 'bSourceCompression', trace => true},
            {name => 'strCurrentUser', trace => true},
            {name => 'strCurrentGroup', trace => true},
            {name => 'oFile', trace => true},
            {name => 'lSizeTotal', trace => true},
            {name => 'lSizeCurrent', trace => true}
        );

    # Generate destination file name
    my $strDestinationFile = $oFile->pathGet(PATH_DB_ABSOLUTE, "$$oFileHash{destination_path}/$$oFileHash{file}");

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
                    $strLog =  'exists and matches size ' . $oStat->size . ' and modification time ' . $oStat->mtime;
                    $bCopy = false;
                }
            }
            else
            {
                my ($strChecksum, $lSize) = $oFile->hashSize(PATH_DB_ABSOLUTE, $strDestinationFile);

                if ($lSize == $$oFileHash{size} && ($lSize == 0 || $strChecksum eq $$oFileHash{checksum}))
                {
                    $strLog =  'exists and ' . ($lSize == 0 ? 'is zero size' : "matches backup");

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
    }

    &log(INFO, "restore file ${strDestinationFile}" . (defined($strLog) ? " - ${strLog}" : '') .
               ' (' . fileSizeFormat($$oFileHash{size}) .
               ($lSizeTotal > 0 ? ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%' : '') . ')' .
               ($$oFileHash{size} != 0 ? " checksum $$oFileHash{checksum}" : ''));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSizeCurrent', value => $lSizeCurrent, trace => true}
    );
}

push @EXPORT, qw(restoreFile);

1;
