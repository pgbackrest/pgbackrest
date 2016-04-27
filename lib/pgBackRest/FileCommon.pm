####################################################################################################################################
# FILE COMMON    MODULE
####################################################################################################################################
package pgBackRest::FileCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Digest::SHA;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode :flock O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);
use File::Path qw(make_path);
use File::stat;
use IO::Handle;

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

####################################################################################################################################
# fileExists
#
# Check if a path or file exists.
####################################################################################################################################
sub fileExists
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileExists', \@_,
            {name => 'strFile', required => true, trace => true}
        );

    # Working variables
    my $bExists = true;

    # Stat the file/path to determine if it exists
    my $oStat = lstat($strFile);

    # Evaluate error
    if (!defined($oStat))
    {
        my $strError = $!;

        # If the error is not entry missing, then throw error
        if (!$!{ENOENT})
        {
            confess &log(ERROR, "unable to read ${strFile}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OPEN);
        }

        $bExists = false;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists, trace => true}
    );
}

push @EXPORT, qw(fileExists);

####################################################################################################################################
# fileHash
#
# Get the file hash and size.
####################################################################################################################################
sub fileHash
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $bCompressed,
        $strHashType
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileHash', \@_,
            {name => 'strFile', trace => true},
            {name => 'bCompressed', default => false, trace => true},
            {name => 'strHashType', default => 'sha1', trace => true}
        );

    # Working variables
    my ($strHash) = fileHashSize($strFile, $bCompressed, $strHashType);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash, trace => true}
    );
}

push @EXPORT, qw(fileHash);

####################################################################################################################################
# fileHashSize
#
# Get the file hash and size.
####################################################################################################################################
sub fileHashSize
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $bCompressed,
        $strHashType,
        $oProtocol
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileHashSize', \@_,
            {name => 'strFile', trace => true},
            {name => 'bCompressed', default => false, trace => true},
            {name => 'strHashType', default => 'sha1', trace => true},
            {name => 'oProtocol', required => false, trace => true}
        );

    # Working variables
    my $strHash;
    my $iSize = 0;
    my $hFile;

    if (!sysopen($hFile, $strFile, O_RDONLY))
    {
        my $strError = $!;

        # If file exists then throw the error
        if (fileExists($strFile))
        {
            confess &log(ERROR, "unable to open ${strFile}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OPEN);
        }

        confess &log(ERROR, "${strFile} does not exist", ERROR_FILE_MISSING);
    }

    my $oSHA = Digest::SHA->new($strHashType);

    if ($bCompressed)
    {
        # ??? Not crazy about pushing the protocol object in here.  Probably binaryXfer() should be refactored into a standalone
        # function in this file.
        if (!defined($oProtocol))
        {
            confess &log(ASSERT, "oProtocol must be provided to hash compressed file");
        }

        ($strHash, $iSize) =
            $oProtocol->binaryXfer($hFile, 'none', 'in', true, false, false);
    }
    else
    {
        my $iBlockSize;
        my $tBuffer;

        do
        {
            # Read a block from the file
            $iBlockSize = sysread($hFile, $tBuffer, 4194304);

            if (!defined($iBlockSize))
            {
                confess &log(ERROR, "${strFile} could not be read: " . $!, ERROR_FILE_READ);
            }

            $iSize += $iBlockSize;
            $oSHA->add($tBuffer);
        }
        while ($iBlockSize > 0);

        $strHash = $oSHA->hexdigest();
    }

    close($hFile);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash, trace => true},
        {name => 'iSize', value => $iSize, trace => true}
    );
}

push @EXPORT, qw(fileHashSize);

