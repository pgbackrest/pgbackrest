####################################################################################################################################
# Posix storage driver.
####################################################################################################################################
package pgBackRest::Storage::Posix::Driver;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Posix::File;

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
            {name => 'bFileSync', optional => true, default => true, trace => true},
            {name => 'bPathSync', optional => true, default => true, trace => true},
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
# openWrite - open a file for write.
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

    my $oFileIO = new pgBackRest::Storage::Posix::File(
        $self, $strFile, STORAGE_FILE_WRITE,
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
# openRead
####################################################################################################################################
sub openRead
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
        __PACKAGE__ . '->openRead', \@_,
        {name => 'strFile', trace => true},
    );

    my $oFileIO = new pgBackRest::Storage::Posix::File($self, $strFile, STORAGE_FILE_READ);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
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
# pathExists
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
# pathCreate - create a directory
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
# linkCreate
####################################################################################################################################
sub linkCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePath,
        $strDestinationPath,
        $bHard,
        $bPathCreate,
        $bIgnoreExists,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkCreate', \@_,
            {name => 'strSourcePath', trace => true},
            {name => 'strDestinationPath', trace => true},
            {name => 'bHard', optional=> true, default => false, trace => true},
            {name => 'bPathCreate', optional=> true, default => true, trace => true},
            {name => 'bIgnoreExists', optional => true, default => false},
        );

    if (!($bHard ? link($strSourcePath, $strDestinationPath) : symlink($strSourcePath, $strDestinationPath)))
    {
        my $strMessage = "unable to create link '${strDestinationPath}'";

        # If parent path or source is missing
        if ($OS_ERROR{ENOENT})
        {
            # Check if source is missing
            if (!$self->exists($strSourcePath))
            {
                confess &log(ERROR, "${strMessage} because source '${strSourcePath}' does not exist", ERROR_FILE_MISSING);
            }

            if (!$bPathCreate)
            {
                confess &log(ERROR, "${strMessage} because parent does not exist", ERROR_PATH_MISSING);
            }

            # Create parent path
            $self->pathCreate(dirname($strDestinationPath), {bIgnoreExists => true, bCreateParent => true});

            # Create link
            $self->linkCreate($strSourcePath, $strDestinationPath, {bHard => $bHard});
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
# move - move a file.
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
    my $strSourcePath = dirname($strSourceFile);
    my $strDestinationPath = dirname($strDestinationFile);

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
                $self->pathCreate($strDestinationPath, {bCreateParent => true, bIgnoreExists => true});

                # Try move again
                $self->move($strSourceFile, $strDestinationFile);
            }
            else
            {
                logErrorResult(ERROR_PATH_MISSING, "${strMessage} to missing path '${strDestinationPath}'");
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
# owner
####################################################################################################################################
sub owner
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFile,
        $strUser,
        $strGroup,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->owner', \@_,
            {name => 'strFile', trace => true},
            {name => 'strUser', optional => true, trace => true},
            {name => 'strGroup', optional => true, trace => true},
        );

    # Only proceed if user or group was specified
    if (defined($strUser) || defined($strGroup))
    {
        my $strMessage = "unable to set ownership for '${strFile}'";
        my $iUserId;
        my $iGroupId;

        # If the user or group is not defined then get it by stat'ing the file.  This is because the chown function requires that
        # both user and group be set.
        if (!(defined($strUser) && defined($strGroup)))
        {
            my $oStat = $self->info($strFile);

            if (!defined($strUser))
            {
                $iUserId = $oStat->uid;
            }

            if (!defined($strGroup))
            {
                $iGroupId = $oStat->gid;
            }
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

        # Set ownership on the file
        if (!chown($iUserId, $iGroupId, $strFile))
        {
            if ($OS_ERROR{ENOENT})
            {
                logErrorResult(ERROR_FILE_OWNER, "${strMessage} because it is missing");
            }

            logErrorResult(ERROR_FILE_OWNER, "${strMessage}", $OS_ERROR);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# pathSync
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

    $hPathDup->sync
        or confess &log(ERROR, "unable to sync path '${strPath}'", ERROR_PATH_SYNC);

    close($hPathDup);
    close($hPath);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# info - stat a pathfile
####################################################################################################################################
sub info
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
            __PACKAGE__ . '->info', \@_,
            {name => 'strFile', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true},
        );

    # Stat the file/path
    my $oInfo = lstat($strFile);

    # Check for errors
    if (!defined($oInfo))
    {
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to stat '${strFile}'", $OS_ERROR);
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
# remove - remove a file
####################################################################################################################################
sub remove
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
            __PACKAGE__ . '->remove', \@_,
            {name => 'strFile', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Working variables
    my $bRemoved = true;

    # Remove the file
    if (unlink($strFile) != 1)
    {
        $bRemoved = false;

        # If path exists then throw the error
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to remove '${strFile}'", $OS_ERROR);
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
# list - list a directory
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
        @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));
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
# manifest - recursively generate a complete list of all path/links/files in a path
####################################################################################################################################
sub manifest
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
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPath', trace => true},
        );

    # Generate the manifest
    my $hManifest = {};
    $self->manifestRecurse($strPath, undef, 0, $hManifest);

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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::manifestRecurse', \@_,
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
    if ($iDepth == 0 && !S_ISDIR(($self->info($strPathRead))->mode))
    {
        $strFilter = basename($strPathRead);
        $strPathRead = dirname($strPathRead);
    }

    # Get a list of all files in the path (including .)
    my @stryFileList = @{$self->list($strPathRead, {bIgnoreMissing => $iDepth != 0})};
    unshift(@stryFileList, '.');
    my $hFileStat = $self->manifestList($strPathRead, \@stryFileList);

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
            $self->manifestRecurse($strPath, $strManifestFile, $iDepth + 1, $hManifest);
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestList', \@_,
            {name => 'strPath', trace => true},
            {name => 'stryFile', trace => true},
        );

    my $hFileStat = {};

    foreach my $strFile (@{$stryFile})
    {
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
# linkDestination - get the destination of a symlink
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

####################################################################################################################################
# Getters/Setters
####################################################################################################################################
sub tempExtensionSet {my $self = shift; $self->{strTempExtension} = shift}
sub tempExtension {shift->{strTempExtension}}
sub className {STORAGE_POSIX_DRIVER}

1;
