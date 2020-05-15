####################################################################################################################################
# Azure Test Host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostAzureTest;
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
# Azure defaults
####################################################################################################################################
use constant HOST_AZURE_ACCOUNT                                     => 'azAccount';
    push @EXPORT, qw(HOST_AZURE_ACCOUNT);
use constant HOST_AZURE_KEY                                         => 'YXpLZXk=';
    push @EXPORT, qw(HOST_AZURE_KEY);
use constant HOST_AZURE_CONTAINER                                   => 'azContainer';
    push @EXPORT, qw(HOST_AZURE_CONTAINER);

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
        HOST_AZURE, 'test-' . testRunGet()->vmId() . '-' . HOST_AZURE, 'mcr.microsoft.com/azure-storage/azurite', 'root', 'u18',
        ["${strFakeCertPath}/s3-server.crt:/root/public.crt:ro",
            "${strFakeCertPath}/s3-server.key:/root/private.key:ro"],
        '-e AZURITE_ACCOUNTS="' . HOST_AZURE_ACCOUNT . ':' . HOST_AZURE_KEY . '"',
        'azurite-blob --blobPort 443 --blobHost 0.0.0.0 --cert=/root/public.crt --key=/root/private.key -d debug.log', false);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

1;
