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

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;

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
    my $self = $class->SUPER::new(
        's3-server', 'test-' . testRunGet()->vmId() . '-s3-server', containerRepo() . ':' . testRunGet()->vm() . '-s3-server',
        'root', testRunGet()->vm());
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# executeS3
####################################################################################################################################
sub executeS3
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->executeS3', \@_,
            {name => 'strCommand', trace => true},
        );

    executeTest(
        'export PYTHONWARNINGS="ignore" && aws --endpoint-url=https://' . $self->ipGet() .
        ' s3 --no-verify-ssl ' . $strCommand);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
