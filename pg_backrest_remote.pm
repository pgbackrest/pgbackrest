####################################################################################################################################
# REMOTE MODULE
####################################################################################################################################
package pg_backrest_remote;

use threads;
use strict;
use warnings;
use Carp;

use Moose;
use Net::OpenSSH;
use File::Basename;

use lib dirname($0) . "/../lib";
use BackRest::Exception;
use BackRest::Utility;

# Protocol strings
has strGreeting => (is => 'ro', default => 'PG_BACKREST_REMOTE');

# Command strings
has strCommand => (is => 'bare');

# Module variables
has strHost => (is => 'bare');            # Host host
has strUser => (is => 'bare');            # User user
has oSSH => (is => 'bare');               # SSH object

# Process variables
has pId => (is => 'bare');               # SSH object
has hIn => (is => 'bare');               # SSH object
has hOut => (is => 'bare');               # SSH object
has hErr => (is => 'bare');               # SSH object

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub BUILD
{
    my $self = shift;
    
    $self->{strGreeting} .= " " . version_get();

    if (defined($self->{strHost}))
    {
        # User must be defined
        if (!defined($self->{strUser}))
        {
            confess &log(ASSERT, "strUser must be defined");
        }

        # User must be defined
        if (!defined($self->{strCommand}))
        {
            confess &log(ASSERT, "strCommand must be defined");
        }

        # Set SSH Options
        my $strOptionSSHRequestTTY = "RequestTTY=yes";
        my $strOptionSSHCompression = "Compression=no";

        # if ($self->{bNoCompression})
        # {
        #     $strOptionSSHCompression = "Compression=yes";
        # }

        &log(TRACE, "connecting to remote ssh host " . $self->{strHost});

        # Make SSH connection
        $self->{oSSH} = Net::OpenSSH->new($self->{strHost}, timeout => 300, user => $self->{strUser},
                                  master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);

        $self->{oSSH}->error and confess &log(ERROR, "unable to connect to $self->{strHost}: " . $self->{oSSH}->error);

        # Execute remote command
        ($self->{hIn}, $self->{hOut}, $self->{hErr}, $self->{pId}) = $self->{oSSH}->open3($self->{strCommand});

        $self->greeting_read();
    }
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;

    return pg_backrest_remote->new
    (
        strCommand => $self->{strCommand},
        strHost => $self->{strUser},
        strUser => $self->{strHost}
    );
}

####################################################################################################################################
# GREETING_READ
####################################################################################################################################
sub greeting_read
{
    my $self = shift;

    # Make sure that the remote is running the right version
    if (readline($self->{hOut}) ne $self->{strGreeting} . "\n")
    {
        confess &log(ERROR, "remote version mismatch");
    }
}

####################################################################################################################################
# GREETING_WRITE
####################################################################################################################################
sub greeting_write
{
    my $self = shift;

    if (!syswrite(*STDOUT, "$self->{strGreeting}\n"))
    {
        confess "unable to write greeting";
    }
}

####################################################################################################################################
# STRING_WRITE
####################################################################################################################################
sub string_write
{
    my $self = shift;
    my $hOut = shift;
    my $strBuffer = shift;

    $strBuffer =~ s/\n/\n\./g;

    if (!syswrite($hOut, "." . $strBuffer))
    {
        confess "unable to write string";
    }
}

####################################################################################################################################
# ERROR_WRITE
####################################################################################################################################
sub error_write
{
    my $self = shift;
    my $oMessage = shift;

    my $iCode;
    my $strMessage;

    if (blessed($oMessage))
    {
        if ($oMessage->isa("BackRest::Exception"))
        {
            $iCode = $oMessage->code();
            $strMessage = $oMessage->message();
        }
        else
        {
            $strMessage = 'unknown error object';
        }
    }
    else
    {
        $strMessage = $oMessage;
    }

    if (defined($strMessage))
    {
        $self->string_write(*STDOUT, trim($strMessage));
    }

    if (!syswrite(*STDOUT, "\nERROR" . (defined($iCode) ? " $iCode" : "") . "\n"))
    {
        confess "unable to write error";
    }
}

