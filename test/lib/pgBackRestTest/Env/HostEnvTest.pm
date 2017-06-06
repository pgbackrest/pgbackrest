####################################################################################################################################
# FullCommonTest.pm - Common code for backup tests
####################################################################################################################################
package pgBackRestTest::Env::HostEnvTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostDbSyntheticTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant WAL_VERSION_92                                      => '92';
    push @EXPORT, qw(WAL_VERSION_92);
use constant WAL_VERSION_92_SYS_ID                               => 6393320793115174899;
    push @EXPORT, qw(WAL_VERSION_92_SYS_ID);
use constant WAL_VERSION_93                                      => '93';
    push @EXPORT, qw(WAL_VERSION_93);
use constant WAL_VERSION_93_SYS_ID                               => 6395542721432104958;
    push @EXPORT, qw(WAL_VERSION_93_SYS_ID);
use constant WAL_VERSION_94                                      => '94';
    push @EXPORT, qw(WAL_VERSION_94);
use constant WAL_VERSION_94_SYS_ID                               => 6353949018581704918;
    push @EXPORT, qw(WAL_VERSION_94_SYS_ID);
use constant WAL_VERSION_95                                      => '95';
    push @EXPORT, qw(WAL_VERSION_95);
use constant WAL_VERSION_95_SYS_ID                               => 6392579261579036436;
    push @EXPORT, qw(WAL_VERSION_95_SYS_ID);

####################################################################################################################################
# setup
####################################################################################################################################
sub setup
{
    my $self = shift;
    my $bSynthetic = shift;
    my $oLogTest = shift;
    my $oConfigParam = shift;

    # Get host group
    my $oHostGroup = hostGroupGet();

    # Create the backup host
    my $strBackupDestination;
    my $bHostBackup = defined($$oConfigParam{bHostBackup}) ? $$oConfigParam{bHostBackup} : false;
    my $oHostBackup = undef;

    if ($bHostBackup)
    {
        $strBackupDestination = defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_BACKUP;

        $oHostBackup = new pgBackRestTest::Env::Host::HostBackupTest(
            {strBackupDestination => $strBackupDestination, bSynthetic => $bSynthetic, oLogTest => $oLogTest});
        $oHostGroup->hostAdd($oHostBackup);
    }
    else
    {
        $strBackupDestination =
            defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_DB_MASTER;
    }

    # Create the db-master host
    my $oHostDbMaster = undef;

    if ($bSynthetic)
    {
        $oHostDbMaster = new pgBackRestTest::Env::Host::HostDbSyntheticTest(
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest});
    }
    else
    {
        $oHostDbMaster = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest});
    }

    $oHostGroup->hostAdd($oHostDbMaster);

    # Create the db-standby host
    my $oHostDbStandby = undef;

    if (defined($$oConfigParam{bStandby}) && $$oConfigParam{bStandby})
    {
        $oHostDbStandby = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, bStandby => true, oLogTest => $oLogTest});

        $oHostGroup->hostAdd($oHostDbStandby);
    }

    # Create db master config
    $oHostDbMaster->configCreate({
        strBackupSource => $$oConfigParam{strBackupSource},
        bCompress => $$oConfigParam{bCompress},
        bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
        bArchiveAsync => $$oConfigParam{bArchiveAsync}});

    # Create backup config if backup host exists
    if (defined($oHostBackup))
    {
        $oHostBackup->configCreate({
            bCompress => $$oConfigParam{bCompress},
            bHardlink => $$oConfigParam{bHardLink}});
    }
    # If backup host is not defined set it to db-master
    else
    {
        $oHostBackup = $strBackupDestination eq HOST_DB_MASTER ? $oHostDbMaster : $oHostDbStandby;
    }

    # Create db-standby config
    if (defined($oHostDbStandby))
    {
        $oHostDbStandby->configCreate({
            strBackupSource => $$oConfigParam{strBackupSource},
            bCompress => $$oConfigParam{bCompress},
            bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
            bArchiveAsync => $$oConfigParam{bArchiveAsync}});
    }

    # Set options needed for storage helper
    my $oOption = {};
    $self->optionSetTest($oOption, OPTION_DB_PATH, $oHostDbMaster->dbBasePath());
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $oHostBackup->repoPath());
    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->testResult(sub {$self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH)}, '', 'config load');

    return $oHostDbMaster, $oHostDbStandby, $oHostBackup;
}

####################################################################################################################################
# archiveGenerate
#
# Generate an WAL segment for testing.
####################################################################################################################################
sub archiveGenerate
{
    my $self = shift;
    my $strXlogPath = shift;
    my $iSourceNo = shift;
    my $iArchiveNo = shift;
    my $strWalVersion = shift;
    my $bPartial = shift;

    my $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo)) .
                         (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = $self->dataPath() . "/backup.wal${iSourceNo}_${strWalVersion}.bin";

    my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

    storageTest()->copy($strArchiveTestFile, $strSourceFile);

    return $strArchiveFile, $strSourceFile;
}

####################################################################################################################################
# walSegment
#
# Generate name of WAL segment from component parts.
####################################################################################################################################
sub walSegment
{
    my $self = shift;
    my $iTimeline = shift;
    my $iMajor = shift;
    my $iMinor = shift;

    return uc(sprintf('%08x%08x%08x', $iTimeline, $iMajor, $iMinor));
}

####################################################################################################################################
# walGenerate
#
# Generate a WAL segment and ready file for testing.
####################################################################################################################################
sub walGenerate
{
    my $self = shift;
    my $strWalPath = shift;
    my $strPgVersion = shift;
    my $iSourceNo = shift;
    my $strWalSegment = shift;
    my $bPartial = shift;

    my $strWalFile = "${strWalPath}/${strWalSegment}" . (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = $self->dataPath() . "/backup.wal${iSourceNo}_${strPgVersion}.bin";

    storageTest()->copy($strArchiveTestFile, $strWalFile);

    storageTest()->put("${strWalPath}/archive_status/${strWalSegment}.ready");

    return $strWalFile;
}

####################################################################################################################################
# walRemove
#
# Remove WAL file and ready file.
####################################################################################################################################
sub walRemove
{
    my $self = shift;
    my $strWalPath = shift;
    my $strWalFile = shift;

    storageTest()->remove("$self->{strWalPath}/${strWalFile}");
    storageTest()->remove("$self->{strWalPath}/archive_status/${strWalFile}.ready");
}

1;
