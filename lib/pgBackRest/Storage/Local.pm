####################################################################################################################################
# Local storage object that uses drivers for file system operations.
####################################################################################################################################
package pgBackRest::Storage::Local;
use parent 'pgBackRest::Storage::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::Filter::Sha;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathBase,
        $oDriver,
        $hRule,
        $bAllowTemp,
        $strTempExtension,
        $strDefaultPathMode,
        $strDefaultFileMode,
        $lBufferMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strPathBase'},
            {name => 'oDriver'},
            {name => 'hRule', optional => true},
            {name => 'bAllowTemp', optional => true, default => true},
            {name => 'strTempExtension', optional => true, default => 'tmp'},
            {name => 'strDefaultPathMode', optional => true, default => '0750'},
            {name => 'strDefaultFileMode', optional => true, default => '0640'},
            {name => 'lBufferMax', optional => true},
        );

    # Create class
    my $self = $class->SUPER::new({lBufferMax => $lBufferMax});
    bless $self, $class;

    $self->{strPathBase} = $strPathBase;
    $self->{oDriver} = $oDriver;
    $self->{hRule} = $hRule;
    $self->{bAllowTemp} = $bAllowTemp;
    $self->{strTempExtension} = $strTempExtension;
    $self->{strDefaultPathMode} = $strDefaultPathMode;
    $self->{strDefaultFileMode} = $strDefaultFileMode;

    # Set temp extension in driver
    if (defined($self->{oDriver}))
    {
        $self->{oDriver}->tempExtensionSet($self->{strTempExtension});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# pathGet
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,                                               # File that that needs to be translated to a path
        $bTemp,                                                    # Return the temp file name
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->pathGet', \@_,
        {name => 'strPathExp', required => false, trace => true},
        {name => 'bTemp', optional => true, default => false, trace => true},
    );

    # Path and file to be returned
    my $strPath;
    my $strFile;

    # Is this an absolute path type?
    my $bAbsolute = false;

    if (defined($strPathExp) && index($strPathExp, qw(/)) == 0)
    {
        $bAbsolute = true;
        $strPath = $strPathExp;
    }
    else
    {
        # Is it a rule type
        if (defined($strPathExp) && index($strPathExp, qw(<)) == 0)
        {
            # Extract the rule type
            my $iPos = index($strPathExp, qw(>));

            if ($iPos == -1)
            {
                confess &log(ASSERT, "found < but not > in '${strPathExp}'");
            }

            my $strType = substr($strPathExp, 0, $iPos + 1);

            # Extract the filename
            if ($iPos < length($strPathExp) - 1)
            {
                $strFile = substr($strPathExp, $iPos + 2);
            }

            # Lookup the rule
            if (!defined($self->{hRule}->{$strType}))
            {
                confess &log(ASSERT, "storage rule '${strType}' does not exist");
            }

            # If rule is a ref then call the function
            if (ref($self->{hRule}->{$strType}))
            {
                $strPath = $self->{strPathBase};
                $strFile = $self->{hRule}{$strType}{fnRule}->($strType, $strFile, $self->{hRule}{$strType}{xData});
            }
            # Else get the path
            else
            {
                $strPath = $self->pathBase() . "/$self->{hRule}->{$strType}";
            }
        }
        # Else it must be relative
        else
        {
            $strPath = $self->{strPathBase};
            $strFile = $strPathExp;
        }
    }

    # Make sure a temp file is valid for this type and file
    if ($bTemp)
    {
        # Only allow temp files for PATH_REPO_ARCHIVE, PATH_SPOOL_ARCHIVE_OUT, PATH_REPO_BACKUP_TMP and any absolute path
        if (!$self->{bAllowTemp})
        {
            confess &log(ASSERT, "temp file not supported for storage '" . $self->pathBase() . "'");
        }

        # The file must be defined
        if (!$bAbsolute)
        {
            if (!defined($strFile))
            {
                confess &log(ASSERT, 'file part must be defined when temp file specified');
            }
        }
    }

    # Combine path and file
    $strPath .= (defined($strFile) ? "/${strFile}" : '');

    # Add temp extension
    $strPath .= $bTemp ? ".$self->{strTempExtension}" : '';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath, trace => true}
    );
}

####################################################################################################################################
# info - stat a file
####################################################################################################################################
sub info
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
            __PACKAGE__ . '::fileStat', \@_,
            {name => 'strFileExp'},
            {name => 'bIgnoreMissing', default => false},
        );

    # Stat the file/path
    my $oInfo = $self->{oDriver}->info($self->pathGet($strFileExp), $bIgnoreMissing);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oInfo', value => $oInfo, trace => true}
    );
}

