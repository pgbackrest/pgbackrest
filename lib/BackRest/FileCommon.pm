####################################################################################################################################
# FILE COMMON    MODULE
####################################################################################################################################
package BackRest::FileCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode :flock O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);
use IO::Handle;

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Log;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_FILE_COMMON                                         => 'FileCommon';

use constant OP_FILE_COMMON_PATH_SYNC                               => OP_FILE_COMMON . '::filePathSync';
use constant OP_FILE_COMMON_STRING_READ                             => OP_FILE_COMMON . '::fileStringRead';
use constant OP_FILE_COMMON_STRING_WRITE                            => OP_FILE_COMMON . '::fileStringWrite';

####################################################################################################################################
# filePathSync
#
# Sync a directory.
####################################################################################################################################
sub filePathSync
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath
    ) =
        logDebugParam
        (
            OP_FILE_COMMON_PATH_SYNC, \@_,
            {name => 'strPath', trace => true}
        );

    open(my $hPath, "<", $strPath)
        or confess &log(ERROR, "unable to open ${strPath}", ERROR_PATH_OPEN);
    open(my $hPathDup, ">&", $hPath)
        or confess &log(ERROR, "unable to duplicate handle for ${strPath}", ERROR_PATH_OPEN);

    $hPathDup->sync
        or confess &log(ERROR, "unable to sync ${strPath}", ERROR_PATH_SYNC);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

push @EXPORT, qw(filePathSync);

####################################################################################################################################
# fileStringRead
#
# Read the specified file as a string.
####################################################################################################################################
sub fileStringRead
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName
    ) =
        logDebugParam
        (
            OP_FILE_COMMON_STRING_READ, \@_,
            {name => 'strFileName', trace => true}
        );

    # Open the file for writing
    sysopen(my $hFile, $strFileName, O_RDONLY)
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Read the string
    my $iBytesRead;
    my $iBytesTotal = 0;
    my $strContent;

    do
    {
        $iBytesRead = sysread($hFile, $strContent, 65536, $iBytesTotal);

        if (!defined($iBytesRead))
        {
            confess &log(ERROR, "unable to read string from ${strFileName}: $!", ERROR_FILE_READ);
        }

        $iBytesTotal += $iBytesRead;
    }
    while ($iBytesRead != 0);

    close($hFile);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strContent', value => $strContent, trace => true}
    );
}

push @EXPORT, qw(fileStringRead);

####################################################################################################################################
# fileStringWrite
#
# Write a string to the specified file.
####################################################################################################################################
sub fileStringWrite
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $strContent,
        $bSync
    ) =
        logDebugParam
        (
            OP_FILE_COMMON_STRING_WRITE, \@_,
            {name => 'strFileName', trace => true},
            {name => 'strContent', trace => true},
            {name => 'bSync', default => true, trace => true},
        );

    # Open the file for writing
    sysopen(my $hFile, $strFileName, O_WRONLY | O_CREAT | O_TRUNC, 0640)
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Write the string
    syswrite($hFile, $strContent)
        or confess &log(ERROR, "unable to write string to ${strFileName}: $!", ERROR_FILE_WRITE);

    # Sync and close ini file
    if ($bSync)
    {
        $hFile->sync();
        filePathSync(dirname($strFileName));
    }

    close($hFile);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

push @EXPORT, qw(fileStringWrite);

1;
