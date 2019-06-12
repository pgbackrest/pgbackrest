####################################################################################################################################
# C Storage Interface
####################################################################################################################################
package pgBackRest::Storage::Storage;
use parent 'pgBackRest::Storage::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::StorageRead;
use pgBackRest::Storage::StorageWrite;

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
        $self->{strType},
        $self->{lBufferMax},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strType'},
            {name => 'lBufferMax', optional => true, default => 65536},
            {name => 'strDefaultPathMode', optional => true, default => '0750'},
            {name => 'strDefaultFileMode', optional => true, default => '0640'},
        );

    # Create C storage object
    $self->{oStorageC} = new pgBackRest::LibC::Storage($self->{strType});

    # Get encryption settings
    if ($self->{strType} eq '<REPO>')
    {
        $self->{strCipherType} = $self->{oStorageC}->cipherType();
        $self->{strCipherPass} = $self->{oStorageC}->cipherPass();
    }

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Check if file exists (not a path)
####################################################################################################################################
sub exists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exists', \@_,
            {name => 'strFileExp'},
        );

    # Check exists
    my $bExists = $self->{oStorageC}->exists($strFileExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists ? true : false}
    );
}

####################################################################################################################################
# Read a buffer from storage all at once
####################################################################################################################################
sub get
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFile,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->get', \@_,
            {name => 'xFile', required => false, trace => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or
    # if the repo has a user passphrase defined - else pass undef
    my $oFileIo = defined($xFile) ? (ref($xFile) ? $xFile : $self->openRead($xFile, {strCipherPass => $strCipherPass})) : undef;

    # Get the file contents
    my $bEmpty = false;
    my $tContent = $self->{oStorageC}->get($oFileIo->{oStorageCRead});

    if (defined($tContent) && length($tContent) == 0)
    {
        $tContent = undef;
        $bEmpty = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => defined($tContent) || $bEmpty ? \$tContent : undef, trace => true},
    );
}

####################################################################################################################################
# Calculate sha1 hash and size of file. If special encryption settings are required, then the file objects from openRead/openWrite
# must be passed instead of file names.
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hashSize', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Set operation variables
    my $strHash;
    my $lSize;

    # Is this an IO object or a file expression?
    my $oFileIo = ref($xFileExp) ? $xFileExp : $self->openRead($self->pathGet($xFileExp), {bIgnoreMissing => $bIgnoreMissing});

    # Add size and sha filters
    $oFileIo->{oStorageCRead}->filterAdd(COMMON_IO_HANDLE, undef);
    $oFileIo->{oStorageCRead}->filterAdd(STORAGE_FILTER_SHA, undef);

    # Read the file and set results if it exists
    if ($self->{oStorageC}->readDrain($oFileIo->{oStorageCRead}))
    {
        $strHash = $oFileIo->result(STORAGE_FILTER_SHA);
        $lSize = $oFileIo->result(COMMON_IO_HANDLE);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash},
        {name => 'lSize', value => $lSize}
    );
}

####################################################################################################################################
# Get information for path/file
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::fileStat', \@_,
            {name => 'strPathFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Stat the path/file
    my $oInfo = lstat($self->pathGet($strPathFileExp));

    # Check for errors
    if (!defined($oInfo))
    {
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult(
                $OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN,
                "unable to stat '" . $self->pathGet($strPathFileExp) . "'", $OS_ERROR);
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
# linkCreate - create a link
####################################################################################################################################
sub linkCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathFileExp,
        $strDestinationLinkExp,
        $bHard,
        $bRelative,
        $bPathCreate,
        $bIgnoreExists,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkCreate', \@_,
            {name => 'strSourcePathFileExp'},
            {name => 'strDestinationLinkExp'},
            {name => 'bHard', optional=> true, default => false},
            {name => 'bRelative', optional=> true, default => false},
            {name => 'bPathCreate', optional=> true, default => true},
            {name => 'bIgnoreExists', optional => true, default => false},
        );

    # Get source and destination paths
    my $strSourcePathFile = $self->pathGet($strSourcePathFileExp);
    my $strDestinationLink = $self->pathGet($strDestinationLinkExp);

    # Generate relative path if requested
    if ($bRelative)
    {
        # Determine how much of the paths are common
        my @strySource = split('/', $strSourcePathFile);
        my @stryDestination = split('/', $strDestinationLink);

        while (defined($strySource[0]) && defined($stryDestination[0]) && $strySource[0] eq $stryDestination[0])
        {
            shift(@strySource);
            shift(@stryDestination);
        }

        # Add relative path sections
        $strSourcePathFile = '';

        for (my $iIndex = 0; $iIndex < @stryDestination - 1; $iIndex++)
        {
            $strSourcePathFile .= '../';
        }

        # Add path to source
        $strSourcePathFile .= join('/', @strySource);

        logDebugMisc
        (
            $strOperation, 'apply relative path',
            {name => 'strSourcePathFile', value => $strSourcePathFile, trace => true}
        );
    }

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
# List all files/paths in path
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strExpression,
        $strSortOrder,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strPathExp', required => false},
            {name => 'strExpression', optional => true},
            {name => 'strSortOrder', optional => true, default => 'forward'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Get file list
    my $rstryFileList = [];
    my $strFileList = $self->{oStorageC}->list($strPathExp, $bIgnoreMissing, $strSortOrder eq 'forward', $strExpression);

    if (defined($strFileList) && $strFileList ne '[]')
    {
        $rstryFileList = $self->{oJSON}->decode($strFileList);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => $rstryFileList}
    );
}

