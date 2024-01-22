####################################################################################################################################
# FullCommonTest.pm - Common code for backup tests
####################################################################################################################################
package pgBackRestTest::Env::HostEnvTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRestDoc::Common::Log;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageBase;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::Host::HostAzureTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostDbSyntheticTest;
use pgBackRestTest::Env::Host::HostGcsTest;
use pgBackRestTest::Env::Host::HostS3Test;

####################################################################################################################################
# Constants
####################################################################################################################################
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
    my $oConfigParam = shift;

    # Start object server first since it takes the longest
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oHostObject;

    if ($oConfigParam->{strStorage} eq S3)
    {
        $oHostObject = new pgBackRestTest::Env::Host::HostS3Test();
    }
    elsif ($oConfigParam->{strStorage} eq AZURE)
    {
        $oHostObject = new pgBackRestTest::Env::Host::HostAzureTest();
    }
    elsif ($oConfigParam->{strStorage} eq GCS)
    {
        $oHostObject = new pgBackRestTest::Env::Host::HostGcsTest();
    }
    elsif ($oConfigParam->{strStorage} eq SFTP)
    {
        $oHostObject = new pgBackRestTest::Env::Host::HostSftpTest();
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
            {strBackupDestination => $strBackupDestination, bSynthetic => $bSynthetic,
                bRepoLocal => $oConfigParam->{strStorage} eq POSIX, bRepoEncrypt => $bRepoEncrypt, bTls => $oConfigParam->{bTls}});
        $oHostGroup->hostAdd($oHostBackup);
    }
    else
    {
        $strBackupDestination =
            defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_DB_PRIMARY;
    }

    # Create the db-primary host
    my $oHostDbPrimary = undef;

    if ($bSynthetic)
    {
        $oHostDbPrimary = new pgBackRestTest::Env::Host::HostDbSyntheticTest(
            {strBackupDestination => $strBackupDestination,
                bRepoLocal => $oConfigParam->{strStorage} eq POSIX, bRepoEncrypt => $bRepoEncrypt, bTls => $oConfigParam->{bTls}});
    }
    else
    {
        $oHostDbPrimary = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, bRepoLocal => $oConfigParam->{strStorage} eq POSIX,
                bRepoEncrypt => $bRepoEncrypt, bTls => $oConfigParam->{bTls}});
    }

    $oHostGroup->hostAdd($oHostDbPrimary);

    # Create the db-standby host
    my $oHostDbStandby = undef;

    if (defined($$oConfigParam{bStandby}) && $$oConfigParam{bStandby})
    {
        $oHostDbStandby = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, bStandby => true, bRepoLocal => $oConfigParam->{strStorage} eq POSIX,
                bTls => $oConfigParam->{bTls}});

        $oHostGroup->hostAdd($oHostDbStandby);
    }

    # Finalize object server
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($oConfigParam->{strStorage} eq S3)
    {
        $oHostGroup->hostAdd($oHostObject, {rstryHostName => ['pgbackrest-dev.s3.amazonaws.com', 's3.amazonaws.com']});
    }
    elsif ($oConfigParam->{strStorage} eq AZURE || $oConfigParam->{strStorage} eq GCS || $oConfigParam->{strStorage} eq SFTP)
    {
        $oHostGroup->hostAdd($oHostObject);
    }

    # Create db-primary config
    $oHostDbPrimary->configCreate({
        bTls => $oConfigParam->{bTls},
        strBackupSource => $$oConfigParam{strBackupSource},
        strCompressType => $$oConfigParam{strCompressType},
        bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
        bArchiveAsync => $$oConfigParam{bArchiveAsync},
        strStorage => $oConfigParam->{strStorage},
        iRepoTotal => $oConfigParam->{iRepoTotal},
        bBundle => $oConfigParam->{bBundle},
        bBlockIncr => $oConfigParam->{bBlockIncr}});

    # Create backup config if backup host exists
    if (defined($oHostBackup))
    {
        $oHostBackup->configCreate({
            bTls => $oConfigParam->{bTls},
            strCompressType => $$oConfigParam{strCompressType},
            bHardlink => $$oConfigParam{bHardLink},
            strStorage => $oConfigParam->{strStorage},
            iRepoTotal => $oConfigParam->{iRepoTotal},
            bBundle => $oConfigParam->{bBundle},
            bBlockIncr => $oConfigParam->{bBlockIncr}});
    }
    # If backup host is not defined set it to db-primary
    else
    {
        $oHostBackup = $strBackupDestination eq HOST_DB_PRIMARY || $strBackupDestination eq HOST_SFTP ? $oHostDbPrimary :
            $oHostDbStandby;
    }

    storageRepoCommandSet(
        $self->backrestExeHelper() .
            ' --config=' . $oHostBackup->backrestConfig() . ' --stanza=' . $self->stanza() . ' --log-level-console=off' .
            ' --log-level-stderr=error' .
            ($oConfigParam->{strStorage} ne POSIX ?
                ($oConfigParam->{strStorage} ne SFTP ? " --no-repo1-storage-verify-tls" : '') .
                " --repo1-$oConfigParam->{strStorage}-" .
                ($oConfigParam->{strStorage} eq GCS ? 'endpoint' : 'host') . "=" . $oHostObject->ipGet() : '') .
                ($oConfigParam->{strStorage} eq GCS ? ':' . HOST_GCS_PORT : ''),
        $oConfigParam->{strStorage} eq POSIX ? STORAGE_POSIX : STORAGE_OBJECT);

    # Create db-standby config
    if (defined($oHostDbStandby))
    {
        $oHostDbStandby->configCreate({
            bTls => $oConfigParam->{bTls},
            strBackupSource => $$oConfigParam{strBackupSource},
            strCompressType => $$oConfigParam{strCompressType},
            bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
            bArchiveAsync => $$oConfigParam{bArchiveAsync},
            strStorage => $oConfigParam->{strStorage},
            iRepoTotal => $oConfigParam->{iRepoTotal},
            bBundle => $oConfigParam->{bBundle},
            bBlockIncr => $oConfigParam->{bBlockIncr}});
    }

    # Create object storage
    if (defined($oHostObject))
    {
        storageRepo()->create();
    }

    return $oHostDbPrimary, $oHostDbStandby, $oHostBackup;
}

