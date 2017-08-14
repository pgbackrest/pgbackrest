####################################################################################################################################
# PROTOCOL COMMAND MASTER MODULE
####################################################################################################################################
package pgBackRest::Protocol::Command::Master;
use parent 'pgBackRest::Protocol::Base::Master';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Time::HiRes qw(gettimeofday);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Io::Process;
use pgBackRest::Protocol::Base::Master;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,                                   # Name of the protocol
        $strId,                                     # Id of this process for error messages
        $strCommand,                                # Command to execute on local/remote
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $iProtocolTimeout,                          # Protocol timeout
        $bWarnOnError,                              # Throw Errors as Warnings
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName'},
            {name => 'strId'},
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'iProtocolTimeout'},
            {name => 'bWarnOnError', optional => true, default => false},
        );

    # Set command
    if (!defined($strCommand))
    {
        confess &log(ASSERT, 'strCommand must be set');
    }

    # Execute the command
    my $oIo = new pgBackRest::Common::Io::Process(
        new pgBackRest::Common::Io::Buffered(
            new pgBackRest::Common::Io::Handle($strId), $iProtocolTimeout, $iBufferMax), $strCommand);

    # Create the class hash
    my $self = $class->SUPER::new($strName, $strId, $oIo, {bWarnOnError => $bWarnOnError}); 
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# close
####################################################################################################################################
sub close
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bComplete,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->close', \@_,
            {name => 'bComplete', default => false, trace => true},
        );

    # Exit status defaults to success
    my $iExitStatus = 0;
    my $bClosed = false;

    # Only send the exit command if the process is running
    if (defined($self->io()) && defined($self->io()->processId()))
    {
        &log(TRACE, "sending exit command to process");

        eval
        {
            $self->cmdWrite(OP_EXIT);
            return true;
        }
        or do
        {
            my $oException = $EVAL_ERROR;
            my $strError = 'unable to shutdown protocol';
            my $strHint = 'HINT: the process completed all operations successfully but protocol-timeout may need to be increased.';

            if (isException($oException))
            {
                $iExitStatus = $oException->code();
            }
            else
            {
                if (!defined($oException))
                {
                    $oException = 'unknown error';
                }

                $iExitStatus = ERROR_UNKNOWN;
            }

            &log(WARN,
                $strError . ($iExitStatus == ERROR_UNKNOWN ? '' : sprintf(' [%03d]', $oException->code())) . ': ' .
                ($iExitStatus == ERROR_UNKNOWN ? $oException : $oException->message()) .
                ($bComplete ? "\n${strHint}" : ''));
        };

        $self->{oIo}->close();
        undef($self->{oIo});
        $bClosed = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iExitStatus', value => $iExitStatus, trace => !$bClosed}
    );
}

1;
