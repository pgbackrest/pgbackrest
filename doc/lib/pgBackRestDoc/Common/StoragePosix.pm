####################################################################################################################################
# Posix Storage
#
# Implements storage functions for Posix-compliant file systems.
####################################################################################################################################
package pgBackRestDoc::Common::StoragePosix;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;

use pgBackRestDoc::Common::StorageBase;
use pgBackRestDoc::Common::StoragePosixRead;
use pgBackRestDoc::Common::StoragePosixWrite;

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

    my $oFileIO = new pgBackRestDoc::Common::StoragePosixRead($self, $strFile, {bIgnoreMissing => $bIgnoreMissing});

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
        $lTimestamp,
        $bPathCreate,
        $bAtomic,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->openWrite', \@_,
        {name => 'strFile', trace => true},
        {name => 'strMode', optional => true, trace => true},
        {name => 'lTimestamp', optional => true, trace => true},
        {name => 'bPathCreate', optional => true, trace => true},
        {name => 'bAtomic', optional => true, trace => true},
    );

    my $oFileIO = new pgBackRestDoc::Common::StoragePosixWrite(
        $self, $strFile,
        {strMode => $strMode, lTimestamp => $lTimestamp, bPathCreate => $bPathCreate, bAtomic => $bAtomic,
            bSync => $self->{bFileSync}});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIO', value => $oFileIO, trace => true},
    );
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
sub tempExtension {shift->{strTempExtension}}
sub tempExtensionSet {my $self = shift; $self->{strTempExtension} = shift}

1;
