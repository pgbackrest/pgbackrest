####################################################################################################################################
# PROTOCOL LOCAL PROCESS MODULE
#
# This module can be extended by commands that want to perform jobs in parallel.
####################################################################################################################################
package pgBackRest::Protocol::Local::Process;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use IO::Select;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Local::Master;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strHostType},
        $self->{iSelectTimeout},
        $self->{strBackRestBin},
        $self->{bConfessError},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strHostType'},
            {name => 'iSelectTimeout', default => int(cfgOption(CFGOPT_PROTOCOL_TIMEOUT) / 2)},
            {name => 'strBackRestBin', default => BACKREST_BIN},
            {name => 'bConfessError', default => true},
        );

    # Declare host map and array
    $self->{hHostMap} = {};
    $self->{hyHost} = undef;

    # Reset module variables to get ready for queueing
    $self->reset();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# reset
#
# Reset to inital state.
####################################################################################################################################
sub reset
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->reset');

    # Select object is used to check for new results
    $self->{oSelect} = undef;

    # Declare local process map and array
    $self->{hyLocalMap} = undef;
    $self->{hyLocal} = undef;

    # Set the processing flag to false
    $self->{bProcessing} = false;

    # Initialize job total to 0
    $self->{iQueued} = 0;

    # Initialize running job total to 0
    $self->{iRunning} = 0;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# hostAdd
#
# Add a host where jobs can be executed.
####################################################################################################################################
sub hostAdd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iHostConfigIdx,
        $iProcessMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostAdd', \@_,
            {name => 'iHostConfigIdx'},
            {name => 'iProcessMax'},
        );

    my $iHostIdx = $self->{hHostMap}{$iHostConfigIdx};

    if (!defined($iHostIdx))
    {
        $iHostIdx = defined($self->{hyHost}) ? @{$self->{hyHost}} : 0;
        $self->{hHostMap}{$iHostConfigIdx} = $iHostIdx;
    }

    my $hHost =
    {
        iHostConfigIdx => $iHostConfigIdx,
        iProcessMax => $iProcessMax,
    };

    push(@{$self->{hyHost}}, $hHost);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# hostConnect
#
# Connect local processes to the hosts.
####################################################################################################################################
sub hostConnect
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->hostConnect');

    # Create a select object used to monitor output from minions
    $self->{oSelect} = IO::Select->new();

    # Iterate hosts
    my $iHostIdx = 0;

    foreach my $hHost (@{$self->{hyHost}})
    {
        # If there a no jobs in the queue for this host then no need to connect
        if (!defined($hHost->{hyQueue}))
        {
            logDebugMisc(
                $strOperation, "no jobs for host",
                {name => 'strHostType', value => $self->{strHostType}},
                {name => 'iHostConfigIdx', value => $hHost->{iHostConfigIdx}});
            next;
        }

        for (my $iHostProcessIdx = 0; $iHostProcessIdx < $hHost->{iProcessMax}; $iHostProcessIdx++)
        {
            my $iLocalIdx = defined($self->{hyLocal}) ? @{$self->{hyLocal}} : 0;
            my $iProcessId = $iLocalIdx + 1;

            logDebugMisc(
                $strOperation, 'start local process',
                {name => 'strHostType', value => $self->{strHostType}},
                {name => 'iHostProcessIdx', value => $iHostProcessIdx},
                {name => 'iHostConfigIdx', value => $hHost->{iHostConfigIdx}},
                {name => 'iHostIdx', value => $iHostIdx},
                {name => 'iProcessId', value => $iProcessId});

            my $oLocal = new pgBackRest::Protocol::Local::Master
            (
                cfgCommandWrite(
                    CFGCMD_LOCAL, true, $self->{strBackRestBin}, undef,
                    {
                        &CFGOPT_COMMAND => {value => cfgCommandName(cfgCommandGet())},
                        &CFGOPT_PROCESS => {value => $iProcessId},
                        &CFGOPT_TYPE => {value => $self->{strHostType}},
                        &CFGOPT_HOST_ID => {value => $hHost->{iHostConfigIdx}},

                        &CFGOPT_LOG_LEVEL_STDERR => {},
                    }),
                $iLocalIdx + 1
            );

            my $hLocal =
            {
                iHostIdx => $iHostIdx,
                iProcessId => $iProcessId,
                iHostProcessIdx => $iHostProcessIdx,
                oLocal => $oLocal,
                hndIn => fileno($oLocal->io()->handleRead()),
            };

            push(@{$self->{hyLocal}}, $hLocal);

            $self->{hLocalMap}{$hLocal->{hndIn}} = $hLocal;
            $self->{oSelect}->add($hLocal->{hndIn});
        }

        $iHostIdx++;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $iHostIdx > 0 ? true : false}
    );
}

