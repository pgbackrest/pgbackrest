####################################################################################################################################
# S3 Request Tests
####################################################################################################################################
package pgBackRestTest::Module::Common::CommonHttpClientPerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

    use IO::Socket::SSL;
use POSIX qw(strftime);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Http::Client;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Port to use for testing
####################################################################################################################################
use constant HTTPS_TEST_PORT                                        => 9443;

####################################################################################################################################
# httpsServerResponse
####################################################################################################################################
sub httpsServerResponse
{
    my $self = shift;
    my $iResponseCode = shift;
    my $strContent = shift;

    # Write header
    $self->{oConnection}->write("HTTP/1.1 ${iResponseCode} GenericMessage\r\n");
    $self->{oConnection}->write(HTTP_HEADER_CONTENT_LENGTH . ': ' . (defined($strContent) ? length($strContent) : 0) . "\r\n");

    # Write new line before content (even if there isn't any)
    $self->{oConnection}->write("\r\n");

    # Write content
    if (defined($strContent))
    {
        $self->{oConnection}->write($strContent);
    }

    # This will block until the connection is closed by the client
    $self->{oConnection}->read();
}

####################################################################################################################################
# httpsServerAccept
####################################################################################################################################
sub httpsServerAccept
{
    my $self = shift;

    # Wait for a connection
    $self->{oConnection} = $self->{oSocketServer}->accept()
        or confess "failed to accept or handshake $!, $SSL_ERROR";
    &log(INFO, "    * socket server connected");
}

####################################################################################################################################
# httpsServer
####################################################################################################################################
sub httpsServer
{
    my $self = shift;
    my $fnServer = shift;

    # Fork off the server
    if (fork() == 0)
    {
        # Run server function
        $fnServer->();

        exit 0;
    }
}

####################################################################################################################################
# Start the https testing server
####################################################################################################################################
sub initModule
{
    my $self = shift;

    # Open the domain socket
    $self->{oSocketServer} = IO::Socket::SSL->new(
        LocalAddr => '127.0.0.1', LocalPort => HTTPS_TEST_PORT, Listen => 1, SSL_cert_file => CERT_FAKE_SERVER,
        SSL_key_file => CERT_FAKE_SERVER_KEY)
        or confess "unable to open https server for testing: $!";
    &log(INFO, "    * socket server open");
}

####################################################################################################################################
# Stop the https testing server
####################################################################################################################################
sub cleanModule
{
    my $self = shift;

    # Shutdown server
    $self->{oSocketServer}->close();
    &log(INFO, "    * socket server closed");
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Test variables
    my $strTestHost = '127.0.0.1';
    my $strTestData = 'TESTDATA';

    ################################################################################################################################
    if ($self->begin('content-length defined'))
    {
        $self->httpsServer(sub
        {
            $self->httpsServerAccept();
            $self->httpsServerResponse(200, $strTestData);
        });

        #---------------------------------------------------------------------------------------------------------------------------
        my $oHttpClient = $self->testResult(
            sub {new pgBackRest::Common::Http::Client(
                $strTestHost, HTTP_VERB_GET, {iPort => HTTPS_TEST_PORT, bVerifySsl => false})},
            '[object]', 'new http client');

        $self->testResult(sub {${$oHttpClient->responseBody()}}, $strTestData, 'response body read');
    }

    ################################################################################################################################
    if ($self->begin('retry'))
    {
        $self->httpsServer(sub
        {
            $self->httpsServerAccept();
            $self->{oConnection}->write("HTTP/1.1 404 Error\r\nBogus-Header\r\n\r\n");

            $self->httpsServerAccept();
            $self->{oConnection}->write("HTTP/1.1 404 Error\r\nBogus-Header\r\n\r\n");

            $self->httpsServerAccept();
            $self->httpsServerResponse(200, $strTestData);
        });

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {new pgBackRest::Common::Http::Client(
                $strTestHost, HTTP_VERB_GET, {iPort => HTTPS_TEST_PORT, bVerifySsl => false, iTryTotal => 1})},
            ERROR_PROTOCOL, "http header 'Bogus-Header' requires colon separator");

        $self->testResult(
            sub {new pgBackRest::Common::Http::Client(
                $strTestHost, HTTP_VERB_GET, {iPort => HTTPS_TEST_PORT, bVerifySsl => false, iTryTotal => 2})},
            '[object]', 'successful retries');
    }
}

1;