####################################################################################################################################
# openWrite - open a file for writing.
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
        $strMode,
        $strUser,
        $strGroup,
        $lTimestamp,
        $bAtomic,
        $bPathCreate,
        $rhyFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'strFileExp'},
            {name => 'strMode', optional => true, default => $self->{strDefaultFileMode}},
            {name => 'strUser', optional => true},
            {name => 'strGroup', optional => true},
            {name => 'lTimestamp', optional => true},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
        );

    # Open the file
    my $oFileIo = $self->{oDriver}->openWrite($self->pathGet($strFileExp),
        {strMode => $strMode, strUser => $strUser, strGroup => $strGroup, lTimestamp => $lTimestamp, bPathCreate => $bPathCreate,
            bAtomic => $bAtomic});

    # Apply filters if file is defined
    if (defined($rhyFilter))
    {
        foreach my $rhFilter (reverse(@{$rhyFilter}))
        {
            $oFileIo = $rhFilter->{strClass}->new($oFileIo, @{$rhFilter->{rxyParam}});
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oFileIo, trace => true},
    );
}

####################################################################################################################################
# openRead - open a file for reading.
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
        $bIgnoreMissing,
        $rhyFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'strFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
        );

    # Open the file
    my $oFileIo;

    eval
    {
        $oFileIo = $self->{oDriver}->openRead($self->pathGet($strFileExp));
        return 1;
    }
    # On error check if missing file should be ignored, otherwise error
    or do
    {
        if (exceptionCode($EVAL_ERROR) != ERROR_FILE_MISSING || !$bIgnoreMissing)
        {
            confess $EVAL_ERROR;
        }
    };

    # Apply filters if file is defined
    if (defined($rhyFilter) && defined($oFileIo))
    {
        foreach my $rhFilter (@{$rhyFilter})
        {
            $oFileIo = $rhFilter->{strClass}->new($oFileIo, @{$rhFilter->{rxyParam}});
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oFileIo, trace => true},
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
        $strSourcePathExp,
        $strDestinationPathExp,
        $bHard,
        $bRelative,
        $bPathCreate,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkCreate', \@_,
            {name => 'strSourcePathExp'},
            {name => 'strDestinationPathExp'},
            {name => 'bHard', optional=> true, default => false},
            {name => 'bRelative', optional=> true, default => false},
            {name => 'bPathCreate', optional=> true, default => true},
        );

    # Generate source and destination files
    my $strSource = $self->pathGet($strSourcePathExp);
    my $strDestination = $self->pathGet($strDestinationPathExp);

    # If the destination path is backup and does not exist, create it
    # ??? This should only happen when the link create errors
    if ($bPathCreate)
    {
        $self->pathCreate(dirname($strDestination), {bIgnoreExists => true, bCreateParent => true});
    }

    # Generate relative path if requested
    if ($bRelative)
    {
        # Determine how much of the paths are common
        my @strySource = split('/', $strSource);
        my @stryDestination = split('/', $strDestination);

        while (defined($strySource[0]) && defined($stryDestination[0]) && $strySource[0] eq $stryDestination[0])
        {
            shift(@strySource);
            shift(@stryDestination);
        }

        # Add relative path sections
        $strSource = '';

        for (my $iIndex = 0; $iIndex < @stryDestination - 1; $iIndex++)
        {
            $strSource .= '../';
        }

        # Add path to source
        $strSource .= join('/', @strySource);

        logDebugMisc
        (
            $strOperation, 'apply relative path',
            {name => 'strSource', value => $strSource, trace => true}
        );
    }

    if ($bHard)
    {
        link($strSource, $strDestination)
            or confess &log(ERROR, "unable to create hardlink from ${strSource} to ${strDestination}");
    }
    else
    {
        symlink($strSource, $strDestination)
            or confess &log(ERROR, "unable to create symlink from ${strSource} to ${strDestination}");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# move - move a file
# !!! MOVE TO DRIVER
####################################################################################################################################
sub move
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathExp,
        $strDestinationPathExp,
        $bPathCreate,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->move', \@_,
            {name => 'strSourcePathExp'},
            {name => 'strDestinationPathExp'},
            {name => 'bPathCreate', optional => true, default => false, trace => true},
        );

    # Set operation variables
    $self->{oDriver}->move(
        $self->pathGet($strSourcePathExp), $self->pathGet($strDestinationPathExp), {bPathCreate => $bPathCreate});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pathCreate - create a path
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
            {name => 'strMode', optional => true, default => $self->{strDefaultPathMode}},
            {name => 'bIgnoreExists', optional => true, default => false},
            {name => 'bCreateParent', optional => true, default => false},
        );

    # Create path
    $self->{oDriver}->pathCreate(
        $self->pathGet($strPathExp), {strMode => $strMode, bIgnoreExists => $bIgnoreExists, bCreateParent => $bCreateParent});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pathSync