####################################################################################################################################
# fileList
#
# List a directory with filters and ordering.
####################################################################################################################################
sub fileList
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $strExpression,
        $strSortOrder,
        $bIgnoreMissing
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileList', \@_,
            {name => 'strPath', trace => true},
            {name => 'strExpression', required => false, trace => true},
            {name => 'strSortOrder', default => 'forward', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true}
        );

    # Working variables
    my @stryFileList;
    my $hPath;

    # Attempt to open the path
    if (opendir($hPath, $strPath))
    {
        @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));
        close($hPath);

        # Apply expression if defined
        if (defined($strExpression))
        {
            @stryFileList = grep(/$strExpression/i, @stryFileList);
        }

        # Reverse sort
        if ($strSortOrder eq 'reverse')
        {
            @stryFileList = sort {$b cmp $a} @stryFileList;
        }
        # Normal sort
        else
        {
            @stryFileList = sort @stryFileList;
        }
    }
    # Else process errors
    else
    {
        my $strError = $!;

        # If path exists then throw the error
        if (fileExists($strPath))
        {
            confess &log(ERROR, "unable to read ${strPath}" . (defined($strError) ? ": $strError" : ''), ERROR_PATH_OPEN);
        }
        # Else throw an error unless missing paths are ignored
        elsif (!$bIgnoreMissing)
        {
            confess &log(ERROR, "${strPath} does not exist", ERROR_PATH_MISSING);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList, trace => true}
    );
}

push @EXPORT, qw(fileList);

####################################################################################################################################
# fileMove
#
# Move a file.
####################################################################################################################################
sub fileMove
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceFile,
        $strDestinationFile,
        $bDestinationPathCreate
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileMove', \@_,
            {name => 'strSourceFile', trace => true},
            {name => 'strDestinationFile', trace => true},
            {name => 'bDestinationPathCreate', default => false, trace => true}
        );

    # Get source and destination paths
    my $strSourcePath = dirname($strSourceFile);
    my $strDestinationPath = dirname($strDestinationFile);

    # Move the file
    if (!rename($strSourceFile, $strDestinationFile))
    {
        my $strError = $!;
        my $bError = true;

        # If the destination path does not exist and can be created then create it
        if ($bDestinationPathCreate && !fileExists($strDestinationPath))
        {
            $bError = false;

            filePathCreate(dirname($strDestinationFile), undef, true, true);

            # Try the rename again and store the error if it fails
            if (!rename($strSourceFile, $strDestinationFile))
            {
                $strError = $!;
                $bError = true;
            }
        }

        # If there was an error then raise it
        if ($bError)
        {
            confess &log(ERROR, "unable to move file ${strSourceFile} to ${strDestinationFile}" .
                                (defined($strError) ? ": $strError" : ''), ERROR_FILE_MOVE);
        }
    }

    # Always sync the destination directory
    filePathSync(dirname($strDestinationFile));

    # If the source and destination directories are not the same then sync the source directory
    if (dirname($strSourceFile) ne dirname($strDestinationFile))
    {
        filePathSync(dirname($strSourceFile));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

push @EXPORT, qw(fileMove);

####################################################################################################################################
# fileOpen
#
# Open a file.
####################################################################################################################################
sub fileOpen
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $lFlags
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileOpen', \@_,
            {name => 'strFile', trace => true},
            {name => 'lFlags', trace => true}
        );

    my $hFile;

    if (!sysopen($hFile, $strFile, $lFlags))
    {
        my $strError = $!;

        # If file exists then throw the error
        if (fileExists($strFile))
        {
            confess &log(ERROR, "unable to open ${strFile}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OPEN);
        }

        confess &log(ERROR, "${strFile} does not exist", ERROR_FILE_MISSING);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hFile', value => $hFile, trace => true}
    );
}

push @EXPORT, qw(fileOpen);

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
            __PACKAGE__ . '::filePathSync', \@_,
            {name => 'strPath', trace => true}
        );

    open(my $hPath, "<", $strPath)
        or confess &log(ERROR, "unable to open ${strPath}", ERROR_PATH_OPEN);
    open(my $hPathDup, ">&", $hPath)
        or confess &log(ERROR, "unable to duplicate handle for ${strPath}", ERROR_PATH_OPEN);

    $hPathDup->sync
        or confess &log(ERROR, "unable to sync ${strPath}", ERROR_PATH_SYNC);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(filePathSync);

