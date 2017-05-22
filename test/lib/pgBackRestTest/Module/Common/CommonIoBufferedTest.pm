####################################################################################################################################
# CommonIoBufferedTest.pm - tests for Common::Io::Buffered module
####################################################################################################################################
package pgBackRestTest::Module::Common::CommonIoBufferedTest;
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
    if ($self->begin('new() & timeout() & bufferMax()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoBuffered = $self->testResult(
            sub {new pgBackRest::Common::Io::Buffered(
                new pgBackRest::Common::Io::Handle('test'), 10, 2048)}, '[object]', 'new - no handles');

        $self->testResult(sub {$oIoBuffered->timeout()}, 10, '    check timeout');
        $self->testResult(sub {$oIoBuffered->bufferMax()}, 2048, '    check buffer max');
    }

    ################################################################################################################################
    if ($self->begin('readLine() & writeLine()'))
    {
        my $oClient = $self->socketServer($strSocketFile, sub
        {
            my $oIoHandle = shift;

            my $tResponse = '';
            my $tData;

            # Ack after timeout
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};

            # Write 8 char line
            $tResponse = '';
            $tData = "12345678\n";
            $oIoHandle->write(\$tData);
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};

            # Write 3 lines
            $tResponse = '';
            $tData = "1\n2\n345678\n";
            $oIoHandle->write(\$tData);
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};

            # Write blank line
            $tResponse = '';
            $tData = "\n";
            $oIoHandle->write(\$tData);
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};
        });

        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoBuffered = $self->testResult(
            sub {new pgBackRest::Common::Io::Buffered(
                new pgBackRest::Common::Io::Handle('socket client', $oClient, $oClient), 1, 4)}, '[object]', 'open');

        $self->testException(
            sub {$oIoBuffered->readLine()}, ERROR_FILE_READ, 'unable to read line after 1 second(s) from socket client');

        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoBuffered->readLine()}, '12345678', 'read 8 char line');
        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoBuffered->readLine()}, '1', 'read 1 char line');
        $self->testResult(sub {$oIoBuffered->readLine()}, '2', 'read 1 char line');
        $self->testResult(sub {$oIoBuffered->readLine()}, '345678', 'read 6 char line');
        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoBuffered->readLine()}, '', 'read blank line');
        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oIoBuffered->readLine()}, ERROR_FILE_READ, 'unexpected EOF reading line from socket client');
        $self->testException(
            sub {$oIoBuffered->readLine(false)}, ERROR_FILE_READ, 'unexpected EOF reading line from socket client');
        $self->testResult(sub {$oIoBuffered->readLine(true)}, undef, 'ignore EOF');
    }

    ################################################################################################################################
    if ($self->begin('read'))
    {
        my $tBuffer;

        my $oClient = $self->socketServer($strSocketFile, sub
        {
            my $oIoHandle = shift;

            my $tResponse = '';
            my $tData;

            # Ack after timeout
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};

            # Write mixed buffer
            $tResponse = '';
            $tData = "123\n123\n1";
            $oIoHandle->write(\$tData);
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};

            # Write buffer
            $tResponse = '';
            $tData = "23456789";
            $oIoHandle->write(\$tData);

            # Wait before writing the rest to force client to loop
            usleep(500);
            $tData = "0";
            $oIoHandle->write(\$tData);
            while (length($tResponse) != 1) {$oIoHandle->read(\$tResponse, 1)};
        });

        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoBuffered = $self->testResult(
            sub {new pgBackRest::Common::Io::Buffered(
                new pgBackRest::Common::Io::Handle('socket client', $oClient, $oClient), 1, 8)}, '[object]', 'open');

        $self->testException(
            sub {$oIoBuffered->read(\$tBuffer, 1, true)}, ERROR_FILE_READ,
            'unable to read 1 byte(s) after 1 second(s) from socket client');

        $self->testResult(sub {$oIoBuffered->read(\$tBuffer, 1)}, 0, '    read 0 char buffer');

        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoBuffered->readLine()}, '123', '    read 3 char line');

        undef($tBuffer);
        $self->testResult(sub {$oIoBuffered->read(\$tBuffer, 1)}, 1, '    read 1 char buffer');
        $self->testResult(sub {$tBuffer}, '1', '    check 1 char buffer');

        $self->testResult(sub {$oIoBuffered->read(\$tBuffer, 3)}, 3, '    read 3 char buffer');
        $self->testResult(sub {$tBuffer}, "123\n", '    check 3 char buffer');

        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        undef($tBuffer);
        $self->testResult(sub {$oIoBuffered->read(\$tBuffer, 10)}, 10, '    read 10 char buffer');
        $self->testResult(sub {$tBuffer}, '1234567890', '    check 10 char buffer');

        $oIoBuffered->writeLine();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoBuffered->read(\$tBuffer, 5)}, 0, '    expect EOF');

        $self->testException(
            sub {$oIoBuffered->read(\$tBuffer, 5, true)}, ERROR_FILE_READ,
            'unable to read 5 byte(s) due to EOF from socket client');
    }
}

1;