# !!! MOVE TO DRIVER
####################################################################################################################################
sub pathSync
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $bRecursive,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPathExp'},
            {name => 'bRecursive', default => false},
        );

    # Sync all paths in the tree
    if ($bRecursive)
    {
        my $oManifest = $self->manifest($strPathExp);

        # Iterate all files in the manifest
        foreach my $strFile (sort(keys(%{$oManifest})))
        {
            # Only sync if this is a directory
            if ($oManifest->{$strFile}{type} eq 'd')
            {
                # If current directory
                if ($strFile eq '.')
                {
                    $self->pathSync($strPathExp);
                }
                # Else a subdirectory
                else
                {
                    $self->pathSync("${strPathExp}/${strFile}");
                }
            }
        }
    }
    # Only sync the specified path
    else
    {
        my $strPath = $self->pathGet($strPathExp);

        open(my $hPath, "<", $strPath)
            or confess &log(ERROR, "unable to open '${strPath}' for sync", ERROR_PATH_OPEN);
        open(my $hPathDup, ">&", $hPath)
            or confess &log(ERROR, "unable to duplicate '${strPath}' handle for sync", ERROR_PATH_OPEN);

        $hPathDup->sync
            or confess &log(ERROR, "unable to sync path '${strPath}'", ERROR_PATH_SYNC);

        close($hPathDup);
        close($hPath);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# exists
#
# Checks for the existence of a file, but does not imply that the file is readable/writeable.
#
# Return: true if file exists, false otherwise
####################################################################################################################################
sub exists
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
            __PACKAGE__ . '->exists', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $bExists = $self->{oDriver}->exists($self->pathGet($strPathExp));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
    );
}

####################################################################################################################################
# remove
####################################################################################################################################
sub remove
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
            __PACKAGE__ . '->remove', \@_,
            {name => 'strFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
        );

    # Remove file
    my $bRemoved = $self->{oDriver}->remove($self->pathGet($strFileExp), {bIgnoreMissing => $bIgnoreMissing});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemoved', value => $bRemoved}
    );
}

####################################################################################################################################
# hashSize
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hashSize', \@_,
            {name => 'xFile'},
        );

    # Set operation variables
    my $strHash;
    my $lSize;

    # Is this an IO object or a file expression?
    my $oFileIo = defined($xFile) ? (ref($xFile) ? $xFile : $self->openRead($self->pathGet($xFile))) : undef;

    if (defined($oFileIo))
    {
        $lSize = 0;
        my $oShaIo = new pgBackRest::Storage::Filter::Sha($oFileIo);
        my $lSizeRead;

        do
        {
            my $tContent;
            $lSizeRead = $oShaIo->read(\$tContent, $self->{lBufferMax});
            $lSize += $lSizeRead;
        }
        while ($lSizeRead != 0);

        # Close the file
        $oShaIo->close();

        # Get the hash
        $strHash = $oShaIo->result(STORAGE_FILTER_SHA);
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
# owner
####################################################################################################################################
sub owner
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strUser,
        $strGroup
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->owner', \@_,
            {name => 'strPathExp'},
            {name => 'strUser', required => false},
            {name => 'strGroup', required => false}
        );

    # Set ownership
    $self->{oDriver}->owner($self->pathGet($strPathExp), {strUser => $strUser, strGroup => $strGroup});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# list
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
            {name => 'strPathExp'},
            {name => 'strExpression', optional => true},
            {name => 'strSortOrder', optional => true, default => 'forward'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Get file list
    my $rstryFileList = $self->{oDriver}->list($self->pathGet($strPathExp), {bIgnoreMissing => $bIgnoreMissing});

    # Apply expression if defined
    if (defined($strExpression))
    {
        @{$rstryFileList} = grep(/$strExpression/i, @{$rstryFileList});
    }

    # Reverse sort
    if ($strSortOrder eq 'reverse')
    {
        @{$rstryFileList} = sort {$b cmp $a} @{$rstryFileList};
    }
    # Normal sort
    else
    {
        @{$rstryFileList} = sort @{$rstryFileList};
    }


    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => $rstryFileList}
    );
}

####################################################################################################################################
# manifest
#
# Builds a path/file manifest starting with the base path and including all subpaths.  The manifest contains all the information
# needed to perform a backup or a delta with a previous backup.
####################################################################################################################################
sub manifest
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
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathExp'},
        );

    my $hManifest = $self->{oDriver}->manifest($self->pathGet($strPathExp));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub pathBase {shift->{strPathBase}}

1;
