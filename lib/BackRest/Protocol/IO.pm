####################################################################################################################################
# PROTOCOL IO MODULE
####################################################################################################################################
package BackRest::Protocol::IO;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use IPC::Open3 qw(open3);
use IO::Select;
use POSIX qw(:sys_wait_h);
use Symbol 'gensym';
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Common::Wait;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_IO_PROTOCOL                                                  => 'IO';

use constant OP_IO_PROTOCOL_NEW                                              => OP_IO_PROTOCOL . "->new";
use constant OP_IO_PROTOCOL_NEW3                                             => OP_IO_PROTOCOL . "->new3";

####################################################################################################################################
# CONSTRUCTOR
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
        $self->{hIn},                               # Input stream
        $self->{hOut},                              # Output stream
        $self->{hErr},                              # Error stream
        $self->{pId},                               # Process ID
        $self->{iProtocolTimeout},                  # Protocol timeout
        $self->{iBufferMax}                         # Maximum buffer size
    ) =
        logDebugParam
        (
            OP_IO_PROTOCOL_NEW, \@_,
            {name => 'hIn', required => false, trace => true},
            {name => 'hOut', required => false, trace => true},
            {name => 'hErr', required => false, trace => true},
            {name => 'pId', required => false, trace => true},
            {name => 'iProtocolTimeout', trace => true},
            {name => 'iBufferMax', trace => true}
        );

    if (defined($self->{hIn}))
    {
        $self->{oInSelect} = IO::Select->new();
        $self->{oInSelect}->add($self->{hIn});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# new3
#
# Use open3 to run the command and get the io handles.
####################################################################################################################################
sub new3
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $iProtocolTimeout,                           # Protocol timeout
        $iBufferMax                                  # Maximum buffer Size
    ) =
        logDebugParam
        (
            OP_IO_PROTOCOL_NEW3, \@_,
            {name => 'strCommand', trace => true},
            {name => 'iProtocolTimeout', trace => true},
            {name => 'iBufferMax', trace => true}
        );

    # Use open3 to run the command
    my ($pId, $hIn, $hOut, $hErr);
    $hErr = gensym;

    $pId = IPC::Open3::open3($hIn, $hOut, $hErr, $strCommand);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $class->new($hOut, $hIn, $hErr, $pId, $iProtocolTimeout, $iBufferMax)}
    );
}

####################################################################################################################################
# Get pid/input/output handles
####################################################################################################################################
sub hInGet
{
    my $self = shift;

    return $self->{hIn};
}

sub hOutGet
{
    my $self = shift;

    return $self->{hOut};
}

sub pIdGet
{
    my $self = shift;

    return $self->{pId};
}

####################################################################################################################################
# kill
####################################################################################################################################
sub kill
{
    my $self = shift;

    if (defined($self->{pId}))
    {
        kill 'KILL', $self->{pId};
        waitpid($self->{pId}, 0);
        undef($self->{pId});
        undef($self->{hIn});
        undef($self->{hOut});
        undef($self->{hErr});
    }
}

