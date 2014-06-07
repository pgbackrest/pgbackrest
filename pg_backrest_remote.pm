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
use pg_backrest_utility;

# Protocol strings
has strGreeting => (is => 'ro', default => 'pg_backrest_remote 0.20');

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
    my $hOut = shift;

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
    my $hOut = shift;

    if (!syswrite(*STDOUT, "$self->{strGreeting}\n"))
    {
        confess "unable to write greeting";
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;
