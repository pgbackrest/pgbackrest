####################################################################################################################################
# Tests for Protocol::Common::Minion module
####################################################################################################################################
package pgBackRestTest::Module::Protocol::ProtocolCommonMinionPerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use IO::Socket::UNIX;
use Time::HiRes qw(usleep);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::LibC qw(:config);
use pgBackRest::Protocol::Base::Minion;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# socketServer
####################################################################################################################################
sub socketServer
{
    my $self = shift;
    my $strSocketFile = shift;
    my $fnServer = shift;

    # Fork off the server
    if (fork() == 0)
    {
        # Open the domain socket
        my $oSocketServer = IO::Socket::UNIX->new(Type => SOCK_STREAM(), Local => $strSocketFile, Listen => 1);
        &log(INFO, "    * socket server open");

        # Wait for a connection and create IO object
        my $oConnection = $oSocketServer->accept();
        my $oIoHandle = new pgBackRest::Common::Io::Handle('socket server', $oConnection, $oConnection);
        &log(INFO, "    * socket server connected");

        # Run server function
        $fnServer->($oIoHandle);

        # Shutdown server
        $oSocketServer->close();
        &log(INFO, "    * socket server closed");
        unlink($strSocketFile);
        exit 0;
    }

    # Wait for client socket
    my $oWait = waitInit(5);
    while (!-e $strSocketFile && waitMore($oWait)) {};

    # Open the client socket
    my $oClient = IO::Socket::UNIX->new(Type => SOCK_STREAM(), Peer => $strSocketFile);

    if (!defined($oClient))
    {
        logErrorResult(ERROR_FILE_OPEN, 'unable to open client socket', $OS_ERROR);
    }

    &log(INFO, "    * socket client connected");

    return $oClient;
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Test data
    my $strSocketFile = $self->testPath() . qw{/} . 'domain.socket';

    ################################################################################################################################
    # if ($self->begin('new() & timeout() & bufferMax()'))
    # {
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     my $oIoBuffered = $self->testResult(
    #         sub {new pgBackRest::Common::Io::Buffered(
    #             new pgBackRest::Common::Io::Handle('test'), 10, 2048)}, '[object]', 'new - no handles');
    #
    #     $self->testResult(sub {$oIoBuffered->timeout()}, 10, '    check timeout');
    #     $self->testResult(sub {$oIoBuffered->bufferMax()}, 2048, '    check buffer max');
    # }

    ################################################################################################################################
    if ($self->begin('process()'))
    {
        my $oClient = $self->socketServer($strSocketFile, sub
        {
            my $oIoHandle = shift;

            my $oMinion = new pgBackRest::Protocol::Base::Minion('test', new pgBackRest::Common::Io::Buffered($oIoHandle, 5, 4096));

            # Use bogus lock path to ensure a lock is not taken for the archive-get command
            $oMinion->process($self->testPath(), cfgCommandName(CFGCMD_ARCHIVE_GET), "test", 0);
        });

        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoBuffered = $self->testResult(
            sub {new pgBackRest::Common::Io::Buffered(
                new pgBackRest::Common::Io::Handle('socket client', $oClient, $oClient), 5, 4096)}, '[object]', 'open');

        $self->testResult(
            sub {$oIoBuffered->readLine()},
            '{"name":"' . PROJECT_NAME . '","service":"test","version":"' . PROJECT_VERSION . '"}', 'read greeting');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oIoBuffered->writeLine('{"cmd":"noop"}')}, 15, 'write noop');
        $self->testResult(
            sub {$oIoBuffered->readLine()}, '{"out":[]}', 'read output');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oIoBuffered->writeLine('{"cmd":"exit"}')}, 15, 'write exit');
        $self->testResult(
            sub {$oIoBuffered->readLine(true)}, undef, 'read EOF');
    }
}

1;