####################################################################################################################################
# init
#
# Initialize data structures required for processing.
####################################################################################################################################
sub init
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->init');

    if ($self->hostConnect())
    {
        foreach my $hLocal (@{$self->{hyLocal}})
        {
            my $hHost = $self->{hyHost}[$hLocal->{iHostIdx}];
            my $hyQueue = $hHost->{hyQueue};

            # Initialize variables to keep track of what job the local is working on
            $hLocal->{iDirection} = $hLocal->{iHostProcessIdx} % 2 == 0 ? 1 : -1;
            $hLocal->{iQueueIdx} = int((@{$hyQueue} / $hHost->{iProcessMax}) * $hLocal->{iHostProcessIdx});

            # Calculate the last queue that this process should pull from
            $hLocal->{iQueueLastIdx} = $hLocal->{iQueueIdx} + ($hLocal->{iDirection} * -1);

            if ($hLocal->{iQueueLastIdx} < 0)
            {
                $hLocal->{iQueueLastIdx} = @{$hyQueue} - 1;
            }
            elsif ($hLocal->{iQueueLastIdx} >= @{$hyQueue})
            {
                $hLocal->{iQueueLastIdx} = 0;
            }

            logDebugMisc(
                $strOperation, 'init local process',
                {name => 'iHostIdx', value => $hLocal->{iHostIdx}},
                {name => 'iProcessId', value => $hLocal->{iProcessId}},
                {name => 'iDirection', value => $hLocal->{iDirection}},
                {name => 'iQueueIdx', value => $hLocal->{iQueueIdx}},
                {name => 'iQueueLastIdx', value => $hLocal->{iQueueLastIdx}});
        }

        $self->{bProcessing} = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $self->processing()}
    );
}

