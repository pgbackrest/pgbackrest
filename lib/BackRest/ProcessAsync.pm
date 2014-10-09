####################################################################################################################################
# PROCESS ASYNC MODULE
####################################################################################################################################
package BackRest::ProcessAsync;

use threads;
use strict;
use warnings;
use Carp;

use Thread::Queue;
use File::Basename;
use IO::Handle;
use IO::Compress::Gzip qw(gzip $GzipError);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);

use lib dirname($0) . '/../lib';
use BackRest::Utility;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize thread and queues
    $self->{oThreadQueue} = Thread::Queue->new();
    $self->{oThreadResult} = Thread::Queue->new();
    $self->{oThread} = threads->create(\&process_thread, $self);

    return $self;
}

####################################################################################################################################
# THREAD_KILL
####################################################################################################################################
sub thread_kill
{
    my $self = shift;

    if (defined($self->{oThread}))
    {
        $self->{oThreadQueue}->enqueue(undef);
        $self->{oThread}->join();
        $self->{oThread} = undef;
    }
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    $self->thread_kill();
}

####################################################################################################################################
# PROCESS_THREAD
#
# De/Compresses/Checksum data on a thread.
####################################################################################################################################
sub process_thread
{
    my $self = shift;

    while (my $strMessage = $self->{oThreadQueue}->dequeue())
    {
        my @stryMessage = split(':', $strMessage);
        my @strHandle = split(',', $stryMessage[1]);

        my $hIn = IO::Handle->new_from_fd($strHandle[0], '<');
        my $hOut = IO::Handle->new_from_fd($strHandle[1], '>');

        $self->{oThreadResult}->enqueue('running');

        if ($stryMessage[0] eq 'compress')
        {
            gzip($hIn => $hOut)
                or confess &log(ERROR, 'unable to compress: ' . $GzipError);
        }
        elsif ($stryMessage[0] eq 'decompress')
        {
            gunzip($hIn => $hOut)
                or confess &log(ERROR, 'unable to decompress: ' . $GunzipError);
        }

        close($hOut);

        $self->{oThreadResult}->enqueue('complete');
    }
}

####################################################################################################################################
# PROCESS_BEGIN
#
# Begins the de/compress/checksum operation
####################################################################################################################################
sub process_begin
{
    my $self = shift;
    my $strProcess = shift;     # process to run (compress, decompress, checksum, size)
    my $hHandle = shift;        # Handle of the input or output
    my $strDirection = shift;   # Does hHandle represent in or out?

    # Set/check value of strDirection
    if (!defined($strDirection))
    {
        $strDirection = 'in';
    }
    else
    {
        if ($strDirection ne 'in' && $strDirection ne 'out')
        {
            confess &log(ASSERT, 'strDirection must be in (in, out) in ProcessAsync->process_begin()');
        }
    }

    # Check if thread is already running
    if (defined($self->{hPipeOut}))
    {
        confess &log(ASSERT, 'thread is already running in ProcessAsync->process_begin()');
    }

    # Validate strProcess
    if (!defined($strProcess))
    {
        confess &log(ASSERT, 'strProcess must be defined in call to ProcessAsync->process_begin()');
    }

    if ($strProcess ne 'compress' && $strProcess ne 'decompress' && $strProcess ne 'checksum')
    {
        confess &log(ASSERT, 'strProcess must be (compress, decompress, checksum) in call to ProcessAsync->process_begin():' .
                             " ${strProcess} was passed");
    }

    # Validate hIn
    if (!defined($hHandle))
    {
        confess &log(ASSERT, 'hHandle must be defined in call to ProcessAsync->process_begin()');
    }

    # Open the out/in pipes
    my $hPipeOut;
    my $hPipeIn;

    pipe $hPipeOut, $hPipeIn;

    # Queue the job in the thread
    if ($strDirection eq 'in')
    {
        $self->{oThreadQueue}->enqueue("${strProcess}:" . fileno($hHandle) . ',' . fileno($hPipeIn));
    }
    else
    {
        $self->{oThreadQueue}->enqueue("${strProcess}:" . fileno($hPipeOut) . ',' . fileno($hHandle));
    }

    # Wait for the thread to acknowledge that it has duplicated the file handles
    my $strMessage = $self->{oThreadResult}->dequeue();

    # Close input pipe so that thread has the only copy
    if ($strMessage eq 'running')
    {
        if ($strDirection eq 'in')
        {
            close($hPipeIn);
            $self->{hPipe} = $hPipeOut;
        }
        else
        {
            close($hPipeOut);
            $self->{hPipe} = $hPipeIn;
        }
    }
    # If any other message is returned then error
    else
    {
        confess "unknown thread message while waiting for running: ${strMessage}";
    }

    return $self->{hPipe};
}

####################################################################################################################################
# PROCESS_END
#
# Ends the de/compress/checksum operation
####################################################################################################################################
sub process_end
{
    my $self = shift;

    # Check if thread is not running
    if (!defined($self->{hPipe}))
    {
        confess &log(ASSERT, 'thread is not running in ProcessAsync->process_end()');
    }

    # Make sure the de/compress pipes are closed
    close($self->{hPipe});
    $self->{hPipe} = undef;

    # Wait for the thread to acknowledge that it has completed
    my $strMessage = $self->{oThreadResult}->dequeue();

    if ($strMessage ne 'complete')
    {
        confess "unknown thread message while waiting for complete: ${strMessage}";
    }
}

true;
