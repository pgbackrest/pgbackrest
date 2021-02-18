####################################################################################################################################
# GCS Test Host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostGcsTest;
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
use constant HOST_GCS_BUCKET                                        => 'gcsbucket';
    push @EXPORT, qw(HOST_GCS_BUCKET);
use constant HOST_GCS_KEY                                           => 'testkey';
    push @EXPORT, qw(HOST_GCS_KEY);
use constant HOST_GCS_KEY_TYPE                                      => 'token';
    push @EXPORT, qw(HOST_GCS_KEY_TYPE);
use constant HOST_GCS_PORT                                          => 4443;
    push @EXPORT, qw(HOST_GCS_PORT);

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
        HOST_GCS, 'test-' . testRunGet()->vmId() . '-' . HOST_GCS, 'fsouza/fake-gcs-server', 'root', 'u18', undef, undef, undef,
        false);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

1;
