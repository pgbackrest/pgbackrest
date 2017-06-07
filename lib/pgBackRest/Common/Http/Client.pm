####################################################################################################################################
# HTTP Client
####################################################################################################################################
package pgBackRest::Common::Http::Client;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use IO::Socket::SSL;

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

use constant HTTP_HEADER_CONTENT_LENGTH                               => 'content-length';
    push @EXPORT, qw(HTTP_HEADER_CONTENT_LENGTH);
use constant HTTP_HEADER_TRANSFER_ENCODING                            => 'transfer-encoding';
    push @EXPORT, qw(HTTP_HEADER_TRANSFER_ENCODING);

####################################################################################################################################
# new
####################################################################################################################################
our @ISA = ();                                                      ## no critic (ClassHierarchies::ProhibitExplicitISA)

sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strHost,
        $strVerb,
        $strUri,
        $hQuery,
        $hRequestHeader,
        $rstrRequestBody,
        $iProtocolTimeout,
        $lBufferMax,
        $bVerifySsl,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strHost', trace => true},
            {name => 'strVerb', trace => true},
            {name => 'strUri', optional => true, default => qw(/), trace => true},
            {name => 'hQuery', optional => true, trace => true},
            {name => 'hRequestHeader', optional => true, trace => true},
            {name => 'rstrRequestBody', optional => true, trace => true},
            {name => 'iProtocolTimeout', optional => true, default => 30, trace => true},
            {name => 'lBufferMax', optional => true, default => 32768, trace => true},
            {name => 'bVerifySsl', optional => true, default => true, trace => true},
        );

    # Connect to the server
    my $oSocket = IO::Socket::SSL->new(
        PeerHost => $strHost, PeerPort => 'https', SSL_verify_mode => $bVerifySsl ? SSL_VERIFY_PEER : SSL_VERIFY_NONE);

    if (!defined($oSocket))
    {
        logErrorResult(ERROR_PROTOCOL, "unable to connect $!", $SSL_ERROR);
    }

    # Create the buffered IO object
    my $self = new pgBackRest::Common::Io::Buffered(
        new pgBackRest::Common::Io::Handle('httpClient', $oSocket, $oSocket), $iProtocolTimeout, $lBufferMax);

    # Bless with the class
    @ISA = $self->isA();                                            ## no critic (ClassHierarchies::ProhibitExplicitISA)
    bless $self, $class;

    # Store socket
    $self->{oSocket} = $oSocket;

    # Generate the query string
    my $strQuery = httpQuery($hQuery);

    # Construct the request headers
    $self->{strRequestHeader} = "${strVerb} ${strUri}?${strQuery} HTTP/1.1" . "\r\n";

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
    ($self->{strResponseProtocol}, $self->{iResponseCode}, $self->{strResponseMessage}) = split(' ', trim($self->readLine()));

    # Read the response headers
    $self->{iContentLength} = undef;

    my $strResponseHeader = '';
    my $strHeader = trim($self->readLine());

    while ($strHeader ne '')
    {
        # Validate header
        $strResponseHeader .= "${strHeader}\n";

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

    # Test response code
    if ($self->{iResponseCode} == 200)
    {
        # Content length should have been defined either by content-length or transfer encoding
        if (!defined($self->{iContentLength}))
        {
            confess &log(ERROR,
                HTTP_HEADER_CONTENT_LENGTH . ' or ' . HTTP_HEADER_TRANSFER_ENCODING . ' must be defined', ERROR_PROTOCOL);
        }
    }

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

    return $self->SUPER::read($rtBuffer, $iRequestSize, true);
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

    my $strResponseBody = undef;

    # Nothing to do if content length is 0
    if ($self->{iContentLength} != 0)
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
sub requestHeader {shift->{hResponseHeader}}
sub responseCode {shift->{iResponseCode}}
sub responseHeader {shift->{hResponseHeader}}
sub responseMessage {shift->{strResponseMessage}}
sub responseProtocol {shift->{strResponseProtocol}}

1;
