####################################################################################################################################
# HostBackupTest.pm - Backup host
####################################################################################################################################
package pgBackRestTest::Backup::Common::HostBaseTest;
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

use pgBackRest::Common::Log;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::HostGroupTest;

####################################################################################################################################
# Host constants
####################################################################################################################################
use constant HOST_BASE                                              => 'base';
    push @EXPORT, qw(HOST_BASE);
use constant HOST_DB_MASTER                                         => 'db-master';
    push @EXPORT, qw(HOST_DB_MASTER);
use constant HOST_DB_STANDBY                                        => 'db-standby';
    push @EXPORT, qw(HOST_DB_STANDBY);
use constant HOST_BACKUP                                            => 'backup';
    push @EXPORT, qw(HOST_BACKUP);

####################################################################################################################################
# Host parameters
####################################################################################################################################
use constant HOST_PARAM_VM_ID                                       => 'vm-id';
    push @EXPORT, qw(HOST_PARAM_VM_ID);
use constant HOST_PARAM_TEST_PATH                                   => 'test-path';
    push @EXPORT, qw(HOST_PARAM_TEST_PATH);
use constant HOST_PARAM_VM                                          => 'vm';
    push @EXPORT, qw(HOST_PARAM_VM);

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

    # Create the test path
    my $oHostGroup = hostGroupGet();
    my $strTestPath = $oHostGroup->paramGet(HOST_PARAM_TEST_PATH) . ($strName eq HOST_BASE ? '' : "/${strName}");
    filePathCreate($strTestPath, '0770');

    # Create the host
    my $strProjectPath = dirname(dirname(abs_path($0)));
    my $strContainer = 'test-' . $oHostGroup->paramGet(HOST_PARAM_VM_ID) . "-$strName";

    my $self = $class->SUPER::new(
        $strName, $strContainer, $$oParam{strImage}, $$oParam{strUser}, $$oParam{strVm},
        ["${strProjectPath}:${strProjectPath}", "${strTestPath}:${strTestPath}"]);
    bless $self, $class;

    # Set parameters
    $self->paramSet(HOST_PARAM_TEST_PATH, $strTestPath);

    # Set permissions on the test path
    $self->executeSimple('chown -R ' . $self->userGet() . ':'. POSTGRES_GROUP . ' ' . $self->testPath(), undef, 'root');

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
sub testPath {return shift->paramGet(HOST_PARAM_TEST_PATH);}

1;
