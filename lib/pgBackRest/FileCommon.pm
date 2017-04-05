####################################################################################################################################
# FILE COMMON MODULE
####################################################################################################################################
package pgBackRest::FileCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode :flock O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname basename);
use File::Path qw(make_path);
use File::stat;
use IO::Handle;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Version;

####################################################################################################################################
# Default modes
####################################################################################################################################
my $strPathModeDefault = '0750';
my $strFileModeDefault = '0640';

####################################################################################################################################
# Compression extension
####################################################################################################################################
use constant COMPRESS_EXT                                           => 'gz';
    push @EXPORT, qw(COMPRESS_EXT);

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
# fileLinkDestination
#
# Return the destination of a symlink.
####################################################################################################################################
sub fileLinkDestination
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLink,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileLinkDestination', \@_,
            {name => 'strFile', trace => true},
        );

    # Get link destination
    my $strLinkDestination = readlink($strLink);

    # Check for errors
    if (!defined($strLinkDestination))
    {
        logErrorResult(
            $OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to get destination for link ${strLink}", $OS_ERROR);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strLinkDestination', value => $strLinkDestination, trace => true}
    );
}

push @EXPORT, qw(fileLinkDestination);

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
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileList', \@_,
            {name => 'strPath', trace => true},
            {name => 'strExpression', optional => true, trace => true},
            {name => 'strSortOrder', optional => true, default => 'forward', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
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

        # Ignore the error is the file is missing and missing files should be ignored
        if (!($!{ENOENT} && $bIgnoreMissing))
        {
            confess &log(ERROR, "unable to read ${strPath}" . (defined($strError) ? ": $strError" : ''), ERROR_PATH_OPEN);
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
# fileManifest
#
# Generate a complete list of all directories/links/files in a directory/subdirectories.
####################################################################################################################################
sub fileManifest
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileManifest', \@_,
            {name => 'strPath', trace => true},
        );

    # Generate the manifest
    my $hManifest = {};
    fileManifestRecurse($strPath, undef, 0, $hManifest);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

push @EXPORT, qw(fileManifest);

sub fileManifestRecurse
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $strSubPath,
        $iDepth,
        $hManifest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileManifestRecurse', \@_,
            {name => 'strPath', trace => true},
            {name => 'strSubPath', required => false, trace => true},
            {name => 'iDepth', default => 0, trace => true},
            {name => 'hManifest', required => false, trace => true},
        );

    # Set operation and debug strings
    my $strPathRead = $strPath . (defined($strSubPath) ? "/${strSubPath}" : '');
    my $hPath;
    my $strFilter;

    # If this is the top level stat the path to discover if it is actually a file
    if ($iDepth == 0 && !S_ISDIR((fileStat($strPathRead))->mode))
    {
        $strFilter = basename($strPathRead);
        $strPathRead = dirname($strPathRead);
    }

    # Get a list of all files in the path (including .)
    my @stryFileList = fileList($strPathRead, {bIgnoreMissing => $iDepth != 0});
    unshift(@stryFileList, '.');
    my $hFileStat = fileManifestList($strPathRead, \@stryFileList);

    # Loop through all subpaths/files in the path
    foreach my $strFile (keys(%{$hFileStat}))
    {
        # Skip this file if it does not match the filter
        if (defined($strFilter) && $strFile ne $strFilter)
        {
            next;
        }

        my $strManifestFile = $iDepth == 0 ? $strFile : ($strSubPath . ($strFile eq qw(.) ? '' : "/${strFile}"));
        $hManifest->{$strManifestFile} = $hFileStat->{$strFile};

        # Recurse into directories
        if ($hManifest->{$strManifestFile}{type} eq 'd' && $strFile ne qw(.))
        {
            fileManifestRecurse($strPath, $strManifestFile, $iDepth + 1, $hManifest);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

sub fileManifestList
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $stryFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileManifestList', \@_,
            {name => 'strPath', trace => true},
            {name => 'stryFile', trace => true},
        );

    my $hFileStat = {};

    foreach my $strFile (@{$stryFile})
    {
        $hFileStat->{$strFile} = fileManifestStat("${strPath}" . ($strFile eq qw(.) ? '' : "/${strFile}"));

        if (!defined($hFileStat->{$strFile}))
        {
            delete($hFileStat->{$strFile});
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hFileStat', value => $hFileStat, trace => true}
    );
}

sub fileManifestStat
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileManifestRecurse', \@_,
            {name => 'strFile', trace => true},
        );

    # Stat the path/file, ignoring any that are missing
    my $oStat = fileStat($strFile, true);

    # Generate file data if stat succeeded (i.e. file exists)
    my $hFile;

    if (defined($oStat))
    {
        # Check for regular file
        if (S_ISREG($oStat->mode))
        {
            $hFile->{type} = 'f';

            # Get size
            $hFile->{size} = $oStat->size;

            # Get modification time
            $hFile->{modification_time} = $oStat->mtime;
        }
        # Check for directory
        elsif (S_ISDIR($oStat->mode))
        {
            $hFile->{type} = 'd';
        }
        # Check for link
        elsif (S_ISLNK($oStat->mode))
        {
            $hFile->{type} = 'l';
            $hFile->{link_destination} = fileLinkDestination($strFile);
        }
        # Not a recognized type
        else
        {
            confess &log(ERROR, "${strFile} is not of type directory, file, or link", ERROR_FILE_INVALID);
        }

        # Get user name
        $hFile->{user} = getpwuid($oStat->uid);

        # Get group name
        $hFile->{group} = getgrgid($oStat->gid);

        # Get mode
        if ($hFile->{type} ne 'l')
        {
            $hFile->{mode} = sprintf('%04o', S_IMODE($oStat->mode));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hFile', value => $hFile, trace => true}
    );
}

