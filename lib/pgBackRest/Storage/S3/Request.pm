####################################################################################################################################
# Process S3 requests
####################################################################################################################################
package pgBackRest::Storage::S3::Request;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(hmac_sha256 hmac_sha256_hex sha256_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use IO::Socket::SSL;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Xml;
use pgBackRest::Common::Http::Client;
use pgBackRest::Common::Http::Common;
use pgBackRest::Storage::S3::Auth;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant HTTP_VERB_GET                                          => 'GET';
    push @EXPORT, qw(HTTP_VERB_GET);
use constant HTTP_VERB_POST                                         => 'POST';
    push @EXPORT, qw(HTTP_VERB_POST);
use constant HTTP_VERB_PUT                                          => 'PUT';
    push @EXPORT, qw(HTTP_VERB_PUT);

use constant S3_HEADER_CONTENT_LENGTH                               => 'content-length';
    push @EXPORT, qw(S3_HEADER_CONTENT_LENGTH);
use constant S3_HEADER_TRANSFER_ENCODING                            => 'transfer-encoding';
    push @EXPORT, qw(S3_HEADER_TRANSFER_ENCODING);
use constant S3_HEADER_ETAG                                         => 'etag';
    push @EXPORT, qw(S3_HEADER_ETAG);

use constant S3_RESPONSE_TYPE_IO                                    => 'io';
    push @EXPORT, qw(S3_RESPONSE_TYPE_IO);
use constant S3_RESPONSE_TYPE_NONE                                  => 'none';
    push @EXPORT, qw(S3_RESPONSE_TYPE_NONE);
use constant S3_RESPONSE_TYPE_XML                                   => 'xml';
    push @EXPORT, qw(S3_RESPONSE_TYPE_XML);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strBucket},
        $self->{strEndPoint},
        $self->{strRegion},
        $self->{strAccessKeyId},
        $self->{strSecretAccessKey},
        $self->{strHost},
        $self->{bVerifySsl},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strBucket', trace => true},
            {name => 'strEndPoint', trace => true},
            {name => 'strRegion', trace => true},
            {name => 'strAccessKeyId', trace => true},
            {name => 'strSecretAccessKey', trace => true},
            {name => 'strHost', optional => true, trace => true},
            {name => 'bVerifySsl', optional => true, default => true, trace => true},
        );

    # If host is not set then it will be bucket + endpoint
    $self->{strHost} = defined($self->{strHost}) ? $self->{strHost} : "$self->{strBucket}.$self->{strEndPoint}";

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# request - send a request to S3.
####################################################################################################################################
sub request
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strVerb,
        $strUri,
        $hQuery,
        $hHeader,
        $rstrBody,
        $strResponseType,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->request', \@_,
            {name => 'strVerb', trace => true},
            {name => 'strUri', optional => true, default => '/', trace => true},
            {name => 'hQuery', optional => true, trace => true},
            {name => 'hHeader', optional => true, trace => true},
            {name => 'rstrBody', optional => true, trace => true},
            {name => 'strResponseType', optional => true, default => S3_RESPONSE_TYPE_NONE, trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Get datetime to be used for auth requests
    my $strDateTime = s3DateTime();

    # Set content length and hash
    $hHeader->{&S3_HEADER_CONTENT_SHA256} = defined($rstrBody) ? sha256_hex($$rstrBody) : PAYLOAD_DEFAULT_HASH;
    $hHeader->{&S3_HEADER_CONTENT_LENGTH} = defined($rstrBody) ? length($$rstrBody) : 0;

    # Generate authorization header
    $hHeader = s3AuthorizationHeader(
        $self->{strRegion}, "$self->{strBucket}.$self->{strEndPoint}", $strVerb, $strUri, httpQuery($hQuery), $strDateTime,
        $hHeader, $self->{strAccessKeyId}, $self->{strSecretAccessKey}, $hHeader->{&S3_HEADER_CONTENT_SHA256});

    # Send the request
    my $oHttpClient = new pgBackRest::Common::Http::Client(
        $self->{strHost}, $strVerb,
        {strUri => $strUri, hQuery => $hQuery, hRequestHeader => $hHeader, rstrRequestBody => $rstrBody,
            bVerifySsl => $self->{bVerifySsl}});

    # Check response code
    my $iReponseCode = $oHttpClient->responseCode();
    my $oResponse;

    if ($oHttpClient->responseCode() == 200)
    {
        # Save the response headers locally
        $self->{hResponseHeader} = $oHttpClient->responseHeader();

        # XML response is expected
        if ($strResponseType eq S3_RESPONSE_TYPE_XML)
        {
            my $rtResponseBody = $oHttpClient->responseBody();

            if ($oHttpClient->contentLength() == 0 || !defined($$rtResponseBody))
            {
                confess &log(ERROR,
                    "response type '${strResponseType}' was requested but content length is zero or content is missing",
                    ERROR_PROTOCOL);
            }

            $oResponse = xmlParse($$rtResponseBody);
        }
        # An IO object is expected for file responses
        elsif ($strResponseType eq S3_RESPONSE_TYPE_IO)
        {
            $oResponse = $oHttpClient;
        }
    }
    else
    {
        if ($oHttpClient->responseCode() == 404)
        {
            if (!$bIgnoreMissing)
            {
                confess &log(ERROR, "unable to open '${strUri}': No such file or directory", ERROR_FILE_MISSING);
            }
        }
        else
        {
            my $rstrResponseBody = $oHttpClient->responseBody();

            confess &log(ERROR,
                "S3 request error [$iReponseCode] " . $oHttpClient->responseCode() .
                    (defined($$rstrResponseBody) ? ":\n${$rstrResponseBody}" : ''),
                ERROR_PROTOCOL);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oResponse', value => $oResponse, trace => true, ref => true}
    );
}

1;
