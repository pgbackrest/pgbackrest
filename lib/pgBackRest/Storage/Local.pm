####################################################################################################################################
# Local Storage
#
# Implements storage functionality using drivers.
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
# exists - check if file exists
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
# hashSize - calculate sha1 hash and size of file
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hashSize', \@_,
            {name => 'xFileExp'},
        );

    # Set operation variables
    my $strHash;
    my $lSize;

    # Is this an IO object or a file expression?
    my $oFileIo = defined($xFileExp) ? (ref($xFileExp) ? $xFileExp : $self->openRead($self->pathGet($xFileExp))) : undef;

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
# info - get information for path/file
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
    my $oInfo = $self->driver()->info($self->pathGet($strPathFileExp), {bIgnoreMissing => $bIgnoreMissing});

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

    # Create the link
    $self->driver()->linkCreate(
        $strSourcePathFile, $strDestinationLink, {bHard => $bHard, bPathCreate => $bPathCreate, bIgnoreExists => $bIgnoreExists});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
    my $rstryFileList = $self->driver()->list(
        $self->pathGet($strPathExp), {strExpression => $strExpression, bIgnoreMissing => $bIgnoreMissing});

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
# manifest - build path/file/link manifest starting with base path and including all subpaths
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
# move - move path/file
####################################################################################################################################
sub move
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathFileExp,
        $strDestinationPathFileExp,
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
        $self->pathGet($strSourcePathFileExp), $self->pathGet($strDestinationPathFileExp), {bPathCreate => $bPathCreate});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
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
        $xFileExp,
        $bIgnoreMissing,
        $rhyFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
        );

    # Need to push this down to drivers so errors do not appear in the log
    my $oFileIo = $self->driver()->openRead($self->pathGet($xFileExp), {bIgnoreMissing => $bIgnoreMissing});

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
# openWrite - open file for writing
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'xFileExp'},
            {name => 'strMode', optional => true, default => $self->{strDefaultFileMode}},
            {name => 'strUser', optional => true},
            {name => 'strGroup', optional => true},
            {name => 'lTimestamp', optional => true},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
        );

    # Open the file
    my $oFileIo = $self->driver()->openWrite($self->pathGet($xFileExp),
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
# owner - change ownership of path/file
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

    # Set ownership
    $self->driver()->owner($self->pathGet($strPathFileExp), {strUser => $strUser, strGroup => $strGroup});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
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
# pathExists - check if path exists
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
# pathGet - resolve a path expression into an absolute path
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
                $strPath = $self->pathBase();
                $strFile = $self->{hRule}{$strType}{fnRule}->($strType, $strFile, $self->{hRule}{$strType}{xData});
            }
            # Else get the path
            else
            {
                $strPath = $self->pathBase() . ($self->pathBase() =~ /\/$/ ? '' : qw{/}) . $self->{hRule}->{$strType};
            }
        }
        # Else it must be relative
        else
        {
            $strPath = $self->pathBase();
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
    $strPath .= defined($strFile) ? ($strPath =~ /\/$/ ? '' : qw{/}) . "${strFile}" : '';

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
# pathSync - perform driver sync operation on path
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

    $self->driver()->pathSync($self->pathGet($strPathExp), {bRecurse => true});

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
        $xstryPathFileExp,
        $bIgnoreMissing,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'xstryPathFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
            {name => 'bRecurse', optional => true, default => false, trace => true},
        );

    # Evaluate expressions for all files
    my @stryPathFileExp;

    if (ref($xstryPathFileExp))
    {
        foreach my $strPathFileExp (@{$xstryPathFileExp})
        {
            push(@stryPathFileExp, $self->pathGet($strPathFileExp));
        }
    }

    # Remove path(s)/file(s)
    my $bRemoved = $self->driver()->remove(
        ref($xstryPathFileExp) ? \@stryPathFileExp : $self->pathGet($xstryPathFileExp),
        {bIgnoreMissing => $bIgnoreMissing, bRecurse => $bRecurse});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemoved', value => $bRemoved}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub pathBase {shift->{strPathBase}}
sub driver {shift->{oDriver}}

1;