####################################################################################################################################
# lineRead
#
# Read an lf-terminated line from the input or error stream.
####################################################################################################################################
sub lineRead
{
    my $self = shift;
    my $iTimeout = shift;
    my $bInRead = shift;
    my $bError = shift;

    # If there's already data in the buffer try to find the next linefeed
    my $iLineFeedPos = defined($self->{strBuffer}) ? index($self->{strBuffer}, "\n", $self->{iBufferPos}) : -1;

    # Store the current time if a timeout is required
    if ($iLineFeedPos == -1)
    {
        my $fTimeout = defined($iTimeout) ? $iTimeout : $self->{iProtocolTimeout};
        my $fRemaining = $fTimeout;
        my $fTimeStart = gettimeofday();

        # If no linefeed was found then load more data
        do
        {
            # If the buffer already has data
            if (defined($self->{strBuffer}) && $self->{iBufferPos} < $self->{iBufferSize})
            {
                # And the buffer position is not 0 then trim it so there's room for more data
                if ($self->{iBufferPos} != 0)
                {
                    $self->{strBuffer} = substr($self->{strBuffer}, $self->{iBufferPos});
                    $self->{iBufferSize} = $self->{iBufferSize} - $self->{iBufferPos};
                    $self->{iBufferPos} = 0;
                }
            }
            # Else the buffer is empty and data will need to be loaded
            else
            {
                undef($self->{strBuffer});  # !!! Do we need this?
                $self->{iBufferSize} = 0;
                $self->{iBufferPos} = 0;
            }

            # Get stream handle and select object
            my $hIn;
            my $oSelect;

            # If this is a normal input read
            if (!defined($bInRead) || $bInRead)
            {
                $hIn = $self->{hIn};
                $oSelect = $self->{oInSelect};
            }
            # Else this is an error read
            else
            {
                # The select object will need to be created the first time an error is read
                if (!defined($self->{oErrSelect}))
                {
                    $self->{oErrSelect} = IO::Select->new();
                    $self->{oErrSelect}->add($self->{hErr});
                }

                $oSelect = $self->{oErrSelect};
                $hIn = $self->{hErr};
            }

            # Load data into the buffer
            my $iBufferRead = 0;

            if ($oSelect->can_read($fRemaining))
            {
                $iBufferRead = sysread($hIn, $self->{strBuffer}, $self->{iBufferSize} >= $self->{iBufferMax} ?
                                       $self->{iBufferMax} : $self->{iBufferMax} - $self->{iBufferSize}, $self->{iBufferSize});

                # An error occurred if undef is returned
                if (!defined($iBufferRead))
                {
                    # Store the error message
                    my $strError = defined($!) && $! ne '' ? $! : undef;

                    # Check if the remote process exited unexpectedly
                    $self->waitPid();

                    # Raise the error
                    confess &log(ERROR, 'unable to read line' . (defined($strError) ? ": ${strError}" : ''));
                }

                # Error on EOF (unless reading from error stream)
                if ($iBufferRead == 0)
                {
                    # Check if the remote process exited unexpectedly
                    $self->waitPid();

                    # Only error if reading from the input stream
                    if (defined($bError) && $bError)
                    {
                        confess &log(ERROR, "unexpected EOF", ERROR_FILE_READ);
                    }
                    # If reading from error stream then just return undef
                    else
                    {
                        return undef;
                    }
                }
            }

            # If data was read then check for a linefeed
            if ($iBufferRead > 0)
            {
                $self->{iBufferSize} += $iBufferRead;

                $iLineFeedPos = index($self->{strBuffer}, "\n");
            }
            else
            {
                # Check if the remote process exited unexpectedly
                $self->waitPid();
            }

            # Calculate time remaining before timeout
            if ($iLineFeedPos == -1)
            {
                $fRemaining = $fTimeout - (gettimeofday() - $fTimeStart);
            }
        }
        while ($iLineFeedPos == -1 && $fRemaining > 0);

        # If not linefeed was found within the time return undef
        if ($iLineFeedPos == -1)
        {
            if (!defined($bError) || $bError)
            {
                confess &log(ERROR, "unable to read line after $self->{iProtocolTimeout} seconds", ERROR_PROTOCOL_TIMEOUT);
            }

            return undef;
        }
    }

    # Return the line that was found and adjust the buffer position
    my $strLine = $iLineFeedPos - $self->{iBufferPos} == 0 ? '' :
                      substr($self->{strBuffer}, $self->{iBufferPos}, $iLineFeedPos - $self->{iBufferPos});
    $self->{iBufferPos} = $iLineFeedPos + 1;

    return $strLine;
}

####################################################################################################################################
# errorWrite
#
# Write error data to the stream.
####################################################################################################################################
sub errorWrite
{
    my $self = shift;
    my $strBuffer = shift;

    if (defined($self->{pId}))
    {
        confess &log(ASSERT, 'errorWrite() not valid in master process');
    }

    $self->lineWrite($strBuffer, $self->{hError});
}