####################################################################################################################################
# Generate database system id for the db version
####################################################################################################################################
sub dbSysId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dbSysId', \@_,
            {name => 'strPgVersion', trace => true},
        );

    return (1000000000000000000 + ($strPgVersion * 10));
}

####################################################################################################################################
# Get database catalog version for the db version
####################################################################################################################################
sub dbCatalogVersion
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sysId', \@_,
            {name => 'strPgVersion', trace => true},
        );

    my $hCatalogVersion =
    {
        &PG_VERSION_94 => 201409291,
        &PG_VERSION_95 => 201510051,
        &PG_VERSION_96 => 201608131,
        &PG_VERSION_10 => 201707211,
        &PG_VERSION_11 => 201806231,
        &PG_VERSION_12 => 201909212,
        &PG_VERSION_13 => 202007201,
        &PG_VERSION_14 => 202105121,
        &PG_VERSION_15 => 202209061,
        &PG_VERSION_16 => 202307071,
    };

    if (!defined($hCatalogVersion->{$strPgVersion}))
    {
        confess &log(ASSERT, "no catalog version defined for pg version ${strPgVersion}");
    }

    return $hCatalogVersion->{$strPgVersion};
}

####################################################################################################################################
# Generate control file content
####################################################################################################################################
sub controlGenerateContent
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->controlGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
        );

    my $hControlContent =
    {
        32 =>
        {
            &PG_VERSION_94 =>
                "5e0064a7b3b6e00dae0300000b43010c00000000000000000000000001000000000000000000000000000000000000000000000001000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000002000000000000000000000000000010000000000000000000000000000000000000000" .
                "000000006b0756c8",
            &PG_VERSION_95 =>
                "5f0064a7b3b6e00dae030000a3cc020c00000000000000000000000001000000000000000000000000000000000000000000000001000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000010000000000000000" .
                "000000000000000000000000000000003bfe413a",
            &PG_VERSION_96 =>
                "600064a7b3b6e00dc0030000c34b040c00000000000000000000000001000000000000000000000000000000000000000000000001000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000010000000000000000" .
                "000000000000000000000000000000005d135da6",
            &PG_VERSION_10 =>
                "640064a7b3b6e00dea030000cbce050c00000000000000000000000001000000000000000000000000000000000000000000000001000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000010000000000000000" .
                "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000f8556c34",
        },
        64 =>
        {
            &PG_VERSION_94 =>
                "5e0064a7b3b6e00dae0300000b43010c00000000000000000000000000000000010000000000000000000000000000000000000000000000" .
                "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000010000000000000000" .
                "00000000000000000000000000000000ee6cf996",
            &PG_VERSION_95 =>
                "5f0064a7b3b6e00dae030000a3cc020c00000000000000000000000000000000010000000000000000000000000000000000000000000000" .
                "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000020000000000000" .
                "0000000000000001000000000000000000000000000000000000000000000000381ec2de",
            &PG_VERSION_96 =>
                "600064a7b3b6e00dc0030000c34b040c00000000000000000000000000000000010000000000000000000000000000000000000000000000" .
                "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000020000000000000" .
                "00000000000000010000000000000000000000000000000000000000000000002d96a4c0",
            &PG_VERSION_10 =>
                "640064a7b3b6e00dea030000cbce050c00000000000000000000000000000000010000000000000000000000000000000000000000000000" .
                "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000020000000000000" .
                "0000000000000001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" .
                "00000000000000008d543cdf",
        },
    };

    my $strControlContent = $hControlContent->{$self->archBits()}{$strPgVersion};

    if (!defined($strControlContent))
    {
        confess &log(ASSERT, "no control content defined for pg version ${strPgVersion}");
    }

    my $tControlContent = '';

    for (my $iIdx = 0; $iIdx < length($strControlContent) / 2; $iIdx++)
    {
        my $iChar = hex(substr($strControlContent, $iIdx * 2, 2));
        $tControlContent .= pack('C', $iChar);
    }


    # Pad bytes
    for (my $iIdx = length($tControlContent); $iIdx < 8192; $iIdx++)
    {
        $tControlContent .= pack('C', 0);
    }

    return \$tControlContent;
}

