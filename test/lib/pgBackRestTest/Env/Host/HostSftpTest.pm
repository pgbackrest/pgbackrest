####################################################################################################################################
# SFTP Test Host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostSftpTest;
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
# SFTP defaults
####################################################################################################################################
use constant HOST_SFTP_ACCOUNT                                      => TEST_USER;
    push @EXPORT, qw(HOST_SFTP_ACCOUNT);
use constant HOST_SFTP_HOSTKEY_HASH_TYPE                            => 'sha1';
    push @EXPORT, qw(HOST_SFTP_HOSTKEY_HASH_TYPE);

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
        HOST_SFTP, 'test-' . testRunGet()->vmId() . '-' . HOST_SFTP,  containerRepo() . ':' . testRunGet()->vm() . '-test', 'root');
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

1;
