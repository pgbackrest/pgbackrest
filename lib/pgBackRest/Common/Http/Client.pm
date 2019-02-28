####################################################################################################################################
# HTTP Client
####################################################################################################################################
package pgBackRest::Common::Http::Client;
use parent 'pgBackRest::Common::Io::Buffered';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use IO::Socket::SSL;
use Socket qw(SOL_SOCKET SO_KEEPALIVE);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Xml;
use pgBackRest::Common::Http::Common;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant HTTP_VERB_GET                                          => 'GET';
    push @EXPORT, qw(HTTP_VERB_GET);
use constant HTTP_VERB_POST                                         => 'POST';
    push @EXPORT, qw(HTTP_VERB_POST);
use constant HTTP_VERB_PUT                                          => 'PUT';
    push @EXPORT, qw(HTTP_VERB_PUT);

use constant HTTP_HEADER_CONTENT_LENGTH                             => 'content-length';
    push @EXPORT, qw(HTTP_HEADER_CONTENT_LENGTH);
use constant HTTP_HEADER_TRANSFER_ENCODING                          => 'transfer-encoding';
    push @EXPORT, qw(HTTP_HEADER_TRANSFER_ENCODING);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strHost,
        $strVerb,
        $iPort,
        $strUri,
        $hQuery,
        $hRequestHeader,
        $rstrRequestBody,
        $bResponseBodyPrefetch,
        $iProtocolTimeout,
        $iTryTotal,
        $lBufferMax,
        $bVerifySsl,
        $strCaPath,
        $strCaFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strHost', trace => true},
            {name => 'strVerb', trace => true},
            {name => 'iPort', optional => true, default => 443, trace => true},
            {name => 'strUri', optional => true, default => qw(/), trace => true},
            {name => 'hQuery', optional => true, trace => true},
            {name => 'hRequestHeader', optional => true, trace => true},
            {name => 'rstrRequestBody', optional => true, trace => true},
            {name => 'bResponseBodyPrefetch', optional => true, default => false, trace => true},
            {name => 'iProtocolTimeout', optional => true, default => 300, trace => true},
            {name => 'iTryTotal', optional => true, default => 3, trace => true},
            {name => 'lBufferMax', optional => true, default => 32768, trace => true},
            {name => 'bVerifySsl', optional => true, default => true, trace => true},
            {name => 'strCaPath', optional => true, trace => true},
            {name => 'strCaFile', optional => true, trace => true},
        );

    # Retry as many times as requested
    my $self;
    my $iTry = 1;
    my $bRetry;

    do
    {
        # Disable logging if a failure will be retried
        logDisable() if $iTry < $iTryTotal;
        $bRetry = false;

        eval
        {
            # Connect to the server
            my $oSocket;

            if (eval{require IO::Socket::IP})
            {
                $oSocket = IO::Socket::IP->new(PeerHost => $strHost, PeerPort => $iPort)
                    or confess &log(ERROR, "unable to create socket: $@", ERROR_HOST_CONNECT);
            }
            else
            {
                require IO::Socket::INET;

                $oSocket = IO::Socket::INET->new(PeerHost => $strHost, PeerPort => $iPort)
                    or confess &log(ERROR, "unable to create socket: $@", ERROR_HOST_CONNECT);
            }

            setsockopt($oSocket, SOL_SOCKET,SO_KEEPALIVE, 1)
                or confess &log(ERROR, "unable to set socket keepalive: $@", ERROR_HOST_CONNECT);

            eval
            {
                IO::Socket::SSL->start_SSL(
                    $oSocket, SSL_verify_mode => $bVerifySsl ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, SSL_ca_path => $strCaPath,
                    SSL_ca_file => $strCaFile);
            }
            or do
            {
                logErrorResult(
                    ERROR_HOST_CONNECT, coalesce(length($!) == 0 ? undef : $!, $SSL_ERROR), length($!) > 0 ? $SSL_ERROR : undef);
            };

            # Bless with new class
            $self = $class->SUPER::new(
                new pgBackRest::Common::Io::Handle('httpClient', $oSocket, $oSocket), $iProtocolTimeout, $lBufferMax);
            bless $self, $class;

            # Store socket
            $self->{oSocket} = $oSocket;

            # Generate the query string
            my $strQuery = httpQuery($hQuery);

            # Construct the request headers
            $self->{strRequestHeader} = "${strVerb} " . httpUriEncode($strUri, true) . "?${strQuery} HTTP/1.1" . "\r\n";

            foreach my $strHeader (sort(keys(%{$hRequestHeader})))
            {
                $self->{strRequestHeader} .= "${strHeader}: $hRequestHeader->{$strHeader}\r\n";
            }

            $self->{strRequestHeader} .= "\r\n";

            # Write request headers
            $self->write(\$self->{strRequestHeader});

            # Write content
            if (defined($rstrRequestBody))
            {
                my $iTotalSize = length($$rstrRequestBody);
                my $iTotalSent = 0;

                # Write the request body in buffer-sized chunks
                do
                {
                    my $strBufferWrite = substr($$rstrRequestBody, $iTotalSent, $lBufferMax);
                    $iTotalSent += $self->write(\$strBufferWrite);
                } while ($iTotalSent < $iTotalSize);
            }

            # Read response code
            ($self->{strResponseProtocol}, $self->{iResponseCode}, $self->{strResponseMessage}) =
                split(' ', trim($self->readLine()));

            # Read the response headers
            $self->{iContentLength} = 0;
            $self->{strResponseHeader} = '';
            my $strHeader = trim($self->readLine());

            while ($strHeader ne '')
            {
                # Validate header
                $self->{strResponseHeader} .= "${strHeader}\n";

                my $iColonPos = index($strHeader, ':');

                if ($iColonPos == -1)
                {
                    confess &log(ERROR, "http header '${strHeader}' requires colon separator", ERROR_PROTOCOL);
                }

                # Parse header
                my $strHeaderKey = lc(substr($strHeader, 0, $iColonPos));
                my $strHeaderValue = trim(substr($strHeader, $iColonPos + 1));

                # Store the header
                $self->{hResponseHeader}{$strHeaderKey} = $strHeaderValue;

                # Process content length
                if ($strHeaderKey eq HTTP_HEADER_CONTENT_LENGTH)
                {
                    $self->{iContentLength} = $strHeaderValue + 0;
                    $self->{iContentRemaining} = $self->{iContentLength};
                }
                # Process transfer encoding (only chunked is supported)
                elsif ($strHeaderKey eq HTTP_HEADER_TRANSFER_ENCODING)
                {
                    if ($strHeaderValue eq 'chunked')
                    {
                        $self->{iContentLength} = -1;
                    }
                    else
                    {
                        confess &log(ERROR, "invalid value '${strHeaderValue} for http header '${strHeaderKey}'", ERROR_PROTOCOL);
                    }
                }

                # Read next header
                $strHeader = trim($self->readLine());
            }

            # Prefetch response - mostly useful when the response is known to be short
            if ($bResponseBodyPrefetch)
            {
                $self->{strResponseBody} = $self->responseBody();
            }

            # Enable logging if a failure will be retried
            logEnable() if $iTry < $iTryTotal;
            return 1;
        }
        or do
        {
            # Enable logging if a failure will be retried
            logEnable() if $iTry < $iTryTotal;

            # If tries reaches total allowed then error
            if ($iTry == $iTryTotal)
            {
                confess $EVAL_ERROR;
            }

            # Try again
            $iTry++;
            $bRetry = true;
        };
    }
    while ($bRetry);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - read content from http stream
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iRequestSize = shift;

    # Make sure request size is not larger than what remains to be read
    $iRequestSize = $iRequestSize < $self->{iContentRemaining} ? $iRequestSize : $self->{iContentRemaining};
    $self->{iContentRemaining} -= $iRequestSize;

    my $iActualSize = $self->SUPER::read($rtBuffer, $iRequestSize, true);

    # Set eof if there is nothing left to read
    if ($self->{iContentRemaining} == 0)
    {
        $self->SUPER::eofSet(true);
    }

    return $iActualSize;
}

