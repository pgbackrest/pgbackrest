####################################################################################################################################
# Implements storage functionality using drivers.
####################################################################################################################################
package pgBackRestDoc::Common::Storage;
use parent 'pgBackRestDoc::Common::StorageBase';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestDoc::Common::StorageBase;

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
# get - reads a buffer from storage all at once
####################################################################################################################################
sub get
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
            __PACKAGE__ . '->get', \@_,
            {name => 'xFile', required => false, trace => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or
    # if the repo has a user passphrase defined - else pass undef
    my $oFileIo = defined($xFile) ? (ref($xFile) ? $xFile : $self->openRead($xFile)) : undef;

    # Read only if there is something to read from
    my $tContent;
    my $lSize = 0;

    if (defined($oFileIo))
    {
        my $lSizeRead;

        do
        {
            $lSizeRead = $oFileIo->read(\$tContent, $self->{lBufferMax});
            $lSize += $lSizeRead;
        }
        while ($lSizeRead != 0);

        # Close the file
        $oFileIo->close();

        # If nothing was read then set to undef
        if ($lSize == 0)
        {
            $tContent = undef;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => defined($oFileIo) ? \$tContent : undef, trace => true},
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Open the file
    my $oFileIo = $self->driver()->openRead($self->pathGet($xFileExp), {bIgnoreMissing => $bIgnoreMissing});

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
        $lTimestamp,
        $bAtomic,
        $bPathCreate,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'xFileExp'},
            {name => 'strMode', optional => true, default => $self->{strDefaultFileMode}},
            {name => 'lTimestamp', optional => true},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => false},
        );

    # Open the file
    my $oFileIo = $self->driver()->openWrite($self->pathGet($xFileExp),
        {strMode => $strMode, lTimestamp => $lTimestamp, bPathCreate => $bPathCreate, bAtomic => $bAtomic});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oFileIo, trace => true},
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
# pathGet - resolve a path expression into an absolute path
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,                                               # File that needs to be translated to a path
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
    # Else it must be relative
    else
    {
        $strPath = $self->pathBase();
        $strFile = $strPathExp;
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->put', \@_,
            {name => 'xFile', trace => true},
            {name => 'xContent', required => false, trace => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or if
    # the repo has a user passphrase defined - else pass undef
    my $oFileIo = ref($xFile) ? $xFile : $self->openWrite($xFile);

    # Determine size of content
    my $lSize = defined($xContent) ? length(ref($xContent) ? $$xContent : $xContent) : 0;

    # Write only if there is something to write
    if ($lSize > 0)
    {
        $oFileIo->write(ref($xContent) ? $xContent : \$xContent);
    }
    # Else open the file so a zero length file is created (since file is not opened until first write)
    else
    {
        $oFileIo->open();
    }

    # Close the file
    $oFileIo->close();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSize', value => $lSize, trace => true},
    );
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