####################################################################################################################################
# lineWrite
#
# Write line to the stream.
####################################################################################################################################
sub lineWrite
{
    my $self = shift;
    my $strBuffer = shift;
    my $hOut = shift;

    # Check if the process has exited abnormally (doesn't seem like we should need this, but the next syswrite does a hard
    # abort if the remote process has already closed)
    $self->waitPid();

    # Write the data
    my $iLineOut = syswrite(defined($hOut) ? $hOut : $self->{hOut}, (defined($strBuffer) ? $strBuffer : '') . "\n");

    if (!defined($iLineOut) || $iLineOut != (defined($strBuffer) ? length($strBuffer) : 0) + 1)
    {
        # Check if the process has exited abnormally
        $self->waitPid();

        confess &log(ERROR, "unable to write ${strBuffer}: $!", ERROR_PROTOCOL);
    }
}

####################################################################################################################################
# bufferRead
#
# Read data from a stream.
####################################################################################################################################
sub bufferRead
{
    my $self = shift;
    my $tBufferRef = shift;
    my $iRequestSize = shift;
    my $iOffset = shift;
    my $bBlock = shift;

    # Set working variables
    my $iRemainingSize = $iRequestSize;
    $iOffset = defined($iOffset) ? $iOffset : 0;
    $bBlock = defined($bBlock) ? $bBlock : false;

    # If there is data left over in the buffer from lineRead then use it
    if (defined($self->{strBuffer}) && $self->{iBufferPos} < $self->{iBufferSize})
    {
        # There is enough data in the buffer to satisfy the entire request and have data left over
        if ($iRemainingSize < $self->{iBufferSize} - $self->{iBufferPos})
        {
            $$tBufferRef = substr($self->{strBuffer}, $self->{iBufferPos}, $iRequestSize);
            $self->{iBufferPos} += $iRequestSize;
            return $iRequestSize;
        }
        # Else the entire buffer will be used (and more may be needed if blocking)
        else
        {
            $$tBufferRef = substr($self->{strBuffer}, $self->{iBufferPos});
            my $iReadSize = $self->{iBufferSize} - $self->{iBufferPos};
            $iRemainingSize -= $iReadSize;
            undef($self->{strBuffer});

            return $iReadSize if !$bBlock || $iRemainingSize == 0;

            $iOffset += $iReadSize;
        }
    }

    # If this is a blocking read then loop until all bytes have been read - else error.
    #
    # This link (http://docstore.mik.ua/orelly/perl/cookbook/ch07_15.htm) demonstrates a way to implement this same loop without
    # using select.  Not sure if it would be better but the link has been left here so it can be found in the future if the
    # implementation needs to be changed.
    if ($bBlock)
    {
        my $fTimeStart = gettimeofday();
        my $fRemaining = $self->{iProtocolTimeout};

        do
        {
            # Check if the sysread call will block
            if ($self->{oInSelect}->can_read($fRemaining))
            {
                # Read a data into the buffer
                my $iReadSize = sysread($self->{hIn}, $$tBufferRef, $iRemainingSize, $iOffset);

                # Process errors from the sysread
                if (!defined($iReadSize))
                {
                    my $strError = $!;

                    $self->waitPid();
                    confess &log(ERROR, 'unable to read' . (defined($strError) ? ": ${strError}" : ''));
                }

                # Check for EOF
                if ($iReadSize == 0)
                {
                    confess &log(ERROR, 'unexpected EOF', ERROR_FILE_READ);
                }

                # Update remaining size and return when it reaches 0
                $iRemainingSize -= $iReadSize;

                if ($iRemainingSize == 0)
                {
                    return $iRequestSize;
                }

                # Update the offset to advance the next read further into the buffer
                $iOffset += $iReadSize;
            }

            # Calculate time remaining before timeout
            $fRemaining = $self->{iProtocolTimeout} - (gettimeofday() - $fTimeStart);
        }
        while ($fRemaining > 0);

        # Throw an error if timeout happened before required bytes were read
        confess &log(ERROR, "unable to read ${iRequestSize} bytes after $self->{iProtocolTimeout} seconds", ERROR_PROTOCOL_TIMEOUT);
    }

    # Otherwise do a non-blocking read and return whatever bytes are ready
    my $iReadSize = sysread($self->{hIn}, $$tBufferRef, $iRemainingSize, $iOffset);

    # Process errors from sysread
    if (!defined($iReadSize))
    {
        my $strError = $!;

        $self->waitPid();
        confess &log(ERROR, 'unable to read' . (defined($strError) ? ": ${strError}" : ''));
    }

    # Return the number of bytes read
    return $iReadSize;
}

