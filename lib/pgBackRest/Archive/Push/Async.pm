####################################################################################################################################
# ARCHIVE PUSH ASYNC MODULE
####################################################################################################################################
package pgBackRest::Archive::Push::Async;
use parent 'pgBackRest::Archive::Push::Push';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
use pgBackRest::Archive::Push::Push;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::LibC qw(:lock);
use pgBackRest::Protocol::Local::Process;
use pgBackRest::Protocol::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Init object
    my $self = $class->SUPER::new();
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strWalPath},
        $self->{strSpoolPath},
        $self->{strBackRestBin},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strWalPath'},
            {name => 'strSpoolPath'},
            {name => 'strBackRestBin', default => projectBin()},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# initServer
#
# Create the spool directory and initialize the archive process.
####################################################################################################################################
sub initServer
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->initServer');

    # Create the spool path
    storageSpool()->pathCreate($self->{strSpoolPath}, {bIgnoreExists => true, bCreateParent => true});

    # Initialize the archive process
    $self->{oArchiveProcess} = new pgBackRest::Protocol::Local::Process(
        CFGOPTVAL_LOCAL_TYPE_BACKUP, cfgOption(CFGOPT_PROTOCOL_TIMEOUT) < 60 ? cfgOption(CFGOPT_PROTOCOL_TIMEOUT) / 2 : 30,
        $self->{strBackRestBin}, false);
    $self->{oArchiveProcess}->hostAdd(1, cfgOption(CFGOPT_PROCESS_MAX));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# process
#
# Setup the server and process the queue.  This function is separate from processQueue() for testing purposes.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Open the log file
    logFileSet(storageLocal(), cfgOption(CFGOPT_LOG_PATH) . '/' . cfgOption(CFGOPT_STANZA) . '-archive-push-async');

    # There is no loop here because it seems wise to let the async process exit periodically.  As the queue grows each async
    # execution will naturally run longer.  This behavior is also far easier to test.
    $self->initServer();
    $self->processQueue();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# processQueue
#
# Push WAL to the archive.
####################################################################################################################################
sub processQueue
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->processQueue');

    # If queue max is exceeded then drop all WAL in the queue
    my $iDropTotal = 0;

    if (cfgOptionTest(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX))
    {
        my $stryDropList = $self->dropList($self->readyList());

        if (@{$stryDropList} > 0)
        {
            foreach my $strDropFile (@{$stryDropList})
            {
                archiveAsyncStatusWrite(
                    WAL_STATUS_OK, $self->{strSpoolPath}, $strDropFile, 0,
                    "dropped WAL file ${strDropFile} because archive queue exceeded " .
                        cfgOption(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX) . ' bytes');

                $iDropTotal++;
            }
        }
    }

    # Get jobs to process
    my $stryWalFile = $self->readyList();

    # Queue the jobs
    foreach my $strWalFile (@{$stryWalFile})
    {
        $self->{oArchiveProcess}->queueJob(
            1, 'default', $strWalFile, OP_ARCHIVE_PUSH_FILE,
            [$self->{strWalPath}, $strWalFile, cfgOption(CFGOPT_COMPRESS), cfgOption(CFGOPT_COMPRESS_LEVEL)]);
    }

    # Process jobs if there are any
    my $iOkTotal = 0;
    my $iErrorTotal = 0;

    if ($self->{oArchiveProcess}->jobTotal() > 0)
    {
        &log(INFO,
            'push ' . @{$stryWalFile} . ' WAL file(s) to archive: ' .
                ${$stryWalFile}[0] . (@{$stryWalFile} > 1 ? "...${$stryWalFile}[-1]" : ''));

        eval
        {
            # Check for a stop lock
            lockStopTest();

            # Hold a lock when the repo is remote to be sure no other process is pushing WAL
            !isRepoLocal() && protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP);

            while (my $hyJob = $self->{oArchiveProcess}->process())
            {
                # Send keep alives to protocol
                protocolKeepAlive();

                foreach my $hJob (@{$hyJob})
                {
                    my $strWalFile = @{$hJob->{rParam}}[1];
                    my $strWarning = @{$hJob->{rResult}}[0];

                    # If error then write out an error file
                    if (defined($hJob->{oException}))
                    {
                        archiveAsyncStatusWrite(
                            WAL_STATUS_ERROR, $self->{strSpoolPath}, $strWalFile, $hJob->{oException}->code(),
                            $hJob->{oException}->message());

                        $iErrorTotal++;

                        &log(WARN,
                            "could not push WAL file ${strWalFile} to archive (will be retried): [" .
                                $hJob->{oException}->code() . "] " . $hJob->{oException}->message());
                    }
                    # Else write success
                    else
                    {
                        archiveAsyncStatusWrite(
                            WAL_STATUS_OK, $self->{strSpoolPath}, $strWalFile, defined($strWarning) ? 0 : undef,
                            defined($strWarning) ? $strWarning : undef);

                        $iOkTotal++;

                        &log(DETAIL, "pushed WAL file ${strWalFile} to archive", undef, undef, undef, $hJob->{iProcessId});
                    }
                }
            }

            return 1;
        }
        or do
        {
            # Get error info
            my $iCode = exceptionCode($EVAL_ERROR);
            my $strMessage = exceptionMessage($EVAL_ERROR);

            # Error all queued jobs
            foreach my $strWalFile (@{$stryWalFile})
            {
                archiveAsyncStatusWrite(
                    WAL_STATUS_ERROR, $self->{strSpoolPath}, $strWalFile, $iCode, $strMessage);
            }
        }
    }

    return logDebugReturn
    (
        $strOperation,
        {name => 'iNewTotal', value => scalar(@{$stryWalFile})},
        {name => 'iDropTotal', value => $iDropTotal},
        {name => 'iOkTotal', value => $iOkTotal},
        {name => 'iErrorTotal', value => $iErrorTotal}
    );
}

1;