####################################################################################################################################
# close/DESTROY - close the HTTP connection
####################################################################################################################################
sub close
{
    my $self = shift;

    # Only close if the socket is open
    if (defined($self->{oSocket}))
    {
        $self->{oSocket}->close();
        undef($self->{oSocket});
    }
}

sub DESTROY {shift->close()}

####################################################################################################################################
# responseBody - return the entire body of the response in a buffer
####################################################################################################################################
sub responseBody
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->responseBody'
        );

    # Return prefetched response body if it exists
    return $self->{strResponseBody} if exists($self->{strResponseBody});

    # Fetch response body if content length is not 0
    my $strResponseBody = undef;

    if ($self->{iContentLength} != 0)
    {
        # Transfer encoding is chunked
        if ($self->{iContentLength} == -1)
        {
            while (1)
            {
                # Read chunk length
                my $strChunkLength = trim($self->readLine());
                my $iChunkLength = hex($strChunkLength);

                # Exit if chunk length is 0
                last if ($iChunkLength == 0);

                # Read the chunk and consume the terminating LF
                $self->SUPER::read(\$strResponseBody, $iChunkLength, true);
                $self->readLine();
            };
        }
        # Else content length is known
        else
        {
            $self->SUPER::read(\$strResponseBody, $self->{iContentLength}, true);
        }

        $self->close();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rstrResponseBody', value => \$strResponseBody, trace => true}
    );
}

####################################################################################################################################
# Properties.
####################################################################################################################################
sub contentLength {shift->{iContentLength}}                         # Content length if available (-1 means not known yet)
sub requestHeaderText {trim(shift->{strRequestHeader})}
sub responseCode {shift->{iResponseCode}}
sub responseHeader {shift->{hResponseHeader}}
sub responseHeaderText {trim(shift->{strResponseHeader})}
sub responseMessage {shift->{strResponseMessage}}
sub responseProtocol {shift->{strResponseProtocol}}

1;
