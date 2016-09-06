####################################################################################################################################
# PROTOCOL LOCAL PROCESS MODULE
#
# This module can be extended by commands that want to perform jobs in parallel.
####################################################################################################################################
package pgBackRest::Protocol::LocalProcess;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use IO::Select;

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::LocalMaster;
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strHostType'},
            {name => 'iSelectTimeout', default => int(optionGet(OPTION_PROTOCOL_TIMEOUT) / 2)},
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
        for (my $iProcessIdx = 0; $iProcessIdx < $hHost->{iProcessMax}; $iProcessIdx++)
        {
            my $iLocalIdx = defined($self->{hyLocal}) ? @{$self->{hyLocal}} : 0;

            logDebugMisc(
                $strOperation, 'start local process',
                {name => 'iHostIdx', value => $iHostIdx},
                {name => 'iHostConfigIdx', value => $hHost->{iHostConfigIdx}},
                {name => 'iProcessIdx', value => $iProcessIdx});

            my $oLocal = new pgBackRest::Protocol::LocalMaster
            (
                commandWrite(
                    CMD_LOCAL, true, BACKREST_BIN, undef,
                    {
                        &OPTION_COMMAND => {value => commandGet()},
                        &OPTION_PROCESS => {value => $iLocalIdx + 1},
                        &OPTION_TYPE => {value => $self->{strHostType}},
                        &OPTION_HOST_ID => {value => $hHost->{iHostConfigIdx}},
                    }),
                $iLocalIdx + 1
            );

            my $hLocal =
            {
                iHostIdx => $iHostIdx,
                iProcessIdx => $iProcessIdx,
                oLocal => $oLocal,
            };

            push(@{$self->{hyLocal}}, $hLocal);

            $self->{oSelect}->add($oLocal->{io}->{hIn});
            $self->{hLocalMap}{$oLocal->{io}->{hIn}} = $hLocal;
        }

        $iHostIdx++;
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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

    $self->hostConnect();

    foreach my $hLocal (@{$self->{hyLocal}})
    {
        my $hHost = $self->{hyHost}[$hLocal->{iHostIdx}];
        my $hyQueue = $hHost->{hyQueue};

        # Initialize variables to keep track of what job the local is working on
        $hLocal->{iDirection} = $hLocal->{iProcessIdx} % 2 == 0 ? 1 : -1;
        $hLocal->{iQueueIdx} = int((@{$hyQueue} / $hHost->{iProcessMax}) * $hLocal->{iProcessIdx});

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
            {name => 'iProcessIdx', value => $hLocal->{iProcessIdx}},
            {name => 'iDirection', value => $hLocal->{iDirection}},
            {name => 'iQueueIdx', value => $hLocal->{iQueueIdx}},
            {name => 'iQueueLastIdx', value => $hLocal->{iQueueLastIdx}});

        # Queue map is no longer needed
        undef($hHost->{hQueueMap});
    }

    $self->{bProcessing} = true;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
    if (!$self->{bProcessing})
    {
        $self->init();
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
        my @gyIn = $self->{oSelect}->can_read($self->{iSelectTimeout});

        # Fetch results from the completed jobs
        foreach my $gIn (@gyIn)
        {
            # Get the local data
            my $hLocal = $self->{hLocalMap}{$gIn};

            if (!defined($hLocal))
            {
                confess &log(ASSERT, "unable to map from file handle ${gIn} to local");
            }

            # Get the job result
            my $hJob = $hLocal->{hJob};
            $self->cmdResult($hLocal->{oLocal}, $hJob);
            $hJob->{iProcessIdx} = $hLocal->{iProcessIdx};
            push(@hyResult, $hJob);

            logDebugMisc(
                $strOperation, 'job complete',
                {name => 'iProcessIdx', value => $hJob->{iProcessIdx}},
                {name => 'strKey', value => $hJob->{strKey}});

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
        my $iLocalIdx = 0;

        # Iterate all local processes
        foreach my $hLocal (@{$self->{hyLocal}})
        {
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
                        {name => 'iHostIdx', value => $hLocal->{iHostIdx}},
                        {name => 'iProcessIdx', value => $hLocal->{iProcessIdx}});

                    # Remove input handle from the select object
                    $self->{oSelect}->remove(glob($hLocal->{oLocal}->{io}->{hIn}));

                    # Remove local process from list
                    splice(@{$self->{hyLocal}}, $iLocalIdx, 1);

                    # Skip to next local process
                    next;
                }

                # Assign job to local process
                $hLocal->{hJob} = $hJob;
                $bFound = true;
                $iLocalIdx++;
                $self->{iRunning}++;

                logDebugMisc(
                    $strOperation, 'get job from queue',
                    {name => 'iHostIdx', value => $hLocal->{iHostIdx}},
                    {name => 'iProcessIdx', value => $hLocal->{iProcessIdx}},
                    {name => 'strQueueIdx', value => $iQueueIdx},
                    {name => 'strKey', value => $hLocal->{hJob}{strKey}});

                # Send job to local process
                $self->cmdSend($hLocal->{oLocal}, $hLocal->{hJob});
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
        $hPayload,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->queueJob', \@_,
            {name => 'iHostConfigIdx', trace => true},
            {name => 'strQueue', trace => true},
            {name => 'strKey', trace => true},
            {name => 'hPayload', trace => true},
        );

    # Don't add jobs while in the middle of processing the current queue
    if ($self->{bProcessing})
    {
        confess &log(ASSERT, 'new jobs cannot be added until processing is complete');
    }

    # Build the hash
    my $hJob =
    {
        iHostConfigIdx => $iHostConfigIdx,
        strQueue => $strQueue,
        strKey => $strKey,
        hPayload => $hPayload,
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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
