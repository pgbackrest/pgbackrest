####################################################################################################################################
# ARCHIVE GET ASYNC MODULE
####################################################################################################################################
package pgBackRest::Archive::Get::Async;
use parent 'pgBackRest::Archive::Get::Get';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
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
        $self->{strSpoolPath},
        $self->{strBackRestBin},
        $self->{rstryWal},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strSpoolPath'},
            {name => 'strBackRestBin', default => backrestBin()},
            {name => 'rstryWal'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Create the spool directory and initialize the archive process.
####################################################################################################################################
sub initServer
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->initServer');

    # Initialize the archive process
    $self->{oArchiveProcess} = new pgBackRest::Protocol::Local::Process(
        CFGOPTVAL_LOCAL_TYPE_BACKUP, cfgOption(CFGOPT_PROTOCOL_TIMEOUT) < 60 ? cfgOption(CFGOPT_PROTOCOL_TIMEOUT) / 2 : 30,
        $self->{strBackRestBin}, false);
    $self->{oArchiveProcess}->hostAdd(1, cfgOption(CFGOPT_PROCESS_MAX));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Setup the server and process the queue.  This function is separate from processQueue() for testing purposes.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Open the log file
    logFileSet(storageLocal(), cfgOption(CFGOPT_LOG_PATH) . '/' . cfgOption(CFGOPT_STANZA) . '-archive-get-async');

    # There is no loop here because it seems wise to let the async process exit periodically.  As the queue grows each async
    # execution will naturally run longer.  This behavior is also far easier to test.
    $self->initServer();
    $self->processQueue();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Get WAL from archive
####################################################################################################################################
sub processQueue
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->processQueue');

    # Queue the jobs
    foreach my $strWalFile (@{$self->{rstryWal}})
    {
        $self->{oArchiveProcess}->queueJob(
            1, 'default', $strWalFile, OP_ARCHIVE_GET_FILE, [$strWalFile, "$self->{strSpoolPath}/${strWalFile}", true]);
    }

    # Process jobs
    my $iFoundTotal = 0;
    my $iMissingTotal = 0;
    my $iErrorTotal = 0;

    &log(INFO,
        'get ' . @{$self->{rstryWal}} . ' WAL file(s) from archive: ' .
            ${$self->{rstryWal}}[0] . (@{$self->{rstryWal}} > 1 ? "...${$self->{rstryWal}}[-1]" : ''));

    eval
    {
        # Check for a stop lock
        lockStopTest();

        while (my $hyJob = $self->{oArchiveProcess}->process())
        {
            foreach my $hJob (@{$hyJob})
            {
                my $strWalFile = @{$hJob->{rParam}}[0];
                my $iResult = @{$hJob->{rResult}}[0];

                # If error then write out an error file
                if (defined($hJob->{oException}))
                {
                    archiveAsyncStatusWrite(
                        WAL_STATUS_ERROR, $self->{strSpoolPath}, $strWalFile, $hJob->{oException}->code(),
                        $hJob->{oException}->message());

                    $iErrorTotal++;

                    &log(WARN,
                        "could not get WAL file ${strWalFile} from archive (will be retried): [" .
                            $hJob->{oException}->code() . "] " . $hJob->{oException}->message());
                }
                # Else write a '.ok' file to indicate that the WAL was not found but there was no error
                elsif ($iResult == 1)
                {
                    archiveAsyncStatusWrite(WAL_STATUS_OK, $self->{strSpoolPath}, $strWalFile);

                    $iMissingTotal++;

                    &log(DETAIL, "WAL file ${strWalFile} not found in archive", undef, undef, undef, $hJob->{iProcessId});
                }
                # Else success so just output a log message
                else
                {
                    $iFoundTotal++;

                    &log(DETAIL, "got WAL file ${strWalFile} from archive", undef, undef, undef, $hJob->{iProcessId});
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
        foreach my $strWalFile (@{$self->{rstryWal}})
        {
            archiveAsyncStatusWrite(WAL_STATUS_ERROR, $self->{strSpoolPath}, $strWalFile, $iCode, $strMessage);
        }
    };

    return logDebugReturn
    (
        $strOperation,
        {name => 'iNewTotal', value => scalar(@{$self->{rstryWal}})},
        {name => 'iFoundTotal', value => $iFoundTotal},
        {name => 'iMissingTotal', value => $iMissingTotal},
        {name => 'iErrorTotal', value => $iErrorTotal}
    );
}

1;
