####################################################################################################################################
# Protocol Minion Base
####################################################################################################################################
package pgBackRest::Protocol::Base::Minion;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Protocol::Base::Master;
use pgBackRest::Protocol::Helper;
use pgBackRest::Version;

####################################################################################################################################
# Constant used to define code to run after each operation
####################################################################################################################################
use constant OP_POST                                                => 'post';
    push @EXPORT, qw(OP_POST);

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
        $self->{oIo},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName', trace => true},
            {name => 'oIo', trace => true},
        );

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

    # Write the greeting so master process knows who we are
    $self->greetingWrite();

    # Initialize module variables
    $self->{hCommandMap} = $self->can('init') ? $self->init() : undef;

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
    $self->io()->writeLine((JSON::PP->new()->canonical()->allow_nonref())->encode(
        {name => BACKREST_NAME, service => $self->{strName}, version => BACKREST_VERSION}));
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
    $self->io()->writeLine($self->{oJSON}->encode({err => $oException->code(), out => $oException->message()}));
}

####################################################################################################################################
# outputWrite
#
# Write output for the master process.
####################################################################################################################################
sub outputWrite
{
    my $self = shift;

    $self->io()->writeLine($self->{oJSON}->encode({out => \@_}));
}

####################################################################################################################################
# cmdRead
#
# Read command sent by the master process.
####################################################################################################################################
sub cmdRead
{
    my $self = shift;

    my $hCommand = $self->{oJSON}->decode($self->io()->readLine());

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
                    protocolKeepAlive();
                    $self->outputWrite();
                }
                else
                {
                    confess "invalid command: ${strCommand}";
                }

                # Run the post command if defined
                if (defined($self->{hCommandMap}{&OP_POST}))
                {
                    $self->{hCommandMap}{&OP_POST}->();
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

####################################################################################################################################
# Getters
####################################################################################################################################
sub io {shift->{oIo}}
sub master {false}

1;
