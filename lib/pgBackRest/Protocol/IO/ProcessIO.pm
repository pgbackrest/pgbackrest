####################################################################################################################################
# PROTOCOL PROCESS IO MODULE
####################################################################################################################################
package pgBackRest::Protocol::IO::ProcessIO;
use parent 'pgBackRest::Protocol::IO::IO';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use IPC::Open3 qw(open3);
use IO::Select;
use POSIX qw(:sys_wait_h);
use Symbol 'gensym';
use Time::HiRes qw(gettimeofday);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Protocol::IO::IO;

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
        $hndIn,                                                     # Input stream
        $hndOut,                                                    # Output stream
        $hndErr,                                                    # Error stream
        $pId,                                                       # Process ID
        $strId,                                                     # Id for messages
        $iProtocolTimeout,                                          # Protocol timeout
        $iBufferMax,                                                # Maximum buffer size
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'hndIn', required => false, trace => true},
            {name => 'hndOut', required => false, trace => true},
            {name => 'hndErr', required => false, trace => true},
            {name => 'pId', required => false, trace => true},
            {name => 'strId', required => false, trace => true},
            {name => 'iProtocolTimeout', trace => true},
            {name => 'iBufferMax', trace => true}
        );

    my $self = $class->SUPER::new($hndIn, $hndOut, $hndErr, $iProtocolTimeout, $iBufferMax);

    $self->{pId} = $pId;
    $self->{strId} = $strId;

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
        $strId,
        $strCommand,
        $iProtocolTimeout,                           # Protocol timeout
        $iBufferMax                                  # Maximum buffer Size
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new3', \@_,
            {name => 'strId', trace => true},
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
        {name => 'self', value => $class->new($hOut, $hIn, $hErr, $pId, $strId, $iProtocolTimeout, $iBufferMax)}
    );
}

####################################################################################################################################
# Get process id
####################################################################################################################################
sub processId
{
    my $self = shift;

    return $self->{pId};
}

####################################################################################################################################
# error
#
# See if the remote process has terminated unexpectedly and attempt to retrieve error information from stderr.  If that fails then
# call parent error() method to report error.
####################################################################################################################################
sub error
{
    my $self = shift;
    my $iCode = shift;
    my $strMessage = shift;
    my $strSubMessage = shift;

    # Record the start time and set initial sleep interval
    my $oWait = waitInit(defined($iCode) ? IO_ERROR_TIMEOUT : 0);

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

                # Initialize error
                my $strError = undef;

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
                        while (my $strLine = $self->lineRead(0, false, false))
                        {
                            if (defined($strError))
                            {
                                $strError .= "\n";
                            }

                            $strError .= $strLine;
                        }

                        return true;
                    }
                    or do
                    {
                        if (!defined($strError))
                        {
                            my $strException = $EVAL_ERROR;

                            $strError =
                                'no output from terminated process' .
                                (defined($strException) && ${strException} ne '' ? ": ${strException}" : '');
                        }
                    };
                }

                $self->{pId} = undef;
                $self->{hIn} = undef;
                $self->{hOut} = undef;
                $self->{hErr} = undef;

                # Finally, confess the error
                confess &log(
                    ERROR, 'remote process terminated on ' . $self->{strId} . ' host' .
                    ($iExitStatus < ERROR_MINIMUM || $iExitStatus > ERROR_MAXIMUM ? " (exit status ${iExitStatus})" : '') .
                    ': ' . (defined($strError) ? $strError : 'no error on stderr'),
                    $iExitStatus >= ERROR_MINIMUM && $iExitStatus <= ERROR_MAXIMUM ? $iExitStatus : ERROR_HOST_CONNECT);
            }
        }
        while (waitMore($oWait));
    }

    # Confess default error
    $self->SUPER::error($iCode, $strMessage, $iCode);
}

1;