####################################################################################################################################
# fileMode
#
# Set the file mode.
####################################################################################################################################
sub fileMode
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $strMode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileMode', \@_,
            {name => 'strFile', trace => true},
            {name => 'strMode', default => $strFileModeDefault, trace => true},
        );

    # Change mode
    if(!chmod(oct($strMode), $strFile))
    {
        my $strError = $!;

        # If file exists then throw the error
        if (fileExists($strFile))
        {
            confess &log(ERROR, "unable to chmod ${strFile}" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_MODE);
        }

        confess &log(ERROR, "${strFile} does not exist", ERROR_FILE_MISSING);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(fileMode);

####################################################################################################################################
# fileModeDefaultSet
#
# Set the default mode to be used when creating files.
####################################################################################################################################
sub fileModeDefaultSet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strMode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileModeDefaultSet', \@_,
            {name => 'strMode', trace => true},
        );

    $strFileModeDefault = $strMode;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(fileModeDefaultSet);

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
        $bDestinationPathCreate,
        $bPathSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileMove', \@_,
            {name => 'strSourceFile', trace => true},
            {name => 'strDestinationFile', trace => true},
            {name => 'bDestinationPathCreate', default => false, trace => true},
            {name => 'bPathSync', default => false, trace => true},
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

            filePathCreate(dirname($strDestinationFile), undef, true, true, $bPathSync);

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

    # Sync path(s) if requested
    if ($bPathSync)
    {
        # Always sync the destination directory
        filePathSync(dirname($strDestinationFile));

        # If the source and destination directories are not the same then sync the source directory
        if (dirname($strSourceFile) ne dirname($strDestinationFile))
        {
            filePathSync(dirname($strSourceFile));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $lFlags,
        $strMode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileOpen', \@_,
            {name => 'strFile', trace => true},
            {name => 'lFlags', trace => true},
            {name => 'strMode', default => $strFileModeDefault, trace => true},
        );

    my $hFile;

    if (!sysopen($hFile, $strFile, $lFlags, oct($strMode)))
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
        or confess &log(ERROR, "unable to open '${strPath}' for sync", ERROR_PATH_OPEN);
    open(my $hPathDup, ">&", $hPath)
        or confess &log(ERROR, "unable to duplicate '${strPath}' handle for sync", ERROR_PATH_OPEN);

    $hPathDup->sync
        or confess &log(ERROR, "unable to sync '${strPath}'", ERROR_PATH_SYNC);

    close($hPathDup);
    close($hPath);

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
        $bIgnoreMissing,
        $bPathSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileRemove', \@_,
            {name => 'strPath', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true},
            {name => 'bPathSync', default => false, trace => true},
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

    # Sync parent directory if requested
    if ($bRemoved && $bPathSync)
    {
        filePathSync(dirname($strPath));
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
        $strFile,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileStat', \@_,
            {name => 'strFile', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true},
        );

    # Stat the file/path
    my $oStat = lstat($strFile);

    # Check for errors
    if (!defined($oStat))
    {
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to stat ${strFile}", $OS_ERROR);
        }
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
# Create/overwrite a file with a string.
####################################################################################################################################
sub fileStringWrite
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $strContent,
        $bSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileStringWrite', \@_,
            {name => 'strFileName', trace => true},
            {name => 'strContent', trace => true, required => false},
            {name => 'bSync', default => true, trace => true},
        );

    # Generate temp filename
    my $strFileNameTemp = $strFileName . '.' . BACKREST_EXE . '.tmp';

    # Open file for writing
    my $hFile = fileOpen($strFileNameTemp, O_WRONLY | O_CREAT | O_TRUNC, $strFileModeDefault);

    # Write content
    if (defined($strContent) && length($strContent) > 0)
    {
        syswrite($hFile, $strContent)
            or confess &log(ERROR, "unable to write string to ${strFileName}: $!", ERROR_FILE_WRITE);
    }

    # Sync file
    $hFile->sync() if $bSync;

    # Close file
    close($hFile);

    # Move file to final location
    fileMove($strFileNameTemp, $strFileName, undef, $bSync);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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

        while (index($strPath, '..') == 0)
        {
            $strBasePath = dirname($strBasePath);
            $strPath = substr($strPath, 2);

            if (index($strPath, '/') == 0)
            {
                $strPath = substr($strPath, 1);
            }
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
# pathModeDefaultSet
#
# Set the default mode to be used when creating paths.
####################################################################################################################################
sub pathModeDefaultSet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strMode
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::pathModeDefaultSet', \@_,
            {name => 'strMode', trace => true},
        );

    $strPathModeDefault = $strMode;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(pathModeDefaultSet);

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
        $bCreateParents,
        $bPathSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::filePathCreate', \@_,
            {name => 'strPath', trace => true},
            {name => 'strMode', default => $strPathModeDefault, trace => true},
            {name => 'bIgnoreExists', default => false, trace => true},
            {name => 'bCreateParents', default => false, trace => true},
            {name => 'bPathSync', default => false, trace => true},
        );

    # Determine if parent directory exists
    my $strParentPath = dirname($strPath);

    if (!fileExists($strParentPath))
    {
        if (!$bCreateParents)
        {
            confess &log(ERROR, "unable to create ${strPath} because parent path does not exist", ERROR_PATH_CREATE);
        }

        filePathCreate($strParentPath, $strMode, $bIgnoreExists, $bCreateParents, $bPathSync);
    }

    # Create the path
    if (!mkdir($strPath, oct($strMode)))
    {
        # Get the error as a string
        my $strError = $OS_ERROR . '';

        # Error if not ignoring existence of the path
        if (!($bIgnoreExists && fileExists($strPath)))
        {
            confess &log(ERROR, "unable to create ${strPath}: " . $OS_ERROR, ERROR_PATH_CREATE);
        }
    }

    # Sync path if requested
    if ($bPathSync)
    {
        filePathSync($strParentPath);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(filePathCreate);

1;
