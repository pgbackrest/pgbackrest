####################################################################################################################################
# Posix Storage
#
# Implements storage functions for Posix-compliant file systems.
####################################################################################################################################
package pgBackRestTest::Common::StoragePosix;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;

use pgBackRestTest::Common::StorageBase;
use pgBackRestTest::Common::StoragePosixRead;
use pgBackRestTest::Common::StoragePosixWrite;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant STORAGE_POSIX_DRIVER                                      => __PACKAGE__;
    push @EXPORT, qw(STORAGE_POSIX_DRIVER);

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
        $self->{bFileSync},
        $self->{bPathSync},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'bFileSync', optional => true, default => true},
            {name => 'bPathSync', optional => true, default => true},
        );

    # Set default temp extension
    $self->{strTempExtension} = 'tmp';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# exists - check if a path or file exists
####################################################################################################################################
sub exists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exists', \@_,
            {name => 'strFile', trace => true},
        );

    # Does the path/file exist?
    my $bExists = true;
    my $oStat = lstat($strFile);

    # Use stat to test if file exists
    if (defined($oStat))
    {
        # Check that it is actually a file
        $bExists = !S_ISDIR($oStat->mode) ? true : false;
    }
    else
    {
        # If the error is not entry missing, then throw error
        if (!$OS_ERROR{ENOENT})
        {
            logErrorResult(ERROR_FILE_EXISTS, "unable to test if file '${strFile}' exists", $OS_ERROR);
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

####################################################################################################################################
# info - get information for path/file
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathFile,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strFile', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Stat the path/file
    my $oInfo = lstat($strPathFile);

    # Check for errors
    if (!defined($oInfo))
    {
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to stat '${strPathFile}'", $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oInfo', value => $oInfo, trace => true}
    );
}

####################################################################################################################################
# linkCreate
####################################################################################################################################
sub linkCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathFile,
        $strDestinationLink,
        $bHard,
        $bPathCreate,
        $bIgnoreExists,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkCreate', \@_,
            {name => 'strSourcePathFile', trace => true},
            {name => 'strDestinationLink', trace => true},
            {name => 'bHard', optional=> true, default => false, trace => true},
            {name => 'bPathCreate', optional=> true, default => true, trace => true},
            {name => 'bIgnoreExists', optional => true, default => false, trace => true},
        );

    if (!($bHard ? link($strSourcePathFile, $strDestinationLink) : symlink($strSourcePathFile, $strDestinationLink)))
    {
        my $strMessage = "unable to create link '${strDestinationLink}'";

        # If parent path or source is missing
        if ($OS_ERROR{ENOENT})
        {
            # Check if source is missing
            if (!$self->exists($strSourcePathFile))
            {
                confess &log(ERROR, "${strMessage} because source '${strSourcePathFile}' does not exist", ERROR_FILE_MISSING);
            }

            if (!$bPathCreate)
            {
                confess &log(ERROR, "${strMessage} because parent does not exist", ERROR_PATH_MISSING);
            }

            # Create parent path
            $self->pathCreate(dirname($strDestinationLink), {bIgnoreExists => true, bCreateParent => true});

            # Create link
            $self->linkCreate($strSourcePathFile, $strDestinationLink, {bHard => $bHard});
        }
        # Else if link already exists
        elsif ($OS_ERROR{EEXIST})
        {
            if (!$bIgnoreExists)
            {
                confess &log(ERROR, "${strMessage} because it already exists", ERROR_PATH_EXISTS);
            }
        }
        else
        {
            logErrorResult(ERROR_PATH_CREATE, ${strMessage}, $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# linkDestination - get destination of symlink
####################################################################################################################################
sub linkDestination
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLink,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkDestination', \@_,
            {name => 'strLink', trace => true},
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

####################################################################################################################################
# list - list all files/paths in path
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strPath', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Working variables
    my @stryFileList;
    my $hPath;

    # Attempt to open the path
    if (opendir($hPath, $strPath))
    {
        @stryFileList = grep(!/^(\.|\.\.)$/m, readdir($hPath));
        close($hPath);
    }
    # Else process errors
    else
    {
        # Ignore the error if the file is missing and missing files should be ignored
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to read path '${strPath}'", $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList, ref => true, trace => true}
    );
}

####################################################################################################################################
# manifest - build path/file/link manifest starting with base path and including all subpaths
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $bIgnoreMissing,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPath', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
            {name => 'strFilter', optional => true, trace => true},
        );

    # Generate the manifest
    my $hManifest = {};
    $self->manifestRecurse($strPath, undef, 0, $hManifest, $bIgnoreMissing, $strFilter);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

sub manifestRecurse
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $strSubPath,
        $iDepth,
        $hManifest,
        $bIgnoreMissing,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::manifestRecurse', \@_,
            {name => 'strPath', trace => true},
            {name => 'strSubPath', required => false, trace => true},
            {name => 'iDepth', default => 0, trace => true},
            {name => 'hManifest', required => false, trace => true},
            {name => 'bIgnoreMissing', required => false, default => false, trace => true},
            {name => 'strFilter', required => false, trace => true},
        );

    # Set operation and debug strings
    my $strPathRead = $strPath . (defined($strSubPath) ? "/${strSubPath}" : '');
    my $hPath;

    # If this is the top level stat the path to discover if it is actually a file
    my $oPathInfo = $self->info($strPathRead, {bIgnoreMissing => $bIgnoreMissing});

    if (defined($oPathInfo))
    {
        # If the initial path passed is a file then generate the manifest for just that file
        if ($iDepth == 0 && !S_ISDIR($oPathInfo->mode()))
        {
            $hManifest->{basename($strPathRead)} = $self->manifestStat($strPathRead);
        }
        # Else read as a normal directory
        else
        {
            # Get a list of all files in the path (including .)
            my @stryFileList = @{$self->list($strPathRead, {bIgnoreMissing => $iDepth != 0})};
            unshift(@stryFileList, '.');
            my $hFileStat = $self->manifestList($strPathRead, \@stryFileList, $strFilter);

            # Loop through all subpaths/files in the path
            foreach my $strFile (keys(%{$hFileStat}))
            {
                my $strManifestFile = $iDepth == 0 ? $strFile : ($strSubPath . ($strFile eq qw(.) ? '' : "/${strFile}"));
                $hManifest->{$strManifestFile} = $hFileStat->{$strFile};

                # Recurse into directories
                if ($hManifest->{$strManifestFile}{type} eq 'd' && $strFile ne qw(.))
                {
                    $self->manifestRecurse($strPath, $strManifestFile, $iDepth + 1, $hManifest);
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

sub manifestList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $stryFile,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestList', \@_,
            {name => 'strPath', trace => true},
            {name => 'stryFile', trace => true},
            {name => 'strFilter', required => false, trace => true},
        );

    my $hFileStat = {};

    foreach my $strFile (@{$stryFile})
    {
        if ($strFile ne '.' && defined($strFilter) && $strFilter ne $strFile)
        {
            next;
        }

        $hFileStat->{$strFile} = $self->manifestStat("${strPath}" . ($strFile eq qw(.) ? '' : "/${strFile}"));

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

sub manifestStat
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestStat', \@_,
            {name => 'strFile', trace => true},
        );

    # Stat the path/file, ignoring any that are missing
    my $oStat = $self->info($strFile, {bIgnoreMissing => true});

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
            $hFile->{link_destination} = $self->linkDestination($strFile);
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
# move - move path/file
####################################################################################################################################
sub move
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceFile,
        $strDestinationFile,
        $bPathCreate,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->move', \@_,
            {name => 'strSourceFile', trace => true},
            {name => 'strDestinationFile', trace => true},
            {name => 'bPathCreate', default => false, trace => true},
        );

    # Get source and destination paths
    my $strSourcePathFile = dirname($strSourceFile);
    my $strDestinationPathFile = dirname($strDestinationFile);

    # Move the file
    if (!rename($strSourceFile, $strDestinationFile))
    {
        my $strMessage = "unable to move '${strSourceFile}'";

        # If something is missing determine if it is the source or destination
        if ($OS_ERROR{ENOENT})
        {
            if (!$self->exists($strSourceFile))
            {
                logErrorResult(ERROR_FILE_MISSING, "${strMessage} because it is missing");
            }

            if ($bPathCreate)
            {
                # Attempt to create the path - ignore exists here in case another process creates it first
                $self->pathCreate($strDestinationPathFile, {bCreateParent => true, bIgnoreExists => true});

                # Try move again
                $self->move($strSourceFile, $strDestinationFile);
            }
            else
            {
                logErrorResult(ERROR_PATH_MISSING, "${strMessage} to missing path '${strDestinationPathFile}'");
            }
        }
        # Else raise the error
        else
        {
            logErrorResult(ERROR_FILE_MOVE, "${strMessage} to '${strDestinationFile}'", $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# openRead - open file for reading
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $bIgnoreMissing,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->openRead', \@_,
        {name => 'strFile', trace => true},
        {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
    );

    my $oFileIO = new pgBackRestTest::Common::StoragePosixRead($self, $strFile, {bIgnoreMissing => $bIgnoreMissing});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
    );
}

####################################################################################################################################
# openWrite - open file for writing
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $strMode,
        $strUser,
        $strGroup,
        $lTimestamp,
        $bPathCreate,
        $bAtomic,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->openWrite', \@_,
        {name => 'strFile', trace => true},
        {name => 'strMode', optional => true, trace => true},
        {name => 'strUser', optional => true, trace => true},
        {name => 'strGroup', optional => true, trace => true},
        {name => 'lTimestamp', optional => true, trace => true},
        {name => 'bPathCreate', optional => true, trace => true},
        {name => 'bAtomic', optional => true, trace => true},
    );

    my $oFileIO = new pgBackRestTest::Common::StoragePosixWrite(
        $self, $strFile,
        {strMode => $strMode, strUser => $strUser, strGroup => $strGroup, lTimestamp => $lTimestamp, bPathCreate => $bPathCreate,
            bAtomic => $bAtomic, bSync => $self->{bFileSync}});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
    );
}

