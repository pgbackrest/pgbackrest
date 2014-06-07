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

use lib dirname($0);
use lib dirname($0) . "/../lib";
use BackRest::Exception;
use pg_backrest_utility;

# Protocol strings
has strGreeting => (is => 'ro', default => 'PG_BACKREST_REMOTE 0.20');

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
    if (trim(readline($self->{hOut})) ne $self->{strGreeting})
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

    my $strLine;
    my $strOutput;
    my $bError = false;
    my $iErrorCode;

    while ($strLine = readline($self->{hOut}))
    {
        if ($strLine =~ /^ERROR.*/)
        {
            $bError = true;
            last;
        }

        if (trim($strLine) =~ /^OK$/)
        {
            last;
        }

        $strOutput .= trim(substr($strLine, 1));
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
# COMMAND_READ
####################################################################################################################################
sub command_read
{
    my $self = shift;

    my $strOut = readline(*STDIN);
    my $iPos = index($strOut, ':');
    
    my $strCommand;
    my $strOptions;
    
    # If no colon then there are no options
    if ($iPos == -1)
    {
        $strCommand = lc(trim($strOut));
    }
    # Else parse options
    else
    {
        $strCommand = lc(substr($strOut, 0, $iPos));
        $strOptions = trim(substr($strOut, $iPos + 1));
    }

    return $strCommand, $strOptions;
}

####################################################################################################################################
# COMMAND_WRITE
####################################################################################################################################
sub command_write
{
    my $self = shift;
    my $strCommand = shift;
    my $strOptions = shift;

    if (!syswrite($self->{hIn}, "$strCommand" . (defined($strOptions) ? ":${strOptions}" : "") . "\n"))
    {
        confess "unable to write command";
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;
