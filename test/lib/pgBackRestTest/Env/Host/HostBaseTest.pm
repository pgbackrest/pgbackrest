####################################################################################################################################
# HostBackupTest.pm - Backup host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostBaseTest;
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

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::JobTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Host constants
####################################################################################################################################
use constant HOST_BASE                                              => 'base';
    push @EXPORT, qw(HOST_BASE);
use constant HOST_DB_PRIMARY                                        => 'db-primary';
    push @EXPORT, qw(HOST_DB_PRIMARY);
use constant HOST_DB_STANDBY                                        => 'db-standby';
    push @EXPORT, qw(HOST_DB_STANDBY);
use constant HOST_BACKUP                                            => 'backup';
    push @EXPORT, qw(HOST_BACKUP);
use constant HOST_GCS                                               => 'gcs';
    push @EXPORT, qw(HOST_GCS);
use constant HOST_AZURE                                             => 'azure';
    push @EXPORT, qw(HOST_AZURE);
use constant HOST_S3                                                => 's3-server';
    push @EXPORT, qw(HOST_S3);

####################################################################################################################################
# CA/cert/key constants
####################################################################################################################################
use constant HOST_CERT_PATH                                         => '/test/certificate/';

use constant HOST_CLIENT_CERT                                       => HOST_CERT_PATH . 'pgbackrest-test-client.crt';
    push @EXPORT, qw(HOST_CLIENT_CERT);
use constant HOST_CLIENT_KEY                                        => HOST_CERT_PATH . 'pgbackrest-test-client.key';
    push @EXPORT, qw(HOST_CLIENT_KEY);

use constant HOST_SERVER_CA                                         => HOST_CERT_PATH . 'pgbackrest-test-ca.crt';
    push @EXPORT, qw(HOST_SERVER_CA);
use constant HOST_SERVER_CERT                                       => HOST_CERT_PATH . 'pgbackrest-test-server.crt';
use constant HOST_SERVER_KEY                                        => HOST_CERT_PATH . 'pgbackrest-test-server.key';

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
        $strName,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName', default => HOST_BASE, trace => true},
            {name => 'oParam', required => false, trace => true},
        );

    my $strTestPath = testRunGet()->testPath() . ($strName eq HOST_BASE ? '' : "/${strName}");
    storageTest()->pathCreate($strTestPath, {strMode => '0770'});

    # Make sure keys have the correct permissions
    if (chmod(0600, testRunGet()->basePath() . HOST_SERVER_KEY, testRunGet()->basePath() . HOST_CLIENT_KEY) != 2)
    {
        confess "unable to set mode on keys";
    }

    # Create the host
    my $strProjectPath = dirname(dirname(abs_path($0)));
    my $strBinPath = dirname(dirname($strTestPath)) . '/bin/' . testRunGet()->vm() . '/' . PROJECT_EXE;
    my $strContainer = 'test-' . testRunGet()->vmId() . "-$strName";

    my $self = $class->SUPER::new(
        $strName, $strContainer, $$oParam{strImage}, $$oParam{strUser}, testRunGet()->vm(),
        ["${strProjectPath}:${strProjectPath}", "${strTestPath}:${strTestPath}", "${strBinPath}:${strBinPath}:ro"], undef,
        $oParam->{bTls} ?
            'server --log-level-console=debug --tls-server-ca=' . testRunGet()->basePath() . HOST_SERVER_CA .
                ' --tls-server-cert=' . testRunGet()->basePath() . HOST_SERVER_CERT . ' --tls-server-key=' .
                testRunGet()->basePath() . HOST_SERVER_KEY . ' --tls-server-allow=pgbackrest-client=*' :
            undef,
        undef, $oParam->{bTls} ? testRunGet()->backrestExe() : undef);
    bless $self, $class;

    # Set test path
    $self->{strTestPath} = $strTestPath;

    # Set permissions on the test path
    $self->executeSimple('chown -R ' . $self->userGet() . ':'. TEST_GROUP . ' ' . $self->testPath(), undef, 'root');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub testPath {return shift->{strTestPath}}

1;
