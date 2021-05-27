####################################################################################################################################
# HostDbTest.pm - Database host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostDbCommonTest;
use parent 'pgBackRestTest::Env::Host::HostBackupTest';

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

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::Wait;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Manifest;

####################################################################################################################################
# Test WAL size
####################################################################################################################################
use constant PG_WAL_SIZE_TEST                                       => 16777216;

####################################################################################################################################
# Host defaults
####################################################################################################################################
use constant HOST_PATH_SPOOL                                        => 'spool';
use constant HOST_PATH_DB                                           => 'db';
use constant HOST_PATH_DB_BASE                                      => 'base';

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
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParam', required => false, trace => true},
        );

    # Get host group
    my $oHostGroup = hostGroupGet();

    # Is standby?
    my $bStandby = defined($$oParam{bStandby}) && $$oParam{bStandby} ? true : false;

    my $self = $class->SUPER::new(
        {
            strName => $bStandby ? HOST_DB_STANDBY : HOST_DB_PRIMARY,
            strImage => $$oParam{strImage},
            strBackupDestination => $$oParam{strBackupDestination},
            oLogTest => $$oParam{oLogTest},
            bSynthetic => $$oParam{bSynthetic},
            bRepoLocal => $oParam->{bRepoLocal},
            bRepoEncrypt => $oParam->{bRepoEncrypt},
        });
    bless $self, $class;

    # Set parameters
    $self->{bStandby} = $bStandby;

    $self->{strDbPath} = $self->testPath() . '/' . HOST_PATH_DB;
    $self->{strDbBasePath} = $self->dbPath() . '/' . HOST_PATH_DB_BASE;
    $self->{strTablespacePath} = $self->dbPath() . '/tablespace';

    storageTest()->pathCreate($self->dbBasePath(), {strMode => '0700', bCreateParent => true});

    $self->{strSpoolPath} = $self->testPath() . '/' . HOST_PATH_SPOOL;
    storageTest()->pathCreate($self->spoolPath());

    # Initialize linkRemap Hashes
    $self->{hLinkRemap} = {};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# archivePush
####################################################################################################################################
sub archivePush
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalPath,
        $strArchiveTestFile,
        $iArchiveNo,
        $iExpectedError,
        $bAsync,
        $strOptionalParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->archivePush', \@_,
            {name => 'strWalPath'},
            {name => 'strArchiveTestFile', required => false},
            {name => 'iArchiveNo', required => false},
            {name => 'iExpectedError', required => false},
            {name => 'bAsync', default => true},
            {name => 'strOptionalParam', required => false},
        );

    my $strSourceFile;

    if (defined($strArchiveTestFile))
    {
        $strSourceFile = "${strWalPath}/" . uc(sprintf('0000000100000001%08x', $iArchiveNo));

        storageTest()->copy($strArchiveTestFile, storageTest()->openWrite($strSourceFile, {bPathCreate => true}));

        storageTest()->pathCreate("${strWalPath}/archive_status/", {bIgnoreExists => true, bCreateParent => true});
        storageTest()->put("${strWalPath}/archive_status/" . uc(sprintf('0000000100000001%08x', $iArchiveNo)) . '.ready');
    }

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --log-level-console=warn --archive-push-queue-max=' . int(2 * PG_WAL_SIZE_TEST) .
        ' --stanza=' . $self->stanza() .
        ($bAsync ? '' : ' --no-archive-async') .
        " archive-push" . (defined($strSourceFile) ? " ${strSourceFile}" : '') .
        (defined($strOptionalParam) ? " ${strOptionalParam}" : ''),
        {iExpectedExitStatus => $iExpectedError, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# linkRemap
####################################################################################################################################
sub linkRemap
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strTarget,
        $strDestination
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->linkRemap', \@_,
            {name => 'strTarget'},
            {name => 'strDestination'},
        );

    ${$self->{hLinkRemap}}{$strTarget} = $strDestination;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub dbPath {return shift->{strDbPath};}

sub dbBasePath
{
    my $self = shift;
    my $iIndex = shift;

    return $self->{strDbBasePath} . (defined($iIndex) ? "-${iIndex}" : '');
}

sub spoolPath {return shift->{strSpoolPath}}
sub standby {return shift->{bStandby}}

sub tablespacePath
{
    my $self = shift;
    my $iTablespace = shift;
    my $iIndex = shift;

    return
        $self->{strTablespacePath} .
        (defined($iTablespace) ? "/ts${iTablespace}" .
        (defined($iIndex) ? "-${iIndex}" : '') : '');
}

1;
