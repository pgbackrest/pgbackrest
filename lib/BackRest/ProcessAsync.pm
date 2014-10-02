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

#use Exporter qw(import);
#our @EXPORT = qw(new DESTROY thread_kill process_begin process_end);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;
    my $self = {};

    $self->{oThreadQueue} = Thread::Queue->new();
    $self->{oThreadResult} = Thread::Queue->new();
    $self->{oThread} = threads->create(\&process_thread, $self);

    bless $self, $class;
    return $self;
}

####################################################################################################################################
# thread_kill
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
# process_thread
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
# process_begin
#
# Begins the de/compress/checksum operation
####################################################################################################################################
sub process_begin
{
    my $self = shift;
    my $strProcess = shift;
    my $hIn = shift;

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
    if (!defined($hIn))
    {
        confess &log(ASSERT, 'strProcess must be defined in call to ProcessAsync->process_begin()');
    }

    # Open the in/out pipes
    my $hPipeIn;

    pipe $self->{hPipeOut}, $hPipeIn;

    # Queue the queue job with the thread
    $self->{oThreadQueue}->enqueue("${strProcess}:" . fileno($hIn) . ',' . fileno($hPipeIn));

    # Wait for the thread to acknowledge that it has duplicated the file handles
    my $strMessage = $self->{oThreadResult}->dequeue();

    # Close input pipe so that thread has the only copy
    if ($strMessage eq 'running')
    {
        close($hPipeIn);
    }
    # If any other message is returned then error
    else
    {
        confess "unknown thread message while waiting for running: ${strMessage}";
    }

    return $self->{hPipeOut};
}

####################################################################################################################################
# process_end
#
# Ends the de/compress/checksum operation
####################################################################################################################################
sub process_end
{
    my $self = shift;

    # Check if thread is not running
    if (!defined($self->{hPipeOut}))
    {
        confess &log(ASSERT, 'thread is not running in ProcessAsync->process_end()');
    }

    # Make sure the de/compress pipes are closed
    close($self->{hPipeOut});
    $self->{hPipeOut} = undef;

    # Wait for the thread to acknowledge that it has completed
    my $strMessage = $self->{oThreadResult}->dequeue();

    if ($strMessage ne 'complete')
    {
        confess "unknown thread message while waiting for complete: ${strMessage}";
    }
}

true;
