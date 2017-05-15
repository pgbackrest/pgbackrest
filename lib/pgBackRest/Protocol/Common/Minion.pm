####################################################################################################################################
# PROTOCOL COMMON MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Common::Minion;
use parent 'pgBackRest::Protocol::Common::Common';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Protocol::Common::Common;
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
        $strCommand,                                # Command the master process is running
        $oIO,                                       # IO object
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $iProtocolTimeout,                          # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName', trace => true},
            {name => 'strCommand', trace => true},
            {name => 'oIO', trace => true},
            {name => 'iBufferMax', trace => true},
            {name => 'iCompressLevel', trace => true},
            {name => 'iCompressLevelNetwork', trace => true},
            {name => 'iProtocolTimeout', trace => true},
        );

    # Create the class hash
    my $self = $class->SUPER::new($iBufferMax, $iCompressLevel, $iCompressLevelNetwork, $iProtocolTimeout, $strName);
    bless $self, $class;

    # Set IO Object
    $self->{io} = $oIO;

    # Set command the master is running
    $self->{strCommand} = $strCommand;

    # Write the greeting so master process knows who we are
    $self->greetingWrite();

    # Initialize module variables
    $self->{hCommandMap} = $self->init();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# greetingWrite
#
# Send a greeting to the master process.
####################################################################################################################################
sub greetingWrite
{
    my $self = shift;

    # Write the greeting
    $self->{io}->lineWrite((JSON::PP->new()->canonical()->allow_nonref())->encode(
        {name => BACKREST_NAME, service => $self->{strName}, version => BACKREST_VERSION}));

    # Exchange one protocol message to catch errors early
    $self->outputWrite();
}

####################################################################################################################################
# binaryXferAbort
#
# Abort transfer when source file does not exist.
####################################################################################################################################
sub binaryXferAbort
{
    my $self = shift;

    # Only allow in the backend process
    $self->{io}->lineWrite('block -1');
}

####################################################################################################################################
# errorWrite
#
# Write errors with error codes in protocol format, otherwise write to stderr and exit with error.
####################################################################################################################################
sub errorWrite
{
    my $self = shift;
    my $oException = shift;

    # Throw hard error if this is not a standard exception
    if (!isException($oException))
    {
        confess &log(ERROR, 'unknown error: ' . $oException, ERROR_UNKNOWN);
    }

    # Write error code and message
    $self->{io}->lineWrite($self->{oJSON}->encode({err => $oException->code(), out => $oException->message()}));
}

####################################################################################################################################
# outputWrite
#
# Write output for the master process.
####################################################################################################################################
sub outputWrite
{
    my $self = shift;

    $self->{io}->lineWrite($self->{oJSON}->encode({out => \@_}));
}

####################################################################################################################################
# cmdRead
#
# Read command sent by the master process.
####################################################################################################################################
sub cmdRead
{
    my $self = shift;

    my $hCommand = $self->{oJSON}->decode($self->{io}->lineRead());

    return $hCommand->{cmd}, $hCommand->{param};
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Reset stderr log level so random errors do not get output
    logLevelSet(undef, undef, OFF);

    # Loop until the exit command is received
    eval
    {
        while (true)
        {
            my ($strCommand, $rParam) = $self->cmdRead();

            last if ($strCommand eq OP_EXIT);

            eval
            {
                # Check for the command in the map and run it if found
                if (defined($self->{hCommandMap}{$strCommand}))
                {
                    $self->outputWrite($self->{hCommandMap}{$strCommand}->($rParam));
                }
                # Run the standard NOOP command.  This this can be overridden in hCommandMap to implement a custom NOOP.
                elsif ($strCommand eq OP_NOOP)
                {
                    $self->outputWrite();
                }
                else
                {
                    confess "invalid command: ${strCommand}";
                }

                # Run the post command if defined
                if (defined($self->{hCommandMap}{&OP_POST}))
                {
                    $self->{hCommandMap}{&OP_POST}->($strCommand, $rParam);
                }

                return true;
            }
            # Process errors
            or do
            {
                $self->errorWrite($EVAL_ERROR);
            };
        }

        return true;
    }
    or do
    {
        my $oException = $EVAL_ERROR;

        # Change log level so error will go to stderr
        logLevelSet(undef, undef, PROTOCOL);

        # If standard exception
        if (isException($oException))
        {
            confess &log($oException->level(), $oException->message(), $oException->code());
        }

        # Else unexpected Perl exception
        confess &log(ERROR, 'unknown error: ' . $oException, ERROR_UNKNOWN);
    };

    return 0;
}

1;
