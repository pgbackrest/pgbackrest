####################################################################################################################################
# PROTOCOL COMMON MASTER MODULE
####################################################################################################################################
package BackRest::Protocol::CommonMaster;
use parent 'BackRest::Protocol::Common';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Config;
use BackRest::Protocol::Common;
use BackRest::Protocol::IO;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_PROTOCOL_COMMON_MASTER                              => 'Protocol::CommonMaster';

use constant OP_PROTOCOL_COMMON_MASTER_NEW                          => OP_PROTOCOL_COMMON_MASTER . "->new";
use constant OP_PROTOCOL_COMMON_MASTER_COMMAND_WRITE                => OP_PROTOCOL_COMMON_MASTER . "->commandWrite";
use constant OP_PROTOCOL_COMMON_MASTER_OUTPUT_READ                  => OP_PROTOCOL_COMMON_MASTER . "->outputRead";

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
        $strCommand,                                # Command to execute on local/remote
        $iBlockSize,                                # Buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork                      # Set compression level for network only compression
    ) =
        logDebugParam
        (
            OP_PROTOCOL_COMMON_MASTER_NEW, \@_,
            {name => 'strName'},
            {name => 'strCommand'},
            {name => 'iBlockSize'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'}
        );

    # Create the class hash
    my $self = $class->SUPER::new($iBlockSize, $iCompressLevel, $iCompressLevelNetwork, $strName);
    bless $self, $class;

    # Set command
    if (!defined($strCommand))
    {
        confess &log(ASSERT, 'strCommand must be set');
    }

    # Execute the command
    $self->{io} = BackRest::Protocol::IO->new3($strCommand);

    # Check greeting to be sure the protocol matches
    $self->greetingRead();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Only send the exit command if the process is running
    if (defined($self->{io}) && defined($self->{io}->pIdGet()))
    {
        &log(TRACE, "sending exit command to process");

        $self->cmdWrite('exit');

        # &log(TRACE, "waiting for remote process");
        # if (!$self->waitPid(5, false))
        # {
        #     &log(TRACE, "killed remote process");
        #     kill('KILL', $self->{pId});
        # }
    }
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
    my $strLine = $self->{io}->lineRead();

    # If the line could not be read or does equal the greeting then error and exit
    if (!defined($strLine) || $strLine ne $self->{strGreeting})
    {
        $self->{io}->kill();

        confess &log(ERROR, 'protocol version mismatch' . (defined($strLine) ? ": ${strLine}" : ''), ERROR_HOST_CONNECT);
    }
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
        $strErrorPrefix,
        $bSuppressLog
    ) =
        logDebugParam
        (
            OP_PROTOCOL_COMMON_MASTER_OUTPUT_READ, \@_,
            {name => 'bOutputRequired', default => false, trace => true},
            {name => 'strErrorPrefix', required => false, trace => true},
            {name => 'bSuppressLog', required => false, trace => true}
        );

    my $strLine;
    my $strOutput;
    my $bError = false;
    my $iErrorCode;
    my $strError;

    # Read output lines
    while ($strLine = $self->{io}->lineRead(false))
    {
        # Exit if an error is found
        if ($strLine =~ /^ERROR.*/)
        {
            $bError = true;
            $iErrorCode = (split(' ', $strLine))[1];
            last;
        }

        # Exit if OK is found
        if ($strLine =~ /^OK$/)
        {
            last;
        }

        # Else append to output
        $strOutput .= (defined($strOutput) ? "\n" : '') . substr($strLine, 1);
    }

    # Check if the process has exited abnormally
    $self->{io}->waitPid();

    # Raise any errors
    if ($bError)
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : '') .
                            (defined($strOutput) ? "${strOutput}" : ''), $iErrorCode, $bSuppressLog);
    }

    # If output is required and there is no output, raise exception
    if (defined($bOutputRequired) && $bOutputRequired && !defined($strOutput))
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : '') . 'output is not defined');
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strOutput', value => $strOutput, trace => true}
    );
}

####################################################################################################################################
# commandParamString
#
# Output command parameters in the hash as a string (used for debugging).
####################################################################################################################################
sub commandParamString
{
    my $self = shift;
    my $oParamHashRef = shift;

    my $strParamList = '';

    if (defined($oParamHashRef))
    {
        foreach my $strParam (sort(keys(%$oParamHashRef)))
        {
            $strParamList .= (defined($strParamList) ? ',' : '') . "${strParam}=" .
                             (defined(${$oParamHashRef}{"${strParam}"}) ? ${$oParamHashRef}{"${strParam}"} : '[undef]');
        }
    }

    return $strParamList;
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
        $oParamRef,
    ) =
        logDebugParam
        (
            OP_PROTOCOL_COMMON_MASTER_COMMAND_WRITE, \@_,
            {name => 'strCommand', trace => true},
            {name => 'oParamRef', required => false, trace => true}
        );

    if (defined($oParamRef))
    {
        $strCommand .= ":\n";

        foreach my $strParam (sort(keys(%$oParamRef)))
        {
            if ($strParam =~ /=/)
            {
                confess &log(ASSERT, "param \"${strParam}\" cannot contain = character");
            }

            my $strValue = ${$oParamRef}{"${strParam}"};

            if ($strParam =~ /\n\$/)
            {
                confess &log(ASSERT, "param \"${strParam}\" value cannot end with LF");
            }

            if (defined(${strValue}))
            {
                $strCommand .= "${strParam}=${strValue}\n";
            }
        }

        $strCommand .= 'end';
    }

    logDebugMisc
    (
        $strOperation, undef,
        {name => 'strCommand', value => $strCommand, trace => true}
    );

    $self->{io}->lineWrite($strCommand);
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
    my $strErrorPrefix = shift;

    $self->cmdWrite($strCommand, $oParamRef);

    return $self->outputRead($bOutputRequired, $strErrorPrefix);
}

1;
