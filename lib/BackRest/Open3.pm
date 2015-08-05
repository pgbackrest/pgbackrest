####################################################################################################################################
# OPEN3 MODULE
####################################################################################################################################
package BackRest::Open3;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_OPEN3                                               => 'Open3';

use constant OP_OPEN3_NEW                                           => OP_OPEN3 . "->new";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $strCommand = shift;             # Command to execute

    # Debug
    logDebug(OP_OPEN3_NEW, DEBUG_CALL, undef, {command => \$strCommand});

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Store the command
    defined($strCommand)
        or confess &log(ASSERT, 'strCommand parameter is required');

    $self->{strCommand} = $strCommand;

    return $self;
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;
    my $bInput = shift;

    # Execute the command
    if (defined($bInput) && $bInput)
    {
        $self->{pId} = open3($self->{hIn}, $self->{hOut}, $self->{hError}, $self->{strCommand});
    }
    else
    {
        $self->{pId} = open3(undef, $self->{hOut}, $self->{hError}, $self->{strCommand});
    }

    # Create select objects
    $self->{oErrorSelect} = IO::Select->new();
    $self->{oErrorSelect}->add($self->{hError});
    $self->{oOutSelect} = IO::Select->new();
    $self->{oOutSelect}->add($self->{hOut});
}

####################################################################################################################################
# lineRead
####################################################################################################################################
sub lineRead
{
    my $self = shift;

    if ($self->{oOutSelect}->can_read(.1))
    {
        return readline($self->{hOut});
    }
}

####################################################################################################################################
# capture
####################################################################################################################################
sub capture
{
    my $self = shift;

    # Run the command without input
    $self->run(false);

    # While the process is running drain the stdout and stderr streams
    my $strLine;

    while(waitpid($self->{pId}, WNOHANG) == 0)
    {
        # Drain the stderr stream
        if ($self->{oErrorSelect}->can_read(.1))
        {
            while ($strLine = readline($self->{hError}))
            {
                $self->{strErrorLog} .= $strLine;
            }
        }

        # Drain the stdout stream
        if ($self->{oOutSelect}->can_read(.1))
        {
            while ($strLine = readline($self->{hOut}))
            {
                $self->{strOutLog} .= $strLine;
            }
        }
    }

    # Check the exit status and output an error if needed
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    # Check exit status and output error
    if ($iExitStatus != 0)
    {
        confess &log(ERROR, "unable to execute '$self->{strCommand}' (returned ${iExitStatus})" .
                     (defined($self->{strErrorLog}) ? "\n$self->{strErrorLog}" : ''));
    }

    return $self->{strOutLog};
}

1;
