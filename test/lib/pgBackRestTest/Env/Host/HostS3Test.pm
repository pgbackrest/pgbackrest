####################################################################################################################################
# S3 Test Host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostS3Test;
use parent 'pgBackRestTest::Common::HostTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::Wait;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Manifest;

####################################################################################################################################
# S3 defaults
####################################################################################################################################
use constant HOST_S3_ACCESS_KEY                                     => 'accessKey1';
    push @EXPORT, qw(HOST_S3_ACCESS_KEY);
use constant HOST_S3_ACCESS_SECRET_KEY                              => 'verySecretKey1';
    push @EXPORT, qw(HOST_S3_ACCESS_SECRET_KEY);
use constant HOST_S3_BUCKET                                         => 'pgbackrest-dev';
    push @EXPORT, qw(HOST_S3_BUCKET);
use constant HOST_S3_ENDPOINT                                       => 's3.amazonaws.com';
    push @EXPORT, qw(HOST_S3_ENDPOINT);
use constant HOST_S3_REGION                                         => 'us-east-1';
    push @EXPORT, qw(HOST_S3_REGION);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
        );

    # Create the host
    my $strProjectPath = dirname(dirname(abs_path($0)));
    my $strFakeCertPath = "${strProjectPath}/doc/resource/fake-cert";

    my $self = $class->SUPER::new(
        HOST_S3, 'test-' . testRunGet()->vmId() . '-s3-server', 'minio/minio:RELEASE.2022-07-30T05-21-40Z', 'root', 'u18',
        ["${strFakeCertPath}/s3-server.crt:/root/.minio/certs/public.crt:ro",
            "${strFakeCertPath}/s3-server.key:/root/.minio/certs/private.key:ro"],
        '-e MINIO_REGION=' . HOST_S3_REGION . ' -e MINIO_DOMAIN=' . HOST_S3_ENDPOINT . ' -e MINIO_BROWSER=off' .
            ' -e MINIO_ACCESS_KEY=' . HOST_S3_ACCESS_KEY . ' -e MINIO_SECRET_KEY=' . HOST_S3_ACCESS_SECRET_KEY,
        'server /data --address :443', false);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

1;