####################################################################################################################################
# Build path/file/link manifest starting with base path and including all subpaths
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathExp'},
            {name => 'strFilter', optional => true, trace => true},
        );

    my $hManifest = $self->{oJSON}->decode($self->{oStorageC}->manifest($strPathExp, $strFilter));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
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
        $strSourceFileExp,
        $strDestinationFileExp,
        $bPathCreate,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->move', \@_,
            {name => 'strSourceFileExp'},
            {name => 'strDestinationFileExp'},
        );

    # Get source and destination paths
    my $strSourceFile = $self->pathGet($strSourceFileExp);
    my $strDestinationFile = $self->pathGet($strDestinationFileExp);

    # Move the file
    if (!rename($strSourceFile, $strDestinationFile))
    {
        logErrorResult(ERROR_FILE_MOVE, "unable to move '${strSourceFile}' to '${strDestinationFile}'", $OS_ERROR);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Open file for reading
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $bIgnoreMissing,
        $rhyFilter,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Open the file
    my $oFileIo = new pgBackRest::LibC::StorageRead($self->{oStorageC}, $xFileExp, $bIgnoreMissing);

    # If cipher is set then decryption is the first filter applied to the read
    if (defined($self->cipherType()))
    {
        $oFileIo->filterAdd(STORAGE_FILTER_CIPHER_BLOCK, $self->{oJSON}->encode([false, $self->cipherType(), $strCipherPass]));
    }

    # Apply any other filters
    if (defined($rhyFilter))
    {
        foreach my $rhFilter (@{$rhyFilter})
        {
            $oFileIo->filterAdd(
                $rhFilter->{strClass}, defined($rhFilter->{rxyParam}) ? $self->{oJSON}->encode($rhFilter->{rxyParam}) : undef);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => new pgBackRest::Storage::StorageRead($self, $oFileIo), trace => true},
    );
}

####################################################################################################################################
# Open file for writing
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $strMode,
        $strUser,
        $strGroup,
        $lTimestamp,
        $bAtomic,
        $bPathCreate,
        $rhyFilter,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'xFileExp'},
            {name => 'strMode', optional => true, default => $self->{strDefaultFileMode}},
            {name => 'strUser', optional => true},
            {name => 'strGroup', optional => true},
            {name => 'lTimestamp', optional => true, default => '0'},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Open the file
    my $oFileIo = new pgBackRest::LibC::StorageWrite(
        $self->{oStorageC}, $xFileExp, oct($strMode), $strUser, $strGroup, $lTimestamp, $bAtomic, $bPathCreate);

    # Apply any other filters
    if (defined($rhyFilter))
    {
        foreach my $rhFilter (@{$rhyFilter})
        {
            $oFileIo->filterAdd(
                $rhFilter->{strClass}, defined($rhFilter->{rxyParam}) ? $self->{oJSON}->encode($rhFilter->{rxyParam}) : undef);
        }
    }

    # If cipher is set then encryption is the last filter applied to the write
    if (defined($self->cipherType()))
    {
        $oFileIo->filterAdd(STORAGE_FILTER_CIPHER_BLOCK, $self->{oJSON}->encode([true, $self->cipherType(), $strCipherPass]));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => new pgBackRest::Storage::StorageWrite($self, $oFileIo), trace => true},
    );
}

