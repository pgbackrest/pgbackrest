####################################################################################################################################
# PROTOCOL COMMON MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::CommonMinion;
use parent 'pgBackRest::Protocol::Common';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::IO;

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
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $iProtocolTimeout,                          # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName'},
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'iProtocolTimeout'},
        );

    # Create the class hash
    my $self = $class->SUPER::new($iBufferMax, $iCompressLevel, $iCompressLevelNetwork, $iProtocolTimeout, $strName);
    bless $self, $class;

    $self->{strCommand} = $strCommand;

    # Create the IO object with std io
    $self->{io} = new pgBackRest::Protocol::IO(*STDIN, *STDOUT, *STDERR, undef, undef, $iProtocolTimeout, $iBufferMax);

    # Write the greeting so master process knows who we are
    $self->greetingWrite();

    # Initialize module variables
    $self->init();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
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

    $self->{io}->lineWrite($self->{strGreeting});
    $self->{io}->lineWrite('OK');
}

####################################################################################################################################
# textWrite
#
# Write text out to the protocol layer prefixing each line with a period.
####################################################################################################################################
sub textWrite
{
    my $self = shift;
    my $strBuffer = shift;

    $strBuffer =~ s/\n/\n\./g;

    $self->{io}->lineWrite('.' . $strBuffer);
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

    my $iCode;
    my $strMessage;

    # If the message is blessed it may be a standard exception
    if (isException($oException))
    {
        $iCode = $oException->code();
        $strMessage = $oException->message();
    }
    # Else terminate the process with an error
    else
    {
        confess &log(ERROR, 'unknown error: ' . $oException, ERROR_UNKNOWN);
    }

    # Write the message text into protocol
    if (defined($strMessage))
    {
        $self->textWrite(trim($strMessage));
    }

    # Indicate that an error ocurred and provide the code
    $self->{io}->lineWrite("ERROR" . (defined($iCode) ? " $iCode" : ''));
}

####################################################################################################################################
# outputWrite
#
# Write output for the master process.
####################################################################################################################################
sub outputWrite
{
    my $self = shift;
    my $strOutput = shift;

    if (defined($strOutput))
    {
        $self->textWrite($strOutput);
    }

    $self->{io}->lineWrite('OK');
}

####################################################################################################################################
# cmdRead
#
# Read command sent by the master process.
####################################################################################################################################
sub cmdRead
{
    my $self = shift;

    my $strLine;
    my $strCommand;
    my $hParam = {};

    while ($strLine = $self->{io}->lineRead())
    {
        if (!defined($strCommand))
        {
            if ($strLine =~ /:$/)
            {
                $strCommand = substr($strLine, 0, length($strLine) - 1);
            }
            else
            {
                $strCommand = $strLine;
                last;
            }
        }
        else
        {
            if ($strLine eq 'end')
            {
                last;
            }

            my $iPos = index($strLine, '=');

            if ($iPos == -1)
            {
                confess "param \"${strLine}\" is missing = character";
            }

            my $strParam = substr($strLine, 0, $iPos);
            my $strValue = substr($strLine, $iPos + 1);

            $hParam->{$strParam} = ${strValue};
        }
    }

    return $strCommand, $hParam;
}

####################################################################################################################################
# paramGet
#
# Helper function that returns the param or an error if required and it does not exist.
####################################################################################################################################
sub paramGet
{
    my $self = shift;
    my $strParam = shift;
    my $bRequired = shift;

    my $strValue = $self->{hParam}{$strParam};

    if (!defined($strValue) && (!defined($bRequired) || $bRequired))
    {
        confess "${strParam} must be defined";
    }

    return $strValue;
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Command string
    my $strCommand = OP_NOOP;

    # Loop until the exit command is received
    eval
    {
        while ($strCommand ne OP_EXIT)
        {
            ($strCommand, $self->{hParam}) = $self->cmdRead();

            eval
            {
                if (!$self->commandProcess($strCommand))
                {
                    if ($strCommand eq OP_NOOP)
                    {
                        $self->outputWrite();
                    }
                    elsif ($strCommand ne OP_EXIT)
                    {
                        confess "invalid command: ${strCommand}";
                    }
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
        logLevelSet(undef, undef, ERROR);

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
