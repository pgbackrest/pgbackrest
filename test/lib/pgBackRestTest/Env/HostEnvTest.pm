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

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostDbSyntheticTest;
use pgBackRestTest::Common::HostGroupTest;

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
# initModule
####################################################################################################################################
sub initModule
{
    # Set file and path modes
    pathModeDefaultSet('0700');
    fileModeDefaultSet('0600');
}

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

    # Create the local file object
    my $oFile =
        new pgBackRest::File
        (
            $oHostDbMaster->stanza(),
            $oHostDbMaster->repoPath(),
            new pgBackRest::Protocol::Common
            (
                OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
                OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
                OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
                HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
            )
        );

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

    return $oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile;
}

####################################################################################################################################
# archiveGenerate
#
# Generate an WAL segment for testing.
####################################################################################################################################
sub archiveGenerate
{
    my $self = shift;
    my $oFile = shift;
    my $strXlogPath = shift;
    my $iSourceNo = shift;
    my $iArchiveNo = shift;
    my $strWalVersion = shift;
    my $bPartial = shift;

    my $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo)) .
                         (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = $self->dataPath() . "/backup.wal${iSourceNo}_${strWalVersion}.bin";

    my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                 PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                 false,                                 # Source is not compressed
                 false);                                # Destination is not compressed

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
    my $oFile = shift;
    my $strWalPath = shift;
    my $strPgVersion = shift;
    my $iSourceNo = shift;
    my $strWalSegment = shift;
    my $bPartial = shift;

    my $strWalFile = "${strWalPath}/${strWalSegment}" . (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = $self->dataPath() . "/backup.wal${iSourceNo}_${strPgVersion}.bin";

    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                 PATH_DB_ABSOLUTE, $strWalFile,         # Destination file
                 false,                                 # Source is not compressed
                 false);                                # Destination is not compressed

    fileStringWrite("${strWalPath}/archive_status/${strWalSegment}.ready");

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

    fileRemove("$self->{strWalPath}/${strWalFile}");
    fileRemove("$self->{strWalPath}/archive_status/${strWalFile}.ready");
}

1;
