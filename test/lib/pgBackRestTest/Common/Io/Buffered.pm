####################################################################################################################################
# Buffered Handle IO
####################################################################################################################################
package pgBackRestTest::Common::Io::Buffered;
use parent 'pgBackRestTest::Common::Io::Filter';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use IO::Select;
use Time::HiRes qw(gettimeofday);

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;

use pgBackRestTest::Common::Io::Base;
use pgBackRestTest::Common::Io::Handle;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant COMMON_IO_BUFFERED                                     => __PACKAGE__;
    push @EXPORT, qw(COMMON_IO_BUFFERED);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParent,
        $iTimeout,
        $lBufferMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParent', trace => true},
            {name => 'iTimeout', default => 0, trace => true},
            {name => 'lBufferMax', default => COMMON_IO_BUFFER_MAX, trace => true},
        );

    # Bless with new class
    my $self = $class->SUPER::new($oParent);
    bless $self, $class;

    # Set read handle so select object is created
    $self->handleReadSet($self->handleRead());

    # Set variables
    $self->{iTimeout} = $iTimeout;
    $self->{lBufferMax} = $lBufferMax;

    # Initialize buffer
    $self->{tBuffer} = '';
    $self->{lBufferSize} = 0;
    $self->{lBufferPos} = 0;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - buffered read from a handle with optional blocking
####################################################################################################################################
sub read
{
    my $self = shift;
    my $tBufferRef = shift;
    my $iRequestSize = shift;
    my $bBlock = shift;

    # Set working variables
    my $iRemainingSize = $iRequestSize;

    # If there is data left over in the buffer from lineRead then use it
    my $lBufferRemaining = $self->{lBufferSize} - $self->{lBufferPos};

    if ($lBufferRemaining > 0)
    {
        my $iReadSize = $iRequestSize < $lBufferRemaining ? $iRequestSize : $lBufferRemaining;

        $$tBufferRef .= substr($self->{tBuffer}, $self->{lBufferPos}, $iReadSize);
        $self->{lBufferPos} += $iReadSize;

        $iRemainingSize -= $iReadSize;
    }

    # If this is a blocking read then loop until all bytes have been read, else error.  If not blocking read until the request size
    # has been met or EOF.
    my $fTimeStart = gettimeofday();
    my $fRemaining = $self->timeout();

    while ($iRemainingSize > 0 && $fRemaining > 0)
    {
        # Check if the sysread call will block
        if ($self->pending() || $self->{oReadSelect}->can_read($fRemaining))
        {
            # Read data into the buffer
            my $iReadSize = $self->parent()->read($tBufferRef, $iRemainingSize);

            # Check for EOF
            if ($iReadSize == 0)
            {
                if ($bBlock)
                {
                    $self->error(ERROR_FILE_READ, "unable to read ${iRequestSize} byte(s) due to EOF from " . $self->id());
                }
                else
                {
                    return $iRequestSize - $iRemainingSize;
                }
            }

            # Update remaining size and return when it reaches 0
            $iRemainingSize -= $iReadSize;
        }

        # Calculate time remaining before timeout
        $fRemaining = $self->timeout() - (gettimeofday() - $fTimeStart);
    };

    # Throw an error if timeout happened before required bytes were read
    if ($iRemainingSize != 0 && $bBlock)
    {
        $self->error(
            ERROR_FILE_READ, "unable to read ${iRequestSize} byte(s) after " . $self->timeout() . ' second(s) from ' . $self->id());
    }

    return $iRequestSize - $iRemainingSize;
}

####################################################################################################################################
# readLine - read the next lf-terminated line.
####################################################################################################################################
sub readLine
{
    my $self = shift;
    my $bIgnoreEOF = shift;
    my $bError = shift;

    # Try to find the next linefeed
    my $iLineFeedPos = index($self->{tBuffer}, "\n", $self->{lBufferPos});

    # If no linefeed was found then load more data
    if ($iLineFeedPos == -1)
    {
        my $fRemaining = $self->timeout();
        my $fTimeStart = gettimeofday();

        # Load data
        do
        {
            # If the buffer already has data and the buffer position is not 0 then trim it so there's room for more data
            if ($self->{lBufferPos} != 0)
            {
                $self->{tBuffer} = substr($self->{tBuffer}, $self->{lBufferPos});
                $self->{lBufferSize} = $self->{lBufferSize} - $self->{lBufferPos};
                $self->{lBufferPos} = 0;
            }

            # Load data into the buffer
            my $iBufferRead = 0;

            if ($self->pending() || $self->{oReadSelect}->can_read($fRemaining))
            {
                $iBufferRead = $self->parent()->read(
                    \$self->{tBuffer},
                    $self->{lBufferSize} >= $self->bufferMax() ? $self->bufferMax() : $self->bufferMax() - $self->{lBufferSize});

                # Check for EOF
                if ($iBufferRead == 0)
                {
                    # Return undef if EOF is ignored
                    if (defined($bIgnoreEOF) && $bIgnoreEOF)
                    {
                        return;
                    }

                    # Else throw an error
                    $self->error(ERROR_FILE_READ, 'unexpected EOF reading line from ' . $self->id());
                }
            }

            # If data was read then check for a linefeed
            if ($iBufferRead > 0)
            {
                $self->{lBufferSize} += $iBufferRead;

                $iLineFeedPos = index($self->{tBuffer}, "\n");
            }

            # Calculate time remaining before timeout
            if ($iLineFeedPos == -1)
            {
                $fRemaining = $self->timeout() - (gettimeofday() - $fTimeStart);
            }
        }
        while ($iLineFeedPos == -1 && $fRemaining > 0);

        # If not linefeed was found within the timeout throw error
        if ($iLineFeedPos == -1)
        {
            if (!defined($bError) || $bError)
            {
                $self->error(
                    ERROR_FILE_READ, 'unable to read line after ' . $self->timeout() . ' second(s) from ' . $self->id());
            }

            return;
        }
    }

    # Return the line that was found and adjust the buffer position
    my $strLine = substr($self->{tBuffer}, $self->{lBufferPos}, $iLineFeedPos - $self->{lBufferPos});
    $self->{lBufferPos} = $iLineFeedPos + 1;

    return $strLine;
}

####################################################################################################################################
# writeLine - write a string and \n terminate it
####################################################################################################################################
sub writeLine
{
    my $self = shift;
    my $strBuffer = shift;

    $strBuffer .= "\n";
    return $self->parent()->write(\$strBuffer);
}

####################################################################################################################################
# Getters/Setters
####################################################################################################################################
sub timeout {shift->{iTimeout}};
sub bufferMax {shift->{lBufferMax}};

####################################################################################################################################
# handleReadSet - create a select object when read handle is set
####################################################################################################################################
sub handleReadSet
{
    my $self = shift;
    my $fhRead = shift;

    $self->parent()->handleReadSet($fhRead);

    # Create select object to make IO waits more efficient
    $self->{oReadSelect} = IO::Select->new();
    $self->{oReadSelect}->add($self->handleRead());

    # Check if the read handle has a pending method.  This should be checked before can_read for SSL sockets.
    $self->{bPending} = defined($fhRead) && $fhRead->can('pending') ? true : false;
}

####################################################################################################################################
# Are bytes pending that won't show up in select?
####################################################################################################################################
sub pending
{
    my $self = shift;

    return ($self->{bPending} && $self->handleRead()->pending() ? true : false);
}

1;