####################################################################################################################################
# process
#
# Run all jobs and return results in batches.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Initialize processing
    if (!$self->processing())
    {
        if (!$self->init())
        {
            logDebugMisc($strOperation, 'no jobs to run');
            $self->reset();
            return;
        }
    }

    # If jobs are running then wait for any of them to complete
    my @hyResult = ();
    my $iCompleted = 0;

    if ($self->{iRunning} > 0)
    {
        &logDebugMisc(
            $strOperation, 'check running jobs',
            {name => 'iRunning', value => $self->{iRunning}, trace => true});

        # Wait for results to be available on any of the local process inputs
        my @hndyIn = $self->{oSelect}->can_read($self->{iSelectTimeout});

        # Fetch results from the completed jobs
        foreach my $hndIn (@hndyIn)
        {
            # Get the local data
            my $hLocal = $self->{hLocalMap}{$hndIn};

            if (!defined($hLocal))
            {
                confess &log(ASSERT, "unable to map from fileno ${hndIn} to local");
            }

            # Get the job result
            my $hJob = $hLocal->{hJob};

            eval
            {
                $hJob->{rResult} = $hLocal->{oLocal}->outputRead(true, undef, undef, true);
                return true;
            }
            or do
            {
                my $oException = $EVAL_ERROR;

                # If not a backrest exception then always confess it - something has gone very wrong
                confess $oException if (!isException(\$oException));

                # If the process is has terminated throw the exception
                if (!defined($hLocal->{oLocal}->io()->processId()))
                {
                    confess logException($oException);
                }

                # If errors should be confessed then do so
                if ($self->{bConfessError})
                {
                    confess logException($oException);
                }
                # Else store exception so caller can process it
                else
                {
                    $hJob->{oException} = $oException;
                }
            };

            $hJob->{iProcessId} = $hLocal->{iProcessId};
            push(@hyResult, $hJob);

            logDebugMisc(
                $strOperation, 'job complete',
                {name => 'iProcessId', value => $hJob->{iProcessId}},
                {name => 'strKey', value => $hJob->{strKey}},
                {name => 'rResult', value => $hJob->{rResult}});

            # Free the local process to receive another job
            $hLocal->{hJob} = undef;
            $self->{iRunning}--;
            $iCompleted++;
        }
    }

    # If any jobs are not running/completed then assign new jobs
    if ($self->{iRunning} == 0 || $iCompleted > 0)
    {
        &logDebugMisc(
            $strOperation, 'get new jobs',
            {name => 'iRunning', value => $self->{iRunning}, trace => true},
            {name => 'iCompleted', value => $iCompleted, trace => true});

        my $bFound = false;
        my $iLocalIdx = -1;

        # Iterate all local processes
        foreach my $hLocal (@{$self->{hyLocal}})
        {
            # Skip this local process if it has already completed
            $iLocalIdx++;
            next if (!defined($hLocal));

            my $hHost = $self->{hyHost}[$hLocal->{iHostIdx}];
            my $hyQueue = $hHost->{hyQueue};

            # If this process does not currently have a job assigned then find one
            if (!defined($hLocal->{hJob}))
            {
                # Search queues for a new job
                my $iQueueIdx = $hLocal->{iQueueIdx};
                my $hJob = shift(@{$$hyQueue[$iQueueIdx]});

                while (!defined($hJob) && $iQueueIdx != $hLocal->{iQueueLastIdx})
                {
                    $iQueueIdx += $hLocal->{iDirection};

                    if ($iQueueIdx < 0)
                    {
                        $iQueueIdx = @{$hyQueue} - 1;
                    }
                    elsif ($iQueueIdx >= @{$hyQueue})
                    {
                        $iQueueIdx = 0;
                    }

                    $hJob = shift(@{$$hyQueue[$iQueueIdx]});
                }

                # If no job was found then stop the local process
                if (!defined($hJob))
                {
                    logDebugMisc(
                        $strOperation, 'no jobs found, stop local',
                        {name => 'strHostType', value => $hLocal->{strHostType}},
                        {name => 'iHostConfigIdx', value => $hLocal->{iHostConfigIdx}},
                        {name => 'iHostIdx', value => $hLocal->{iHostIdx}},
                        {name => 'iProcessId', value => $hLocal->{iProcessId}});

                    # Remove input handle from the select object
                    my $iHandleTotal = $self->{oSelect}->count();

                    $self->{oSelect}->remove($hLocal->{hndIn});

                    if ($iHandleTotal - $self->{oSelect}->count() != 1)
                    {
                        confess &log(ASSERT,
                            "iProcessId $hLocal->{iProcessId}, handle $hLocal->{hndIn} was not removed from select object");
                    }

                    # Remove input handle from the map
                    delete($self->{hLocalMap}{$hLocal->{hndIn}});

                    # Close the local process
                    $hLocal->{oLocal}->close(true);

                    # Undefine local process so it is no longer checked for new jobs
                    undef(${$self->{hyLocal}}[$iLocalIdx]);

                    # Skip to next local process
                    next;
                }

                # Assign job to local process
                $hLocal->{hJob} = $hJob;
                $bFound = true;
                $self->{iRunning}++;
                $self->{iQueued}--;

                logDebugMisc(
                    $strOperation, 'get job from queue',
                    {name => 'iHostIdx', value => $hLocal->{iHostIdx}},
                    {name => 'iProcessId', value => $hLocal->{iProcessId}},
                    {name => 'strQueueIdx', value => $iQueueIdx},
                    {name => 'strKey', value => $hLocal->{hJob}{strKey}});

                # Send job to local process
                $hLocal->{oLocal}->cmdWrite($hLocal->{hJob}{strOp}, $hLocal->{hJob}->{rParam});
            }
        }

        # If nothing is running, no more jobs, and nothing to return, then processing is complete
        if (!$bFound && !$self->{iRunning} && @hyResult == 0)
        {
            logDebugMisc($strOperation, 'all jobs complete');
            $self->reset();
            return;
        }
    }

    # Return job results
    return \@hyResult;
}

