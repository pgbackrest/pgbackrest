####################################################################################################################################
# S3 SSL Certificate Tests
#
# Verify that SSL certificate validation works on live S3 servers.
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageS3CertTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Use long random string so bucket lookups will fail and expose access errors
    my $strBucket = 'bnBfyKpXR8ZqQY5RXszxemRgvtmjXd4tf5HkFYhTpT9BndUCYMDy5NCCyRz';
    my $strEndpoint = 's3-us-west-2.amazonaws.com';
    my $strRegion = 'us-west-2';

    # Options
    my $oOptionGlobal = {};
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_TYPE, REPO_TYPE_S3);
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_S3_KEY, BOGUS);
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_S3_KEY_SECRET, BOGUS);
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_S3_BUCKET, $strBucket);
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_S3_ENDPOINT, $strEndpoint);
    $self->optionSetTest($oOptionGlobal, OPTION_REPO_S3_REGION, $strRegion);

    $self->optionSetTest($oOptionGlobal, OPTION_STANZA, $self->stanza());

    ################################################################################################################################
    if ($self->begin('validation'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        if ($self->vm() eq VM_CO7)
        {
            # Tests fails on co7 because by default certs cannot be located.  This logic may need to be changed in the future if
            # this bug gets fixed by Red Hat.
            $self->testResult(sub {$self->configLoadExpect(dclone($oOptionGlobal), CMD_ARCHIVE_PUSH)}, '', 'config load');

            $self->testException(
                sub {storageRepo({strStanza => 'test1'})->list('/')}, ERROR_HOST_CONNECT,
                'IO::Socket::IP configuration failed SSL connect attempt failed.*certificate verify failed',
                'cert verify fails on ' . VM_CO7);

            # It should work when verification is disabled
            my $oOptionLocal = dclone($oOptionGlobal);
            $self->optionBoolSetTest($oOptionLocal, OPTION_REPO_S3_VERIFY_SSL, false);
            $self->testResult(sub {$self->configLoadExpect($oOptionLocal, CMD_ARCHIVE_PUSH)}, '', 'config load');

            $self->testException(
                sub {storageRepo({strStanza => 'test2'})->list('/')}, ERROR_PROTOCOL, 'S3 request error \[403\] Forbidden.*',
                'connection succeeds with verification disabled, (expected) error on invalid access key');
        }

        #---------------------------------------------------------------------------------------------------------------------------
        my $oOptionLocal = dclone($oOptionGlobal);

        # CO7 doesn't locate certs automatically so specify the path
        if ($self->vm() eq VM_CO7)
        {
            $self->optionSetTest($oOptionLocal, OPTION_REPO_S3_CA_FILE, '/etc/pki/tls/certs/ca-bundle.crt');
        }

        $self->testResult(sub {$self->configLoadExpect($oOptionLocal, CMD_ARCHIVE_PUSH)}, '', 'config load');

        $self->testException(
            sub {storageRepo({strStanza => 'test3'})->list('/')}, ERROR_PROTOCOL, 'S3 request error \[403\] Forbidden.*',
            'connection succeeds, (expected) error on invalid access key');

        #---------------------------------------------------------------------------------------------------------------------------
        $oOptionLocal = dclone($oOptionGlobal);
        $self->optionSetTest($oOptionLocal, OPTION_REPO_S3_CA_PATH, '/bogus');
        $self->testResult(sub {$self->configLoadExpect($oOptionLocal, CMD_ARCHIVE_PUSH)}, '', 'config load');

        $self->testException(
            sub {storageRepo({strStanza => 'test4'})->list('/')}, ERROR_HOST_CONNECT,
            $self->vm() eq VM_CO6 ? 'IO::Socket::INET configuration failed' : 'SSL_ca_path /bogus does not exist',
            'invalid ca path');
    }
}

1;
