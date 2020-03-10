####################################################################################################################################
# Basic Handle IO
####################################################################################################################################
package pgBackRestTest::Common::Io::Handle;
use parent 'pgBackRestTest::Common::Io::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant COMMON_IO_HANDLE                                       => __PACKAGE__;
    push @EXPORT, qw(COMMON_IO_HANDLE);

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
        $strId,
        $fhRead,
        $fhWrite,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strId', trace => true},
            {name => 'fhRead', required => false, trace => true},
            {name => 'fhWrite', required => false, trace => true},
        );

    # Create class
    my $self = $class->SUPER::new($strId);
    bless $self, $class;

    # Set handles
    $self->handleReadSet($fhRead) if defined($fhRead);
    $self->handleWriteSet($fhWrite) if defined($fhWrite);

    # Size tracks number of bytes read and written
    $self->{lSize} = 0;

    # Initialize EOF to false
    $self->eofSet(false);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# eof - have reads reached eof?
#
# Note that there may be more bytes to be read but this is set to true whenever there is a zero read and back to false on a
# non-zero read.
####################################################################################################################################
sub eof
{
    return shift->{bEOF};
}

####################################################################################################################################
# eofSet - set eof
####################################################################################################################################
sub eofSet
{
    my $self = shift;
    my $bEOF = shift;

    $self->{bEOF} = $bEOF;
}

####################################################################################################################################
# read - read data from handle
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    # Read the block
    my $iActualSize;

    eval
    {
        $iActualSize = sysread($self->handleRead(), $$rtBuffer, $iSize, defined($$rtBuffer) ? length($$rtBuffer) : 0);
        return true;
    }
    or do
    {
        $self->error(ERROR_FILE_READ, 'unable to read from ' . $self->id(), $EVAL_ERROR);
    };

    # Report any errors
    # uncoverable branch true - all errors seem to be caught by the handler above but check for error here just in case
    defined($iActualSize)
        or $self->error(ERROR_FILE_READ, 'unable to read from ' . $self->id(), $OS_ERROR);

    # Update size
    $self->{lSize} += $iActualSize;

    # Update EOF
    $self->eofSet($iActualSize == 0 ? true : false);

    return $iActualSize;
}

####################################################################################################################################
# write - write data to handle
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Write the block
    my $iActualSize;

    eval
    {
        $iActualSize = syswrite($self->handleWrite(), $$rtBuffer);
        return true;
    }
    or do
    {
        $self->error(ERROR_FILE_WRITE, 'unable to write to ' . $self->id(), $EVAL_ERROR);
    };

    # Report any errors
    # uncoverable branch true - all errors seem to be caught by the handler above but check for error here just in case
    defined($iActualSize)
        or $self->error(ERROR_FILE_WRITE, 'unable to write to ' . $self->id(), $OS_ERROR);

    # Update size
    $self->{lSize} += $iActualSize;

    return $iActualSize;
}

####################################################################################################################################
# close - record read/write size
####################################################################################################################################
sub close
{
    my $self = shift;

    # Set bytes read and written
    if (defined($self->{lSize}))
    {
        $self->resultSet(COMMON_IO_HANDLE, $self->{lSize});
        undef($self->{lSize});
    }

    return true;
}

####################################################################################################################################
# Getters/Setters
####################################################################################################################################
sub handleRead {return shift->{fhHandleRead}}
sub handleReadSet {my $self = shift; $self->{fhHandleRead} = shift}
sub handleWrite {return shift->{fhHandleWrite}}
sub handleWriteSet {my $self = shift; $self->{fhHandleWrite} = shift}
sub size {shift->{lSize}}

1;
