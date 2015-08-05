####################################################################################################################################
# PROTOCOL IO MODULE
####################################################################################################################################
package BackRest::Protocol::IO;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use IPC::Open3;
use POSIX qw(:sys_wait_h);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

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
    my $class = shift;                  # Class name
    my $hIn = shift;                    # Input stream
    my $hOut = shift;                   # Output stream
    my $hErr = shift;                   # Error stream
    my $pId = shift;                    # Process ID

    # Debug
    logTrace(OP_IO_PROTOCOL_NEW3, DEBUG_CALL);

    # Create the class hash
    my $self = {};
    bless $self, $class;

    $self->{hIn} = $hIn;
    $self->{hOut} = $hOut;
    $self->{hErr} = $hErr;
    $self->{pId} = $pId;

    return $self;
}

####################################################################################################################################
# new3
#
# Use open3 to run the command and get the io handles.
####################################################################################################################################
sub new3
{
    my $class = shift;
    my $strCommand = shift;

    # Debug
    logTrace(OP_IO_PROTOCOL_NEW3, DEBUG_CALL, undef, {command => \$strCommand});

    # Use open3 to run the command
    my ($pId, $hIn, $hOut, $hErr);

    $pId = IPC::Open3::open3($hIn, $hOut, $hErr, $strCommand);

    # Return the IO class
    return $class->new($hOut, $hIn, $hErr, $pId);
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
# Read a line.
####################################################################################################################################
sub lineRead
{
    my $self = shift;
    my $bError = shift;

    my $strLine;
    my $strChar;
    my $iByteIn;

    # my $oOutSelect = IO::Select->new();
    # $oOutSelect->add($self->{hOut});
    #
    # if ($oOutSelect->can_read(2))
    # {
    #     $strLine = readline($self->{hOut});
    #     $strLine = trim($strLine);
    # }

    while (1)
    {
        $iByteIn = sysread($self->{hIn}, $strChar, 1);

        if (!defined($iByteIn) || $iByteIn != 1)
        {
            $self->waitPid();

            if (defined($bError) and !$bError)
            {
                return undef;
            }

            confess &log(ERROR, 'unable to read 1 byte' . (defined($!) ? ': ' . $! : ''));
        }

        if ($strChar eq "\n")
        {
            last;
        }

        $strLine .= $strChar;
    }

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
        confess &log(ASSERT, 'errorWrite() not valid got master process.');
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

    my $iLineOut = syswrite(defined($hOut) ? $hOut : $self->{hOut}, (defined($strBuffer) ? $strBuffer : '') . "\n");

    if (!defined($iLineOut) || $iLineOut != (defined($strBuffer) ? length($strBuffer) : 0) + 1)
    {
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
    my $tBlockRef = shift;
    my $iBlockSize = shift;
    my $iOffset = shift;

    # Read a block from the stream
    my $iBlockIn = sysread($self->{hIn}, $$tBlockRef, $iBlockSize, defined($iOffset) ? $iOffset : 0);

    if (!defined($iBlockIn))
    {
        $self->waitPid();
        confess &log(ERROR, 'unable to read');
    }

    return $iBlockIn;
}

####################################################################################################################################
# bufferWrite
#
# Write data to a stream.
####################################################################################################################################
sub bufferWrite
{
    my $self = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write the block
    my $iBlockOut = syswrite($self->{hOut}, $$tBlockRef, $iBlockSize);

    # Report any errors
    if (!defined($iBlockOut) || $iBlockOut != $iBlockSize)
    {
        my $strError = $!;

        $self->waitPid();
        confess "unable to write ${iBlockSize} bytes" . (defined($strError) ? ': ' . $strError : '');
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

            if (defined($fWaitTime))
            {
                confess &log(TRACE, "waitpid result = $iResult");
            }

            # If there is no such process
            if ($iResult == -1)
            {
                return true;
            }

            if ($iResult > 0)
            {
                if (!defined($bReportError) || $bReportError)
                {
                    my $strError = 'no error on stderr';

                    if (!defined($self->{hErr}))
                    {
                        $strError = 'no error captured because stderr is already closed';
                    }
                    else
                    {
                        $strError = $self->pipeToString($self->{hErr});
                    }

                    $self->{pId} = undef;
                    $self->{hIn} = undef;
                    $self->{hOut} = undef;
                    $self->{hErr} = undef;

                    confess &log(ERROR, "remote process terminated: ${strError}", ERROR_HOST_CONNECT);
                }

                return true;
            }

            &log(TRACE, "waiting for pid");
        }
        while (waitMore($oWait));
    }

    return false;
}

1;