####################################################################################################################################
# Change ownership of path/file
####################################################################################################################################
sub owner
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathFileExp,
        $strUser,
        $strGroup
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->owner', \@_,
            {name => 'strPathFileExp'},
            {name => 'strUser', required => false},
            {name => 'strGroup', required => false}
        );

    # Only proceed if user or group was specified
    if (defined($strUser) || defined($strGroup))
    {
        my $strPathFile = $self->pathGet($strPathFileExp);
        my $strMessage = "unable to set ownership for '${strPathFile}'";
        my $iUserId;
        my $iGroupId;

        # If the user or group is not defined then get it by stat'ing the file.  This is because the chown function requires that
        # both user and group be set.
        my $oStat = $self->info($strPathFile);

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
            if (!chown($iUserId, $iGroupId, $strPathFile))
            {
                logErrorResult(ERROR_FILE_OWNER, "${strMessage}", $OS_ERROR);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# Generate an absolute path from an absolute base path and a relative path
####################################################################################################################################
sub pathAbsolute
{
    my $self = shift;

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

####################################################################################################################################
# Create a path
####################################################################################################################################
sub pathCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strMode,
        $bIgnoreExists,
        $bCreateParent,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathCreate', \@_,
            {name => 'strPathExp'},
            {name => 'strMode', optional => true},
            {name => 'bIgnoreExists', optional => true, default => false},
            {name => 'bCreateParent', optional => true, default => false},
        );

    # Create path
    $self->{oStorageC}->pathCreate($strPathExp, $strMode, $bIgnoreExists, $bCreateParent);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# Check if path exists
####################################################################################################################################
sub pathExists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathExists', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $bExists = $self->{oStorageC}->pathExists($strPathExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists ? true : false}
    );
}

####################################################################################################################################
# Resolve a path expression into an absolute path
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathGet', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $strPath = $self->{oStorageC}->pathGet($strPathExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath, trace => true}
    );
}

####################################################################################################################################
# Remove path and all files below it
####################################################################################################################################
sub pathRemove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $bIgnoreMissing,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathRemove', \@_,
            {name => 'strPathExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
            {name => 'bRecurse', optional => true, default => false},
        );

    $self->{oStorageC}->pathRemove($strPathExp, $bIgnoreMissing, $bRecurse);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Sync path so newly added file entries are not lost
####################################################################################################################################
sub pathSync
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPathExp'},
        );

    $self->{oStorageC}->pathSync($strPathExp);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# put - writes a buffer out to storage all at once
####################################################################################################################################
sub put
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFile,
        $xContent,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->put', \@_,
            {name => 'xFile', trace => true},
            {name => 'xContent', required => false, trace => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), trace => true, redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or if
    # the repo has a user passphrase defined - else pass undef
    my $oFileIo = ref($xFile) ? $xFile : $self->openWrite($xFile, {strCipherPass => $strCipherPass});

    # Write the content
    my $lSize = $self->{oStorageC}->put($oFileIo->{oStorageCWrite}, ref($xContent) ? $$xContent : $xContent);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSize', value => $lSize, trace => true},
    );
}

####################################################################################################################################
# Remove file
####################################################################################################################################
sub remove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
        );

    foreach my $strFileExp (ref($xFileExp) ? @{$xFileExp} : ($xFileExp))
    {
        $self->{oStorageC}->remove($strFileExp, $bIgnoreMissing);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# encrypted - determine if the file is encrypted or not
####################################################################################################################################
sub encrypted
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->encrypted', \@_,
            {name => 'strFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    my $bEncrypted = false;

    # Open the file via the driver
    # !!! NEED TO ADD LENGTH BACK, length(CIPHER_MAGIC));
    my $tBuffer = $self->get(
        new pgBackRest::Storage::StorageRead(
            $self, new pgBackRest::LibC::StorageRead($self->{oStorageC}, $strFileExp, $bIgnoreMissing)));

    # If the file does not exist because we're ignoring missing (else it would error before this is executed) then determine if it
    # should be encrypted based on the repo
    if (!defined($tBuffer))
    {
        if (defined($self->cipherType()))
        {
            $bEncrypted = true;
        }
    }
    elsif (defined($$tBuffer) && substr($$tBuffer, 0, length(CIPHER_MAGIC)) eq CIPHER_MAGIC)
    {
        $bEncrypted = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bEncrypted', value => $bEncrypted}
    );
}

####################################################################################################################################
# encryptionValid - determine if encyption set properly based on the value passed
####################################################################################################################################
sub encryptionValid
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bEncrypted,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->encryptionValid', \@_,
            {name => 'bEncrypted'},
        );

    my $bValid = ($bEncrypted && defined($self->cipherType())) || (!$bEncrypted && !defined($self->cipherType()));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bValid', value => $bValid ? true : false}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub capability {shift->type() eq 'posix'}
sub type {shift->{oStorageC}->type()}
sub cipherType {shift->{strCipherType}}
sub cipherPassUser {shift->{strCipherPass}}

1;
