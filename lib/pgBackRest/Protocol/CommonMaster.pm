####################################################################################################################################
# PROTOCOL COMMON MASTER MODULE
####################################################################################################################################
package pgBackRest::Protocol::CommonMaster;
use parent 'pgBackRest::Protocol::Common';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Time::HiRes qw(gettimeofday);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::IO;
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
        $strRemoteType,                             # Type of remote (DB or BACKUP)
        $strName,                                   # Name of the protocol
        $strId,                                     # Id of this process for error messages
        $strCommand,                                # Command to execute on local/remote
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $iProtocolTimeout,                          # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strRemoteType'},
            {name => 'strName'},
            {name => 'strId'},
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'iProtocolTimeout'},
        );

    # Create the class hash
    my $self = $class->SUPER::new($iBufferMax, $iCompressLevel, $iCompressLevelNetwork, $iProtocolTimeout, $strName);
    bless $self, $class;

    # Set remote to specificied value
    $self->{strRemoteType} = $strRemoteType;

    # Set command
    if (!defined($strCommand))
    {
        confess &log(ASSERT, 'strCommand must be set');
    }

    # Execute the command
    $self->{io} = pgBackRest::Protocol::IO->new3($strId, $strCommand, $iProtocolTimeout, $iBufferMax);

    # Check greeting to be sure the protocol matches
    $self->greetingRead();

    # Setup the keepalive timer
    $self->{fKeepAliveTimeout} = $iProtocolTimeout / 2 > 120 ? 120 : $iProtocolTimeout / 2;
    $self->{fKeepAliveTime} = gettimeofday();

    # Set the id to be used for messages (especially error messages)
    $self->{strId} = $strId;

    # Set the error prefix used when raising error messages
    $self->{strErrorPrefix} = 'raised on ' . $self->{strId} . ' host';

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
    if (defined($self->{io}) && defined($self->{io}->pIdGet()))
    {
        &log(TRACE, "sending exit command to process");

        eval
        {
            $self->cmdWrite('exit');
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
                $strError . ($iExitStatus == ERROR_UNKNOWN ? '' : ' [' . $oException->code() . ']') . ': ' .
                ($iExitStatus == ERROR_UNKNOWN ? $oException : $oException->message()) .
                ($bComplete ? "\n${strHint}" : ''));
        };

        undef($self->{io});
        $bClosed = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iExitStatus', value => $iExitStatus, trace => !$bClosed}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    $self->close();
}

####################################################################################################################################
# greetingRead
#
# Read the greeting and make sure it is as expected.
####################################################################################################################################
sub greetingRead
{
    my $self = shift;

    # Get the first line of output from the remote if possible
    my $hGreeting = $self->{oJSON}->decode($self->{io}->lineRead(undef, undef, undef, true));

    # Error if greeting parameters do not match
    for my $hParam ({strName => 'name', strExpected => BACKREST_NAME},
                    {strName => 'version', strExpected => BACKREST_VERSION},
                    {strName => 'service', strExpected => $self->{strName}})
    {
        if (!defined($hGreeting->{$hParam->{strName}}) || $hGreeting->{$hParam->{strName}} ne $hParam->{strExpected})
        {
            $self->{io}->kill();

            confess &log(ERROR,
                'found name \'' . (defined($hGreeting->{$hParam->{strName}}) ? $hGreeting->{$hParam->{strName}} : '[undef]') .
                "' in protocol greeting instead of expected '$hParam->{strExpected}'", ERROR_HOST_CONNECT);
        }
    }

    # Exchange one protocol message to catch errors early
    $self->outputRead();
}

####################################################################################################################################
# outputRead
#
# Read output from the remote process.
####################################################################################################################################
sub outputRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bOutputRequired,
        $bSuppressLog,
        $bWarnOnError,
        $bRef,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->outputRead', \@_,
            {name => 'bOutputRequired', default => false, trace => true},
            {name => 'bSuppressLog', required => false, trace => true},
            {name => 'bWarnOnError', default => false, trace => true},
            {name => 'bRef', default => false, trace => true},
        );

    my $strProtocolResult = $self->{io}->lineRead();

    logDebugMisc
    (
        $strOperation, undef,
        {name => 'strProtocolResult', value => $strProtocolResult, trace => true}
    );

    my $hResult = $self->{oJSON}->decode($strProtocolResult);

    # Raise any errors
    if (defined($hResult->{err}))
    {
        my $strError = $self->{strErrorPrefix} . (defined($hResult->{out}) ? ": $hResult->{out}" : '');

        # Raise the error if a warning is not requested
        if (!$bWarnOnError)
        {
            confess &log(ERROR, $strError, $hResult->{err}, $bSuppressLog);
        }

        &log(WARN, $strError, $hResult->{err});
        undef($hResult->{out});
    }

    # Reset the keep alive time
    $self->{fKeepAliveTime} = gettimeofday();

    # If output is required and there is no output, raise exception
    if ($bOutputRequired && !defined($hResult->{out}))
    {
        $self->{io}->waitPid();
        confess &log(ERROR, "$self->{strErrorPrefix}: output is not defined", ERROR_PROTOCOL_OUTPUT_REQUIRED);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hOutput', value => $hResult->{out}, ref => $bRef, trace => true}
    );
}

####################################################################################################################################
# cmdWrite
#
# Send command to remote process.
####################################################################################################################################
sub cmdWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $hParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->cmdWrite', \@_,
            {name => 'strCommand', trace => true},
            {name => 'hParam', required => false, trace => true},
        );

    my $strProtocolCommand = $self->{oJSON}->encode({cmd => $strCommand, param => $hParam});

    logDebugMisc
    (
        $strOperation, undef,
        {name => 'strProtocolCommand', value => $strProtocolCommand, trace => true}
    );

    # Write out the command
    $self->{io}->lineWrite($strProtocolCommand);

    # Reset the keep alive time
    $self->{fKeepAliveTime} = gettimeofday();

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

####################################################################################################################################
# cmdExecute
#
# Send command to remote process and wait for output.
####################################################################################################################################
sub cmdExecute
{
    my $self = shift;
    my $strCommand = shift;
    my $oParamRef = shift;
    my $bOutputRequired = shift;
    my $bWarnOnError = shift;

    $self->cmdWrite($strCommand, $oParamRef);

    return $self->outputRead($bOutputRequired, undef, $bWarnOnError);
}

####################################################################################################################################
# keepAlive
#
# Send periodic noops so the remote does not time out.
####################################################################################################################################
sub keepAlive
{
    my $self = shift;

    if (gettimeofday() - $self->{fKeepAliveTimeout} > $self->{fKeepAliveTime})
    {
        $self->noOp();

        # Keep alive test point
        &log(TEST, TEST_KEEP_ALIVE);
    }
}

####################################################################################################################################
# noOp
#
# Send noop to test connection or keep it alive.
####################################################################################################################################
sub noOp
{
    my $self = shift;

    $self->cmdExecute(OP_NOOP, undef, false);
    $self->{fKeepAliveTime} = gettimeofday();
}

1;
