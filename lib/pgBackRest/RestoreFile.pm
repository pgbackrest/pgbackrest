####################################################################################################################################
# RESTORE FILE MODULE
####################################################################################################################################
package pgBackRest::RestoreFile;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);
use File::stat qw(lstat);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;

####################################################################################################################################
# restoreFile
#
# Restores a single file.
####################################################################################################################################
sub restoreFile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,                                     # File object
        $strDbFile,
        $lSize,
        $lModificationTime,
        $strChecksum,
        $bZero,
        $bForce,                                    # Force flag
        $strRepoFile,
        $strReference,
        $strMode,
        $strUser,
        $strGroup,
        $lCopyTimeStart,                            # Backup start time - used for size/timestamp deltas
        $bDelta,                                    # Is restore a delta?
        $strBackupPath,                             # Backup path
        $bSourceCompressed,                         # Is the source compressed?
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::restoreFile', \@_,
            {name => 'oFile', trace => true},
            {name => 'strDbFile', trace => true},
            {name => 'lSize', trace => true},
            {name => 'lModificationTime', trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'bZero', required => false, default => false, trace => true},
            {name => 'bForce', trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'strReference', required => false, trace => true},
            {name => 'strMode', trace => true},
            {name => 'strUser', trace => true},
            {name => 'strGroup', trace => true},
            {name => 'lCopyTimeStart', trace => true},
            {name => 'bDelta', trace => true},
            {name => 'strBackupPath', trace => true},
            {name => 'bSourceCompressed', trace => true},
        );

    # Copy flag and log message
    my $bCopy = true;

    if ($bZero)
    {
        $bCopy = false;

        # Open the file truncating to zero bytes in case it already exists
        my $hFile = fileOpen($strDbFile, O_WRONLY | O_CREAT | O_TRUNC, $strMode);

        # Now truncate to the original size.  This will create a sparse file which is very efficient for this use case.
        truncate($hFile, $lSize);

        # Sync the file
        $hFile->sync()
            or confess &log(ERROR, "unable to sync ${strDbFile}", ERROR_FILE_SYNC);

        # Close the file
        close($hFile)
            or confess &log(ERROR, "unable to close ${strDbFile}", ERROR_FILE_CLOSE);

        # Fix the timestamp - not really needed in this case but good for testing
        utime($lModificationTime, $lModificationTime, $strDbFile)
            or confess &log(ERROR, "unable to set time for ${strDbFile}");

        # Set file ownership
        $oFile->owner(PATH_DB_ABSOLUTE, $strDbFile, $strUser, $strGroup);
    }
    elsif ($oFile->exists(PATH_DB_ABSOLUTE, $strDbFile))
    {
        # Perform delta if requested
        if ($bDelta)
        {
            # If force then use size/timestamp delta
            if ($bForce)
            {
                my $oStat = lstat($strDbFile);

                # Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
                if (defined($oStat) && $oStat->size == $lSize &&
                    $oStat->mtime == $lModificationTime && $oStat->mtime < $lCopyTimeStart)
                {
                    $bCopy = false;
                }
            }
            else
            {
                my ($strActualChecksum, $lActualSize) = $oFile->hashSize(PATH_DB_ABSOLUTE, $strDbFile);

                if ($lActualSize == $lSize && ($lSize == 0 || $strActualChecksum eq $strChecksum))
                {
                    # Even if hash is the same set the time back to backup time.  This helps with unit testing, but also
                    # presents a pristine version of the database after restore.
                    utime($lModificationTime, $lModificationTime, $strDbFile)
                        or confess &log(ERROR, "unable to set time for ${strDbFile}");

                    $bCopy = false;
                }
            }
        }
    }

    # Copy the file from the backup to the database
    if ($bCopy)
    {
        my ($bCopyResult, $strCopyChecksum, $lCopySize) = $oFile->copy(
            PATH_BACKUP_CLUSTER, (defined($strReference) ? $strReference : $strBackupPath) .
                "/${strRepoFile}" . ($bSourceCompressed ? '.' . $oFile->{strCompressExtension} : ''),
            PATH_DB_ABSOLUTE, $strDbFile,
            $bSourceCompressed,
            undef, undef,
            $lModificationTime, $strMode,
            undef,
            $strUser, $strGroup);

        if ($lCopySize != 0 && $strCopyChecksum ne $strChecksum)
        {
            confess &log(ERROR, "error restoring ${strDbFile}: actual checksum ${strCopyChecksum} " .
                                "does not match expected checksum ${strChecksum}", ERROR_CHECKSUM);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bCopy', value => $bCopy, trace => true}
    );
}

push @EXPORT, qw(restoreFile);

####################################################################################################################################
# restoreLog
#
# Log a restored file.
####################################################################################################################################
sub restoreLog
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iLocalId,
        $strDbFile,
        $lSize,
        $lModificationTime,
        $strChecksum,
        $bZero,
        $bForce,
        $bCopy,
        $lSizeTotal,
        $lSizeCurrent,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::restoreLog', \@_,
            {name => 'iLocalId', required => false},
            {name => 'strDbFile'},
            {name => 'lSize'},
            {name => 'lModificationTime'},
            {name => 'strChecksum', required => false},
            {name => 'bZero', required => false, default => false},
            {name => 'bForce'},
            {name => 'bCopy'},
            {name => 'lSizeTotal'},
            {name => 'lSizeCurrent'},
        );

    # If the file was not copied then create a log entry to explain why
    my $strLog;

    if (!$bCopy && !$bZero)
    {
        if ($bForce)
        {
            $strLog =  'exists and matches size ' . $lSize . ' and modification time ' . $lModificationTime;
        }
        else
        {
            $strLog =  'exists and ' . ($lSize == 0 ? 'is zero size' : 'matches backup');
        }
    }

    # Log the restore
    $lSizeCurrent += $lSize;

    &log($bCopy ? INFO : DETAIL,
         'restore' . ($bZero ? ' zeroed' : '') .
         " file ${strDbFile}" . (defined($strLog) ? " - ${strLog}" : '') .
         ' (' . fileSizeFormat($lSize) .
         ($lSizeTotal > 0 ? ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%' : '') . ')' .
         ($lSize != 0 && !$bZero ? " checksum ${strChecksum}" : ''), undef, undef, undef, $iLocalId);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSizeCurrent', value => $lSizeCurrent, trace => true}
    );
}

push @EXPORT, qw(restoreLog);

1;
