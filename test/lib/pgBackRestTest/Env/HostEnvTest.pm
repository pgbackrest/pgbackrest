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
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
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

use constant ENCRYPTION_KEY_ARCHIVE                              => 'archive';
    push @EXPORT, qw(ENCRYPTION_KEY_ARCHIVE);
use constant ENCRYPTION_KEY_MANIFEST                             => 'manifest';
    push @EXPORT, qw(ENCRYPTION_KEY_MANIFEST);
use constant ENCRYPTION_KEY_BACKUPSET                            => 'backupset';
    push @EXPORT, qw(ENCRYPTION_KEY_BACKUPSET);

####################################################################################################################################
# setup
####################################################################################################################################
sub setup
{
    my $self = shift;
    my $bSynthetic = shift;
    my $oLogTest = shift;
    my $oConfigParam = shift;

    # Start S3 server first since it takes the longest
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oHostS3;

    if ($oConfigParam->{bS3})
    {
        $oHostS3 = new pgBackRestTest::Env::Host::HostS3Test();
    }

    # Get host group
    my $oHostGroup = hostGroupGet();

    # Create the backup host
    my $strBackupDestination;
    my $bHostBackup = defined($$oConfigParam{bHostBackup}) ? $$oConfigParam{bHostBackup} : false;
    my $oHostBackup = undef;

    my $bRepoEncrypt = defined($$oConfigParam{bRepoEncrypt}) ? $$oConfigParam{bRepoEncrypt} : false;

    if ($bHostBackup)
    {
        $strBackupDestination = defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_BACKUP;

        $oHostBackup = new pgBackRestTest::Env::Host::HostBackupTest(
            {strBackupDestination => $strBackupDestination, bSynthetic => $bSynthetic, oLogTest => $oLogTest,
                bRepoLocal => !$oConfigParam->{bS3}, bRepoEncrypt => $bRepoEncrypt});
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
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest, bRepoLocal => !$oConfigParam->{bS3},
                bRepoEncrypt => $bRepoEncrypt});
    }
    else
    {
        $oHostDbMaster = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest, bRepoLocal => !$oConfigParam->{bS3},
                bRepoEncrypt => $bRepoEncrypt});
    }

    $oHostGroup->hostAdd($oHostDbMaster);

    # Create the db-standby host
    my $oHostDbStandby = undef;

    if (defined($$oConfigParam{bStandby}) && $$oConfigParam{bStandby})
    {
        $oHostDbStandby = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, bStandby => true, oLogTest => $oLogTest,
                bRepoLocal => !$oConfigParam->{bS3}});

        $oHostGroup->hostAdd($oHostDbStandby);
    }

    # Finalize S3 server
    #-------------------------------------------------------------------------------------------------------------------------------
    if (defined($oHostS3))
    {
        $oHostGroup->hostAdd($oHostS3, {rstryHostName => ['pgbackrest-dev.s3.amazonaws.com', 's3.amazonaws.com']});

        # Wait for server to start
        executeTest('docker logs -f ' . $oHostS3->container() . " | grep -m 1 \"server started\"");

        $oHostS3->executeS3('mb s3://' . HOST_S3_BUCKET);
    }

    # Create db master config
    $oHostDbMaster->configCreate({
        strBackupSource => $$oConfigParam{strBackupSource},
        bCompress => $$oConfigParam{bCompress},
        bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
        bArchiveAsync => $$oConfigParam{bArchiveAsync},
        bS3 => $$oConfigParam{bS3}});

    # Create backup config if backup host exists
    if (defined($oHostBackup))
    {
        $oHostBackup->configCreate({
            bCompress => $$oConfigParam{bCompress},
            bHardlink => $$oConfigParam{bHardLink},
            bS3 => $$oConfigParam{bS3}});
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
    $self->optionTestSet(CFGOPT_DB_PATH, $oHostDbMaster->dbBasePath());
    $self->optionTestSet(CFGOPT_REPO_PATH, $oHostBackup->repoPath());
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());

    # Configure the repo to be encrypted if required
    if ($bRepoEncrypt)
    {
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
    }

    # Set S3 options
    if (defined($oHostS3))
    {
        $self->optionTestSet(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY, HOST_S3_ACCESS_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY_SECRET, HOST_S3_ACCESS_SECRET_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_BUCKET, HOST_S3_BUCKET);
        $self->optionTestSet(CFGOPT_REPO_S3_ENDPOINT, HOST_S3_ENDPOINT);
        $self->optionTestSet(CFGOPT_REPO_S3_REGION, HOST_S3_REGION);
        $self->optionTestSet(CFGOPT_REPO_S3_HOST, $oHostS3->ipGet());
        $self->optionTestSetBool(CFGOPT_REPO_S3_VERIFY_SSL, false);
    }

    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    return $oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3;
}

####################################################################################################################################
# archiveGenerate
#
# Generate an WAL segment for testing.
####################################################################################################################################
sub archiveGenerate
{
    my $self = shift;
    my $strWalPath = shift;
    my $iSourceNo = shift;
    my $iArchiveNo = shift;
    my $strWalVersion = shift;
    my $bPartial = shift;

    my $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo)) .
                         (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = $self->dataPath() . "/backup.wal${iSourceNo}_${strWalVersion}.bin";

    my $strSourceFile = "${strWalPath}/${strArchiveFile}";

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
