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
    $self->driver()->tempExtensionSet($self->{strTempExtension}) if $self->driver()->can('tempExtensionSet');

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
        # Error when temp files are not allowed
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
    my $oInfo = $self->driver()->info($self->pathGet($strFileExp), $bIgnoreMissing);

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
    my $oFileIo = $self->driver()->openWrite($self->pathGet($strFileExp),
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
        $oFileIo = $self->driver()->openRead($self->pathGet($strFileExp));
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
        $bIgnoreExists,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkCreate', \@_,
            {name => 'strSourcePathExp'},
            {name => 'strDestinationPathExp'},
            {name => 'bHard', optional=> true, default => false},
            {name => 'bRelative', optional=> true, default => false},
            {name => 'bPathCreate', optional=> true, default => true},
            {name => 'bIgnoreExists', optional => true, default => false},
        );

    # Get source and destination paths
    my $strSourcePath = $self->pathGet($strSourcePathExp);
    my $strDestinationPath = $self->pathGet($strDestinationPathExp);

    # Generate relative path if requested
    if ($bRelative)
    {
        # Determine how much of the paths are common
        my @strySource = split('/', $strSourcePath);
        my @stryDestination = split('/', $strDestinationPath);

        while (defined($strySource[0]) && defined($stryDestination[0]) && $strySource[0] eq $stryDestination[0])
        {
            shift(@strySource);
            shift(@stryDestination);
        }

        # Add relative path sections
        $strSourcePath = '';

        for (my $iIndex = 0; $iIndex < @stryDestination - 1; $iIndex++)
        {
            $strSourcePath .= '../';
        }

        # Add path to source
        $strSourcePath .= join('/', @strySource);

        logDebugMisc
        (
            $strOperation, 'apply relative path',
            {name => 'strSourcePath', value => $strSourcePath, trace => true}
        );
    }

    # Create the link
    $self->driver()->linkCreate(
        $strSourcePath, $strDestinationPath, {bHard => $bHard, bPathCreate => $bPathCreate, bIgnoreExists => $bIgnoreExists});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# move - move a file
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
    $self->driver()->move(
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
    $self->driver()->pathCreate(
        $self->pathGet($strPathExp), {strMode => $strMode, bIgnoreExists => $bIgnoreExists, bCreateParent => $bCreateParent});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
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
        $strPathExp,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPathExp'},
            {name => 'bRecurse', default => false},
        );

    # Sync all paths in the tree
    if ($bRecurse)
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
        $self->driver()->pathSync($self->pathGet($strPathExp));
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
        $strFileExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exists', \@_,
            {name => 'strFileExp'},
        );

    # Check exists
    my $bExists = $self->driver()->exists($self->pathGet($strFileExp));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
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
        $strPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathExists', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $bExists = $self->driver()->pathExists($self->pathGet($strPathExp));

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
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'strFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
            {name => 'bRecurse', optional => true, default => false, trace => true},
        );

    # Remove file
    my $bRemoved = $self->driver()->remove($self->pathGet($strFileExp), {bIgnoreMissing => $bIgnoreMissing, bRecurse => $bRecurse});

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
    $self->driver()->owner($self->pathGet($strPathExp), {strUser => $strUser, strGroup => $strGroup});

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
    my $rstryFileList = $self->driver()->list($self->pathGet($strPathExp), {bIgnoreMissing => $bIgnoreMissing});

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

    my $hManifest = $self->driver()->manifest($self->pathGet($strPathExp));

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
sub driver {shift->{oDriver}}

1;
