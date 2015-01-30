####################################################################################################################################
# THREADGROUP MODULE
####################################################################################################################################
package BackRest::ThreadGroup;

use threads;
use threads::shared;
use strict;
use warnings;
use Carp;

use File::Basename;
use Time::HiRes qw(gettimeofday usleep);

use lib dirname($0) . '/../lib';
use BackRest::Utility;

####################################################################################################################################
# Shared for all objects
####################################################################################################################################
my %oThreadGroupHash :shared; # Stores active thread groups

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $strClass = shift;       # Class name
    my $strDescription = shift; # Description of the group

    # strDescription must be defined
    if (!defined($strDescription))
    {
        confess &log(ASSERT, 'strDescription is not defined');
    }

    # Create the label based on time and description
    hsleep(.001);
    my $strLabel = '[' . gettimeofday() . '] ' . $strDescription;

    # Create the class hash
    my $self = {};
    bless $self, $strClass;

    &log(TRACE, ref($self) . " create [$self] '${strLabel}'");

    # Initialize variables
    $self->{iThreadTotal} = 0;
    $self->{iThreadId} = threads->tid();

    # Lock the group has so that mods are synchronized
    lock(%oThreadGroupHash);

    # Add the time to the description and store it as the label, making sure there are no collisions
    if (defined($oThreadGroupHash{$strLabel}))
    {
        confess &log(ASSERT, "collision detected in thread group '${strLabel}'");
    }

    $self->{strLabel} = shared_clone($strLabel);
    $oThreadGroupHash{$self->{strLabel}} = true;

    return $self;
}

####################################################################################################################################
# ADD
#
# Add a thread to the group.  Once a thread is added, it can be tracked as part of the group.
####################################################################################################################################
sub add
{
    my $self = shift;
    my $oThread = shift;

    &log(TRACE, ref($self) . " add [$self], thread [$oThread]");

    $self->{oyThread}[$self->{iThreadTotal}] = $oThread;
    $self->{iThreadTotal}++;

    return $self->{iThreadTotal} - 1;
}

####################################################################################################################################
# COMPLETE
#
# Wait for threads to complete.
####################################################################################################################################
sub complete
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
        # hsleep(.1);
        hsleep(1);
        &log(TRACE, 'waiting for threads to exit');

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
                    # $self->kill();

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
# kill
####################################################################################################################################
sub kill
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

            $self->{oyThread}[$iThreadIdx] = undef;
            $iTotal++;
        }
    }

    return($iTotal);
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    if ($self->{iThreadId} != threads->tid())
    {
        return;
    }

    &log(TRACE, "BackRest::ThreadGroup destroy [$self] '$self->{strLabel}'");

    $self->kill();

    # Lock the group hash and delete the current group
    lock(%oThreadGroupHash);

    if (!defined($oThreadGroupHash{$self->{strLabel}}))
    {
        confess &log(ASSERT, "BackRest::ThreadGroup [$self] '$self->{strLabel}' has already been destroyed");
    }

    delete($oThreadGroupHash{$self->{strLabel}});
}

####################################################################################################################################
# print
####################################################################################################################################
sub print
{
    # Lock the group hash
    lock(%oThreadGroupHash);

    # print all active groups
    foreach my $strLabel (sort(keys %oThreadGroupHash))
    {
        &log(TRACE, "BackRest::ThreadGroup active '${strLabel}'");
    }
}

1;