####################################################################################################################################
# fileRemove
#
# Remove a file from the file system.
####################################################################################################################################
sub fileRemove
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $bIgnoreMissing
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileRemove', \@_,
            {name => 'strPath', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true}
        );

    # Working variables
    my $bRemoved = true;

    # Remove the file
    if (unlink($strPath) != 1)
    {
        $bRemoved = false;
        my $strError = $!;

        # If path exists then throw the error
        if (fileExists($strPath))
        {
            confess &log(ERROR, "unable to remove ${strPath}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OPEN);
        }
        # Else throw an error unless missing paths are ignored
        elsif (!$bIgnoreMissing)
        {
            confess &log(ERROR, "${strPath} does not exist", ERROR_FILE_MISSING);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemoved', value => $bRemoved, trace => true}
    );
}

push @EXPORT, qw(fileRemove);


####################################################################################################################################
# fileStat
#
# Stat a file.
####################################################################################################################################
sub fileStat
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileStat', \@_,
            {name => 'strFile', required => true, trace => true}
        );

    # Stat the file/path to determine if it exists
    my $oStat = lstat($strFile);

    # Evaluate error
    if (!defined($oStat))
    {
        my $strError = $!;
        confess &log(ERROR, "unable to read ${strFile}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OPEN);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStat', value => $oStat, trace => true}
    );
}

push @EXPORT, qw(fileStat);

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
            __PACKAGE__ . '::fileStringRead', \@_,
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
            __PACKAGE__ . '::fileStringWrite', \@_,
            {name => 'strFileName', trace => true},
            {name => 'strContent', trace => true},
            {name => 'bSync', default => true, trace => true},
        );

    # Open the file for writing
    sysopen(my $hFile, $strFileName, O_WRONLY | O_CREAT | O_TRUNC, oct(640))
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Write the string
    syswrite($hFile, $strContent)
        or confess &log(ERROR, "unable to write string to ${strFileName}: $!", ERROR_FILE_WRITE);

    # Sync file
    $hFile->sync() if $bSync;

    # Close file
    close($hFile);

    # Sync directory
    filePathSync(dirname($strFileName)) if $bSync;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

push @EXPORT, qw(fileStringWrite);

####################################################################################################################################
# pathAbsolute
#
# Generate an absolute path from an absolute base path and a relative path.
####################################################################################################################################
sub pathAbsolute
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBasePath,
        $strPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::pathAbsolute', \@_,
            {name => 'strBasePath', trace => true},
            {name => 'strPath', trace => true}
        );

    # Working variables
    my $strAbsolutePath;

    # If the path is already absolute
    if (index($strPath, '/') == 0)
    {
        $strAbsolutePath = $strPath;
    }
    # Else make it absolute using the base path
    else
    {
        # Make sure the absolute path is really absolute
        if (index($strBasePath, '/') != 0 || index($strBasePath, '/..') != -1)
        {
            confess &log(ERROR, "${strBasePath} is not an absolute path", ERROR_PATH_TYPE);
        }

        while (index($strPath, '../') == 0)
        {
            $strBasePath = dirname($strBasePath);
            $strPath = substr($strPath, 3);
        }

        $strAbsolutePath = "${strBasePath}/${strPath}";
    }

    # Make sure the result is really an absolute path
    if (index($strAbsolutePath, '/') != 0 || index($strAbsolutePath, '/..') != -1)
    {
        confess &log(ERROR, "result ${strAbsolutePath} was not an absolute path", ERROR_PATH_TYPE);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strAbsolutePath', value => $strAbsolutePath, trace => true}
    );
}

push @EXPORT, qw(pathAbsolute);

####################################################################################################################################
# filePathCreate
#
# Create a path.
####################################################################################################################################
sub filePathCreate
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $strMode,
        $bIgnoreExists,
        $bCreateParents
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::filePathCreate', \@_,
            {name => 'strPath', trace => true},
            {name => 'strMode', default => '0750', trace => true},
            {name => 'bIgnoreExists', default => false, trace => true},
            {name => 'bCreateParents', default => false, trace => true}
        );

    if (!($bIgnoreExists && fileExists($strPath)))
    {
        # Attempt to create the directory
        my $stryError;

        if (!$bCreateParents && !fileExists(dirname($strPath)))
        {
            confess &log(ERROR, "unable to create ${strPath} because parent path does not exist", ERROR_PATH_CREATE);
        }

        make_path($strPath, {mode => oct($strMode), error => \$stryError});

        # Throw any errrors that were returned
        if (@$stryError)
        {
            my ($strErrorPath, $strErrorMessage) = %{@$stryError[0]};
            confess &log(ERROR, "unable to create ${strPath}: ${strErrorMessage}", ERROR_PATH_CREATE);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

push @EXPORT, qw(filePathCreate);

1;
