####################################################################################################################################
# THREADGROUP MODULE
####################################################################################################################################
package BackRest::ThreadGroup;

use threads;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

####################################################################################################################################
# MODULE EXPORTS
####################################################################################################################################
use Exporter qw(import);

our @EXPORT = qw(thread_group_create thread_group_add thread_group_complete);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub thread_group_create
{
    # Create the class hash
    my $self = {};

    # Initialize variables
    $self->{iThreadTotal} = 0;

    return $self;
}

####################################################################################################################################
# ADD
#
# Add a thread to the group.  Once a thread is added, it can be tracked as part of the group.
####################################################################################################################################
sub thread_group_add
{
    my $self = shift;
    my $oThread = shift;

    $self->{oyThread}[$self->{iThreadTotal}] = $oThread;
    $self->{iThreadTotal}++;

    return $self->{iThreadTotal} - 1;
}

####################################################################################################################################
# COMPLETE
#
# Wait for threads to complete.
####################################################################################################################################
sub thread_group_complete
{
    my $self = shift;
    my $iTimeout = shift;
    my $bConfessOnError = shift;

    # Set defaults
    $bConfessOnError = defined($bConfessOnError) ? $bConfessOnError : true;

    # Wait for all threads to complete and handle errors
    my $iThreadComplete = 0;
    my $lTimeBegin = time();

    # Rejoin the threads
    while ($iThreadComplete < $self->{iThreadTotal})
    {
        hsleep(.1);

        # If a timeout has been defined, make sure we have not been running longer than that
        if (defined($iTimeout))
        {
            if (time() - $lTimeBegin >= $iTimeout)
            {
                confess &log(ERROR, "threads have been running more than ${iTimeout} seconds, exiting...");

                #backup_thread_kill();

                #confess &log(WARN, "all threads have exited, aborting...");
            }
        }

        for (my $iThreadIdx = 0; $iThreadIdx < $self->{iThreadTotal}; $iThreadIdx++)
        {
            if (defined($self->{oyThread}[$iThreadIdx]))
            {
                if (defined($self->{oyThread}[$iThreadIdx]->error()))
                {
                    $self->kill();

                    if ($bConfessOnError)
                    {
                        confess &log(ERROR, 'error in thread ' . (${iThreadIdx} + 1) . ': check log for details');
                    }
                    else
                    {
                        return false;
                    }
                }

                if ($self->{oyThread}[$iThreadIdx]->is_joinable())
                {
                    &log(DEBUG, "thread ${iThreadIdx} exited");
                    $self->{oyThread}[$iThreadIdx]->join();
                    &log(TRACE, "thread ${iThreadIdx} object undef");
                    undef($self->{oyThread}[$iThreadIdx]);
                    $iThreadComplete++;
                }
            }
        }
    }

    &log(DEBUG, 'all threads exited');

    return true;
}

####################################################################################################################################
# KILL
####################################################################################################################################
sub thread_group_destroy
{
    my $self = shift;

    # Total number of threads killed
    my $iTotal = 0;

    for (my $iThreadIdx = 0; $iThreadIdx < $self->{iThreadTotal}; $iThreadIdx++)
    {
        if (defined($self->{oyThread}[$iThreadIdx]))
        {
            if ($self->{oyThread}[$iThreadIdx]->is_running())
            {
                $self->{oyThread}[$iThreadIdx]->kill('KILL')->join();
            }
            elsif ($self->{oyThread}[$iThreadIdx]->is_joinable())
            {
                $self->{oyThread}[$iThreadIdx]->join();
            }

            undef($self->{oyThread}[$iThreadIdx]);
            $iTotal++;
        }
    }

    return($iTotal);
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
# sub thread_group_destroy
# {
#     my $self = shift;
#
#     $self->kill();
# }

1;
