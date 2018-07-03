####################################################################################################################################
# S3 Authentication Tests
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageS3AuthTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use POSIX qw(strftime);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Storage::S3::Auth;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin('s3DateTime'))
    {
        $self->testResult(sub {s3DateTime(1491267845)}, '20170404T010405Z', 'format date/time');

        #---------------------------------------------------------------------------------------------------------------------------
        waitRemainder();
        $self->testResult(sub {s3DateTime()}, strftime("%Y%m%dT%H%M%SZ", gmtime()), 'format current date/time');
    }

    ################################################################################################################################
    if ($self->begin('s3CanonicalRequest'))
    {
        $self->testResult(
            sub {s3CanonicalRequest(
                'GET', qw(/), 'list-type=2',
                {'host' => 'bucket.s3.amazonaws.com', 'x-amz-date' => '20170606T121212Z',
                    'x-amz-content-sha256' => '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9'},
                '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9')},
            "(GET\n/\nlist-type=2\nhost:bucket.s3.amazonaws.com\n" .
                "x-amz-content-sha256:705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9\n" .
                "x-amz-date:20170606T121212Z\n\nhost;x-amz-content-sha256;x-amz-date\n" .
                '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9' .
                ', host;x-amz-content-sha256;x-amz-date)',
            'canonical request');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {s3CanonicalRequest(
                'GET', qw(/), 'list-type=2', {'Host' => 'bucket.s3.amazonaws.com'},
                '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9')},
            ERROR_ASSERT, "header 'Host' must be lower case");
    }

    ################################################################################################################################
    if ($self->begin('s3SigningKey'))
    {
        $self->testResult(
            sub {unpack('H*', s3SigningKey('20170412', 'us-east-1', 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY'))},
            '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9', 'signing key');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {unpack('H*', s3SigningKey('20170412', 'us-east-1', 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY'))},
            '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9', 'same signing key from cache');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {unpack('H*', s3SigningKey('20170505', 'us-west-1', 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY'))},
            'c1a1cb590bbc38ba789c8e5695a1ec0cd7fd44c6949f922e149005a221524c09', 'new signing key');
    }

    ################################################################################################################################
    if ($self->begin('s3StringToSign'))
    {
        $self->testResult(
            sub {s3StringToSign(
                '20170412T141414Z', 'us-east-1', '705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9')},
            "AWS4-HMAC-SHA256\n20170412T141414Z\n20170412/us-east-1/s3/aws4_request\n" .
                "705636ecdedffc09f140497bcac3be1e8d069008ecc6a8029e104d6291b4e4e9",
            'string to sign');
    }

    ################################################################################################################################
    if ($self->begin('s3AuthorizationHeader'))
    {
        $self->testResult(
            sub {s3AuthorizationHeader(
                'us-east-1', 'bucket.s3.amazonaws.com', 'GET', qw(/), 'list-type=2', '20170606T121212Z',
                {'authorization' => BOGUS, 'host' => 'bucket.s3.amazonaws.com', 'x-amz-date' => '20170606T121212Z'},
                'AKIAIOSFODNN7EXAMPLE', 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY',
                'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')},
            '({authorization => AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,' .
                'SignedHeaders=host;x-amz-content-sha256;x-amz-date,' .
                'Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3,' .
                ' host => bucket.s3.amazonaws.com,' .
                ' x-amz-content-sha256 => e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855,' .
                ' x-amz-date => 20170606T121212Z}, ' .
            "GET\n" .
            "/\n" .
            "list-type=2\n" .
            "host:bucket.s3.amazonaws.com\n" .
            "x-amz-content-sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n" .
            "x-amz-date:20170606T121212Z\n" .
            "\n" .
            "host;x-amz-content-sha256;x-amz-date\n" .
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855, " .
            "host;x-amz-content-sha256;x-amz-date, " .
            "AWS4-HMAC-SHA256\n" .
            "20170606T121212Z\n" .
            "20170606/us-east-1/s3/aws4_request\n" .
            "4f2d4ee971f579e60ba6b3895e87434e17b1260f04392f02b512c1e8bada72dd)",
            'authorization header request');
    }
}

1;