####################################################################################################################################
# queueJob
#
# Queue a job for processing.
####################################################################################################################################
sub queueJob
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iHostConfigIdx,
        $strQueue,
        $strKey,
        $strOp,
        $rParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->queueJob', \@_,
            {name => 'iHostConfigIdx'},
            {name => 'strQueue'},
            {name => 'strKey'},
            {name => 'strOp'},
            {name => 'rParam'},
        );

    # Don't add jobs while in the middle of processing the current queue
    if ($self->processing())
    {
        confess &log(ASSERT, 'new jobs cannot be added until processing is complete');
    }

    # Build the hash
    my $hJob =
    {
        iHostConfigIdx => $iHostConfigIdx,
        strQueue => $strQueue,
        strKey => $strKey,
        strOp => $strOp,
        rParam => $rParam,
    };

    # Get the host that will perform this job
    my $iHostIdx = $self->{hHostMap}{$iHostConfigIdx};

    if (!defined($iHostIdx))
    {
        confess &log(ASSERT, "iHostConfigIdx = $iHostConfigIdx does not exist");
    }

    my $hHost = $self->{hyHost}[$iHostIdx];

    # Get the queue to hold this job
    my $iQueueIdx = $hHost->{hQueueMap}{$strQueue};

    if (!defined($iQueueIdx))
    {
        $iQueueIdx = defined($hHost->{hyQueue}) ? @{$hHost->{hyQueue}} : 0;
        $hHost->{hQueueMap}{$strQueue} = $iQueueIdx;
    }

    push(@{$hHost->{hyQueue}[$iQueueIdx]}, $hJob);
    $self->{iQueued}++;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# dequeueJobs
#
# Dequeue all jobs from a queue.
####################################################################################################################################
sub dequeueJobs
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iHostConfigIdx,
        $strQueue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dequeueJobs', \@_,
            {name => 'iHostConfigIdx'},
            {name => 'strQueue'},
        );

    # Don't add jobs while in the middle of processing the current queue
    if (!$self->processing())
    {
        confess &log(ASSERT, 'unable to dequeue a job when not processing');
    }

    # Get the host that contains the queue to clear
    my $iHostIdx = $self->{hHostMap}{$iHostConfigIdx};

    if (!defined($iHostIdx))
    {
        confess &log(ASSERT, "iHostConfigIdx = $iHostConfigIdx does not exist");
    }

    my $hHost = $self->{hyHost}[$iHostIdx];

    # Get the queue to clear
    my $iQueueIdx = $hHost->{hQueueMap}{$strQueue};

    if (!defined($iQueueIdx))
    {
        confess &log(ASSERT, "unable to find queue '${strQueue}'");
    }

    $hHost->{hyQueue}[$iQueueIdx] = [];
    $self->{iQueued} = 0;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# jobTotal
#
# Total jobs in the queue.
####################################################################################################################################
sub jobTotal
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->jobTotal');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iJobTotal', value => $self->{iQueued} + $self->{iRunning}}
    );
}

####################################################################################################################################
# processing
#
# Are jobs being processed?
####################################################################################################################################
sub processing
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->processing');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bProcessing', value => $self->{bProcessing}, trace => true}
    );
}

1;
