####################################################################################################################################
# ARCHIVE PUSH ASYNC MODULE
####################################################################################################################################
package pgBackRest::Archive::Push::Async;
use parent 'pgBackRest::Archive::Push::Push';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);
use IO::Socket::UNIX;
use POSIX qw(setsid);
use Scalar::Util qw(blessed);

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
            {name => 'strBackRestBin', default => backrestBin()},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
#
# Fork and run the async process if it is not already running.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    my $bClient = true;

    # This first lock request is a quick test to see if the async process is running.  If the lock is successful we need to release
    # the lock, fork, and then let the async process acquire its own lock.  Otherwise the lock will be held by the client process
    # which will soon exit and leave the async process unlocked.
    if (lockAcquire(cfgCommandName(cfgCommandGet()), false))
    {
        &log(TEST, TEST_ARCHIVE_PUSH_ASYNC_START);

        lockRelease(cfgCommandName(cfgCommandGet()));
        $bClient = fork() == 0 ? false : true;
    }
    else
    {
        logDebugMisc($strOperation, 'async archive-push process is already running');
    }

    # Run async process
    if (!$bClient)
    {
        # uncoverable branch false - reacquire the lock since it was released by the client process above
        if (lockAcquire(cfgCommandName(cfgCommandGet()), false))
        {
            # uncoverable branch true - chdir to /
            chdir '/'
                or confess &log(ERROR, "unable to chdir to /: $OS_ERROR", ERROR_PATH_MISSING);

            # uncoverable branch true - close stdin
            open(STDIN, '<', '/dev/null')
                or confess &log(ERROR, "unable to close stdin: $OS_ERROR", ERROR_FILE_OPEN);
            # uncoverable branch true - close stdout
            open(STDOUT, '>', '/dev/null')
                or confess &log(ERROR, "unable to close stdout: $OS_ERROR", ERROR_FILE_OPEN);
            # uncoverable branch true - close stderr
            open(STDERR, '>', '/dev/null')
                or confess &log(ERROR, "unable to close stderr: $OS_ERROR", ERROR_FILE_OPEN);

            # uncoverable branch true - create new session group
            setsid()
                or confess &log(ERROR, "unable to create new session group: $OS_ERROR", ERROR_ASSERT);

            # Open the log file
            logFileSet(storageLocal(), cfgOption(CFGOPT_LOG_PATH) . '/' . cfgOption(CFGOPT_STANZA) . '-archive-async', true);

            # Start processing
            $self->processServer();
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bClient', value => $bClient, trace => true}
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
# processServer
#
# Setup the server and process the queue.  This function is separate from processQueue() for testing purposes.
####################################################################################################################################
sub processServer
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->processServer');

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
    my $iDropTotal = 0;

    if ($self->{oArchiveProcess}->jobTotal() > 0)
    {
        &log(INFO,
            'push ' . @{$stryWalFile} . ' WAL file(s) to archive: ' .
                ${$stryWalFile}[0] . (@{$stryWalFile} > 1 ? "...${$stryWalFile}[-1]" : ''));

        eval
        {
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
                        $self->walStatusWrite(
                            WAL_STATUS_ERROR, $strWalFile, $hJob->{oException}->code(), $hJob->{oException}->message());

                        $iErrorTotal++;

                        &log(WARN,
                            "could not push WAl file ${strWalFile} to archive (will be retried): [" .
                                $hJob->{oException}->code() . "] " . $hJob->{oException}->message());
                    }
                    # Else write success
                    else
                    {
                        $self->walStatusWrite(
                            WAL_STATUS_OK, $strWalFile, defined($strWarning) ? 0 : undef,
                            defined($strWarning) ? $strWarning : undef);

                        $iOkTotal++;

                        &log(DETAIL, "pushed WAL file ${strWalFile} to archive", undef, undef, undef, $hJob->{iProcessId});
                    }
                }

                # Drop any jobs that exceed the queue max
                if (cfgOptionTest(CFGOPT_ARCHIVE_QUEUE_MAX))
                {
                    my $stryDropList = $self->dropList($self->readyList());

                    if (@{$stryDropList} > 0)
                    {
                        foreach my $strDropFile (@{$stryDropList})
                        {
                            $self->walStatusWrite(
                                WAL_STATUS_OK, $strDropFile, 0,
                                "dropped WAL file ${strDropFile} because archive queue exceeded " .
                                    cfgOption(CFGOPT_ARCHIVE_QUEUE_MAX) . ' bytes');

                            $iDropTotal++;
                        }

                        $self->{oArchiveProcess}->dequeueJobs(1, 'default');
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
                $self->walStatusWrite(
                    WAL_STATUS_ERROR, $strWalFile, $iCode, $strMessage);
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

####################################################################################################################################
# walStatusWrite
#
# Write out a status file to the spool path with information about the success or failure of an archive push.
####################################################################################################################################
sub walStatusWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strWalFile,
        $iCode,
        $strMessage,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->writeStatus', \@_,
            {name => 'strType'},
            {name => 'strWalFile'},
            {name => 'iCode', required => false},
            {name => 'strMessage', required => false},
        );

    # Remove any error file exists unless a new one will be written
    if ($strType ne WAL_STATUS_ERROR)
    {
        # Remove the error file, if any
        storageSpool()->remove("$self->{strSpoolPath}/${strWalFile}.error", {bIgnoreMissing => true});
    }

    # Write the status file
    my $strStatus;

    if (defined($iCode))
    {
        if (!defined($strMessage))
        {
            confess &log(ASSERT, 'strMessage must be set when iCode is set');
        }

        $strStatus = "${iCode}\n${strMessage}";
    }
    elsif ($strType eq WAL_STATUS_ERROR)
    {
        confess &log(ASSERT, 'error status must have iCode and strMessage set');
    }

    storageSpool()->put(
        storageSpool()->openWrite("$self->{strSpoolPath}/${strWalFile}.${strType}", {bAtomic => true}), $strStatus);
}

1;