####################################################################################################################################
# OUTPUT_READ
####################################################################################################################################
sub output_read
{
    my $self = shift;

#    &log(TRACE, "Remote->output_read");

    my $strLine;
    my $strOutput;
    my $bError = false;
    my $iErrorCode;

    while ($strLine = readline($self->{hOut}))
    {
        if ($strLine =~ /^ERROR.*/)
        {
            $bError = true;

            $iErrorCode = (split(' ', trim($strLine)))[1];

            last;
        }

        if (trim($strLine) =~ /^OK$/)
        {
            last;
        }

        $strOutput .= (defined($strOutput) ? "\n" : "") . trim(substr($strLine, 1));
    }

    return ($strOutput, $bError, $iErrorCode);
}

####################################################################################################################################
# OUTPUT_WRITE
####################################################################################################################################
sub output_write
{
    my $self = shift;
    my $strOutput = shift;

    $self->string_write(*STDOUT, $strOutput);

    if (!syswrite(*STDOUT, "\nOK\n"))
    {
        confess "unable to write output";
    }
}

####################################################################################################################################
# COMMAND_PARAM_STRING
####################################################################################################################################
sub command_param_string
{
    my $self = shift;
    my $oParamHashRef = shift;

    my $strParamList;

    foreach my $strParam (sort(keys $oParamHashRef))
    {
        $strParamList .= (defined($strParamList) ? "," : "") . "${strParam}=" .
                         (defined(${$oParamHashRef}{"${strParam}"}) ? ${$oParamHashRef}{"${strParam}"} : "[undef]");
    }

    return $strParamList;
}

####################################################################################################################################
# COMMAND_READ
####################################################################################################################################
sub command_read
{
    my $self = shift;
    my $oParamHashRef = shift;

#    &log(TRACE, "Remote->command_read");

    my $strLine;
    my $strCommand;

    while ($strLine = readline(*STDIN))
    {
        $strLine = trim($strLine);

        if (!defined($strCommand))
        {
            if ($strLine =~ /:$/)
            {
                $strCommand = substr($strLine, 0, length($strLine) - 1);
#                print "command ${strCommand} continues\n";
            }
            else
            {
                $strCommand = $strLine;
#                print "command ${strCommand}\n";
                last;
            }
        }
        else
        {
            if ($strLine eq 'end')
            {
                last;
            }

            my $iPos = index($strLine, "=");

            if ($iPos == -1)
            {
                confess "param \"${strLine}\" is missing = character";
            }

            my $strParam = substr($strLine, 0, $iPos);
            my $strValue = substr($strLine, $iPos + 1);

            ${$oParamHashRef}{"${strParam}"} = ${strValue};

#            print "${strParam}=${strValue}\n";
        }
    }

    return $strCommand;
}

####################################################################################################################################
# COMMAND_WRITE
####################################################################################################################################
sub command_write
{
    my $self = shift;
    my $strCommand = shift;
    my $oParamRef = shift;

    my $strOutput = $strCommand;

    if (defined($oParamRef))
    {
        $strOutput = "${strCommand}:\n";

        foreach my $strParam (sort(keys $oParamRef))
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
                $strOutput .= "${strParam}=${strValue}\n";
            }
        }

        $strOutput .= "end";
    }

    &log(TRACE, "Remote->command_write:\n" . trim($strOutput));

    if (!syswrite($self->{hIn}, "${strOutput}\n"))
    {
        confess "unable to write command";
    }
}

####################################################################################################################################
# COMMAND_EXECUTE
####################################################################################################################################
sub command_execute
{
    my $self = shift;
    my $strCommand = shift;
    my $strOptions = shift;
    my $strErrorPrefix = shift;

    $self->command_write($strCommand, $strOptions);

    my ($strOutput, $bError, $iErrorCode) = $self->output_read();

    # Capture any errors
    if ($bError)
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : "") .
                            (defined($strOutput) ? ": ${strOutput}" : ""), $iErrorCode);
    }

    return $strOutput;
}

no Moose;
__PACKAGE__->meta->make_immutable;
