####################################################################################################################################
# RESTORE FILE MODULE
####################################################################################################################################
package pgBackRest::RestoreFile;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode);
use File::Basename qw(dirname);
use File::stat qw(lstat);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

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