####################################################################################################################################
# Generate control file and write to disk
####################################################################################################################################
sub controlGenerate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbPath,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->controlGenerate', \@_,
            {name => 'strDbPath', trace => true},
            {name => 'strPgVersion', trace => true},
        );

    storageTest()->put("${strDbPath}/global/pg_control", $self->controlGenerateContent($strPgVersion));
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
# Generate WAL file content
####################################################################################################################################
sub walGenerateContent
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
        $iSourceNo,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->walGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
            {name => 'iSourceNo', optional => true, default => 1, trace => true},
        );

    # Get WAL magic for the PG version
    my $hWalMagic =
    {
        &PG_VERSION_94 => hex('0xD07E'),
        &PG_VERSION_95 => hex('0xD087'),
        &PG_VERSION_96 => hex('0xD093'),
        &PG_VERSION_10 => hex('0xD097'),
        &PG_VERSION_11 => hex('0xD098'),
        &PG_VERSION_12 => hex('0xD101'),
        &PG_VERSION_13 => hex('0xD106'),
    };

    my $tWalContent = pack('S', $hWalMagic->{$strPgVersion});

    # Indicate that the long header is present
    $tWalContent .= pack('S', 2);

    # Add junk (H for header) for the bytes that won't be read by the tests
    my $iOffset = 12 + (testRunGet()->archBits() / 8);
    $tWalContent .= ('H' x $iOffset);

    # Add the system identifier
    $tWalContent .= pack('Q', $self->dbSysId($strPgVersion));

    # Add segment size
    $tWalContent .= pack('L', PG_WAL_SEGMENT_SIZE);

    # Add the source number to produce WAL segments with different checksums
    $tWalContent .= pack('S', $iSourceNo);

    # Pad out to the required size (B for body)
    $tWalContent .= ('B' x (PG_WAL_SEGMENT_SIZE - length($tWalContent)));

    return \$tWalContent;
}

####################################################################################################################################
# Generate WAL file content checksum
####################################################################################################################################
sub walGenerateContentChecksum
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
        $hParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->walGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
            {name => 'hParam', required => false, trace => true},
        );

    return sha1_hex(${$self->walGenerateContent($strPgVersion, $hParam)});
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
    my $bChecksum = shift;
    my $bReady = shift;

    my $rtWalContent = $self->walGenerateContent($strPgVersion, {iSourceNo => $iSourceNo});
    my $strWalFile =
        "${strWalPath}/${strWalSegment}" . ($bChecksum ? '-' . sha1_hex($rtWalContent) : '') .
            (defined($bPartial) && $bPartial ? '.partial' : '');

    # Put the WAL segment and the ready file
    storageTest()->put($strWalFile, $rtWalContent);

    if (!defined($bReady) || $bReady)
    {
        storageTest()->put("${strWalPath}/archive_status/${strWalSegment}.ready");
    }

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
