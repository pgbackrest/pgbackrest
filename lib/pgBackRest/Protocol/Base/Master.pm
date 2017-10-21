####################################################################################################################################
# Protocol Master Base
####################################################################################################################################
package pgBackRest::Protocol::Base::Master;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Time::HiRes qw(gettimeofday);
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Version;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_NOOP                                                => 'noop';
    push @EXPORT, qw(OP_NOOP);
use constant OP_EXIT                                                => 'exit';
    push @EXPORT, qw(OP_EXIT);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strName},
        $self->{strId},
        $self->{oIo},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName', trace => true},
            {name => 'strId', trace => true},
            {name => 'oIo', trace => true},
        );

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

    # Setup the keepalive timer
    $self->{fKeepAliveTimeout} = $self->io()->timeout() / 2 > 120 ? 120 : $self->io()->timeout() / 2;
    $self->{fKeepAliveTime} = gettimeofday();

    # Set the error prefix used when raising error messages
    $self->{strErrorPrefix} = 'raised on ' . $self->{strId} . ' host';

    # Check greeting to be sure the protocol matches
    $self->greetingRead();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
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
    my $strGreeting = $self->io()->readLine(true);

    # Parse the greeting and make sure it is valid
    my $hGreeting;

    eval
    {
        $hGreeting = $self->{oJSON}->decode($strGreeting);

        return true;
    }
    # Report any error that stopped parsing
    or do
    {
        $self->io()->error(ERROR_PROTOCOL, 'invalid protocol greeting', $strGreeting);
    };

    # Error if greeting parameters do not match
    for my $hParam ({strName => 'name', strExpected => BACKREST_NAME},
                    {strName => 'version', strExpected => BACKREST_VERSION},
                    {strName => 'service', strExpected => $self->{strName}})
    {
        if (!defined($hGreeting->{$hParam->{strName}}) || $hGreeting->{$hParam->{strName}} ne $hParam->{strExpected})
        {
            confess &log(ERROR,
                'found name \'' . (defined($hGreeting->{$hParam->{strName}}) ? $hGreeting->{$hParam->{strName}} : '[undef]') .
                "' in protocol greeting instead of expected '$hParam->{strExpected}'", ERROR_HOST_CONNECT);
        }
    }

    # Perform noop to catch errors early
    $self->noOp();
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

    my $strProtocolResult = $self->io()->readLine();

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
    $self->io()->writeLine($strProtocolCommand);

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

####################################################################################################################################
# Getters
####################################################################################################################################
sub io {shift->{oIo}}
sub master {true}

1;