####################################################################################################################################
# owner - change ownership of path/file
####################################################################################################################################
sub owner
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFilePath,
        $strUser,
        $strGroup,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->owner', \@_,
            {name => 'strFilePath', trace => true},
            {name => 'strUser', optional => true, trace => true},
            {name => 'strGroup', optional => true, trace => true},
        );

    # Only proceed if user or group was specified
    if (defined($strUser) || defined($strGroup))
    {
        my $strMessage = "unable to set ownership for '${strFilePath}'";
        my $iUserId;
        my $iGroupId;

        # If the user or group is not defined then get it by stat'ing the file.  This is because the chown function requires that
        # both user and group be set.
        my $oStat = $self->info($strFilePath);

        if (!defined($strUser))
        {
            $iUserId = $oStat->uid;
        }

        if (!defined($strGroup))
        {
            $iGroupId = $oStat->gid;
        }

        # Lookup user if specified
        if (defined($strUser))
        {
            $iUserId = getpwnam($strUser);

            if (!defined($iUserId))
            {
                logErrorResult(ERROR_FILE_OWNER, "${strMessage} because user '${strUser}' does not exist");
            }
        }

        # Lookup group if specified
        if (defined($strGroup))
        {
            $iGroupId = getgrnam($strGroup);

            if (!defined($iGroupId))
            {
                logErrorResult(ERROR_FILE_OWNER, "${strMessage} because group '${strGroup}' does not exist");
            }
        }

        # Set ownership on the file if the user or group would be changed
        if ($iUserId != $oStat->uid || $iGroupId != $oStat->gid)
        {
            if (!chown($iUserId, $iGroupId, $strFilePath))
            {
                logErrorResult(ERROR_FILE_OWNER, "${strMessage}", $OS_ERROR);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# pathCreate - create path
####################################################################################################################################
sub pathCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
        $strMode,
        $bIgnoreExists,
        $bCreateParent,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathCreate', \@_,
            {name => 'strPath', trace => true},
            {name => 'strMode', optional => true, default => '0750', trace => true},
            {name => 'bIgnoreExists', optional => true, default => false, trace => true},
            {name => 'bCreateParent', optional => true, default => false, trace => true},
        );

    # Attempt to create the directory
    if (!mkdir($strPath, oct($strMode)))
    {
        my $strMessage = "unable to create path '${strPath}'";

        # If parent path is missing
        if ($OS_ERROR{ENOENT})
        {
            if (!$bCreateParent)
            {
                confess &log(ERROR, "${strMessage} because parent does not exist", ERROR_PATH_MISSING);
            }

            # Create parent path
            $self->pathCreate(dirname($strPath), {strMode => $strMode, bIgnoreExists => true, bCreateParent => $bCreateParent});

            # Create path
            $self->pathCreate($strPath, {strMode => $strMode, bIgnoreExists => true});
        }
        # Else if path already exists
        elsif ($OS_ERROR{EEXIST})
        {
            if (!$bIgnoreExists)
            {
                confess &log(ERROR, "${strMessage} because it already exists", ERROR_PATH_EXISTS);
            }
        }
        else
        {
            logErrorResult(ERROR_PATH_CREATE, ${strMessage}, $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# pathExists - check if path exists
####################################################################################################################################
sub pathExists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathExists', \@_,
            {name => 'strPath', trace => true},
        );

    # Does the path/file exist?
    my $bExists = true;
    my $oStat = lstat($strPath);

    # Use stat to test if path exists
    if (defined($oStat))
    {
        # Check that it is actually a path
        $bExists = S_ISDIR($oStat->mode) ? true : false;
    }
    else
    {
        # If the error is not entry missing, then throw error
        if (!$OS_ERROR{ENOENT})
        {
            logErrorResult(ERROR_FILE_EXISTS, "unable to test if path '${strPath}' exists", $OS_ERROR);
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

####################################################################################################################################
# pathSync - perform fsync on path
####################################################################################################################################
sub pathSync
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPath', trace => true},
        );

    open(my $hPath, "<", $strPath)
        or confess &log(ERROR, "unable to open '${strPath}' for sync", ERROR_PATH_OPEN);
    open(my $hPathDup, ">&", $hPath)
        or confess &log(ERROR, "unable to duplicate '${strPath}' handle for sync", ERROR_PATH_OPEN);

    $hPathDup->sync()
        or confess &log(ERROR, "unable to sync path '${strPath}'", ERROR_PATH_SYNC);

    close($hPathDup);
    close($hPath);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# remove - remove path/file
####################################################################################################################################
sub remove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xstryPathFile,
        $bIgnoreMissing,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'xstryPathFile', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
            {name => 'bRecurse', optional => true, default => false, trace => true},
        );

    # Working variables
    my $bRemoved = true;

    # Remove a tree
    if ($bRecurse)
    {
        my $oManifest = $self->manifest($xstryPathFile, {bIgnoreMissing => true});

        # Iterate all files in the manifest
        foreach my $strFile (sort({$b cmp $a} keys(%{$oManifest})))
        {
            # remove directory
            if ($oManifest->{$strFile}{type} eq 'd')
            {
                my $xstryPathFileRemove = $strFile eq '.' ? $xstryPathFile : "${xstryPathFile}/${strFile}";

                if (!rmdir($xstryPathFileRemove))
                {
                    # Throw error if this is not an ignored missing path
                    if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
                    {
                        logErrorResult(ERROR_PATH_REMOVE, "unable to remove path '${strFile}'", $OS_ERROR);
                    }
                }
            }
            # Remove file
            else
            {
                $self->remove("${xstryPathFile}/${strFile}", {bIgnoreMissing => true});
            }
        }
    }
    # Only remove the specified file
    else
    {
        foreach my $strFile (ref($xstryPathFile) ? @{$xstryPathFile} : ($xstryPathFile))
        {
            if (unlink($strFile) != 1)
            {
                $bRemoved = false;

                # Throw error if this is not an ignored missing file
                if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
                {
                    logErrorResult(
                        $OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to remove file '${strFile}'", $OS_ERROR);
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemoved', value => $bRemoved, trace => true}
    );
}

####################################################################################################################################
# Getters/Setters
####################################################################################################################################
sub className {STORAGE_POSIX_DRIVER}
sub tempExtension {shift->{strTempExtension}}
sub tempExtensionSet {my $self = shift; $self->{strTempExtension} = shift}
sub type {STORAGE_POSIX}

1;