####################################################################################################################################
# bufferWrite
#
# Write data to a stream.
####################################################################################################################################
sub bufferWrite
{
    my $self = shift;
    my $tBufferRef = shift;
    my $iWriteSize = shift;

    # If block size is not defined, get it from buffer length
    $iWriteSize = defined($iWriteSize) ? $iWriteSize : length($$tBufferRef);

    # Write the block
    my $iWriteOut = syswrite($self->{hOut}, $$tBufferRef, $iWriteSize);

    # Report any errors
    if (!defined($iWriteOut) || $iWriteOut != $iWriteSize)
    {
        my $strError = $!;

        $self->waitPid();
        confess "unable to write ${iWriteSize} bytes" . (defined($strError) ? ': ' . $strError : '');
    }
}

####################################################################################################################################
# waitPid
#
# See if the remote process has terminated unexpectedly.
####################################################################################################################################
sub waitPid
{
    my $self = shift;
    my $fWaitTime = shift;
    my $bReportError = shift;

    # Record the start time and set initial sleep interval
    my $oWait = waitInit($fWaitTime);

    if (defined($self->{pId}))
    {
        do
        {
            my $iResult = waitpid($self->{pId}, WNOHANG);

            # If there is no such process we'll assume it terminated previously
            if ($iResult == -1)
            {
                return true;
            }

            # If the process exited then this is unexpected
            if ($iResult > 0)
            {
                # Get the exit status so we can report it later
                my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

                # Sometimes we'll expect an error so it won't always be reported
                if (!defined($bReportError) || $bReportError)
                {
                    # Default error
                    my $strError = 'no error on stderr';

                    # If the error stream is already closed then we can't fetch the real error
                    if (!defined($self->{hErr}))
                    {
                        $strError = 'no error captured because stderr is already closed';
                    }
                    # Get whatever text we can from the error stream
                    else
                    {
                        eval
                        {
                            $strError = undef;

                            while (my $strLine = $self->lineRead(0, false, false))
                            {
                                if (defined($strError))
                                {
                                    $strError .= "\n";
                                }

                                $strError .= $strLine;
                            }
                        };

                        if ($@ || !defined($strError))
                        {
                            $strError = 'no output from terminated process' . (defined($@) && $@ ne '' ? ": $@" : '');
                        }
                    }

                    $self->{pId} = undef;
                    $self->{hIn} = undef;
                    $self->{hOut} = undef;
                    $self->{hErr} = undef;

                    # Finally, confess the error
                    confess &log(ERROR, 'remote process terminated' .
                                 ($iExitStatus < ERROR_MINIMUM && $iExitStatus > ERROR_MAXIMUM ?
                                    " (exit status ${iExitStatus}" : '') .
                                 ": ${strError}",
                                 $iExitStatus >= ERROR_MINIMUM && $iExitStatus <= ERROR_MAXIMUM ? $iExitStatus : ERROR_HOST_CONNECT);
                }

                return true;
            }
        }
        while (waitMore($oWait));
    }

    return false;
}

1;
