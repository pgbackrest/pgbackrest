####################################################################################################################################
# Tests for Backup File module
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupFileUnitPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::File;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strPgControl} = $self->{strDbPath} . '/' . DB_FILE_PGCONTROL;

    # Create backup path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Generate pg_control file
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
    $self->controlGenerate($self->{strDbPath}, PG_VERSION_94);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_PG_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());

    $self->optionTestSetBool(CFGOPT_ONLINE, false);

    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);
    $self->optionTestSet(CFGOPT_COMPRESS_LEVEL, 3);

    $self->configTestLoad(CFGCMD_BACKUP);

    # Repo
    my $strRepoBackupPath = storageRepo()->pathGet(STORAGE_REPO_BACKUP);
    my $strBackupLabel = "20180724-182750F";

    # File
    my $strFileName = "12345";
    my $strFileDb = $self->{strDbPath} . "/$strFileName";
    my $strFileHash = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $strFileRepo = storageRepo()->pathGet(
        STORAGE_REPO_BACKUP . "/$strBackupLabel/" . MANIFEST_TARGET_PGDATA . "/$strFileName");
    my $strRepoFile = MANIFEST_TARGET_PGDATA . "/$strFileName";
    my $strRepoPgControl = MANIFEST_FILE_PGCONTROL;
    my $strPgControlRepo = storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/$strBackupLabel/$strRepoPgControl");
    my $strPgControlHash =
        $self->archBits() == 32 ? '8107e546c59c72a8c1818fc3610d7cc1e5623660' : '4c77c900f7af0d9ab13fa9982051a42e0b637f6c';

    # Copy file to db path
    executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strFileDb}");

    # Get size and data info for the files in the db path
    my $hManifest = storageDb()->manifest($self->{strDbPath});
    my $lFileSize = $hManifest->{$strFileName}{size} + 0;
    my $lFileTime = $hManifest->{$strFileName}{modification_time} + 0;
    my $lPgControlSize = $hManifest->{&DB_FILE_PGCONTROL}{size} + 0;
    my $lPgControlTime = $hManifest->{&DB_FILE_PGCONTROL}{modification_time} + 0;

    my $lRepoFileCompressSize = 3646899;

    my $strBackupPath = $self->{strBackupPath} . "/$strBackupLabel";
    my $strHost = "host";
    my $iLocalId = 1;

    # Initialize the manifest
    my $oBackupManifest = new pgBackRest::Manifest("$strBackupPath/" . FILE_MANIFEST,
        {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => 201409291});
    $oBackupManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);

    # Set the initial size values for backupManifestUpdate - running size and size for when to save the file
    my $lSizeCurrent = 0;
    my $lSizeTotal = 16785408;
    my $lManifestSaveCurrent = 0;
    my $lManifestSaveSize = int($lSizeTotal / 100);

    # Result variables
    my $iResultCopyResult;
    my $lResultCopySize;
    my $lResultRepoSize;
    my $strResultCopyChecksum;
    my $rResultExtra;

    ################################################################################################################################
    if ($self->begin('backupFile(), backupManifestUpdate()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        # Create backup path so manifest can be saved
        storageRepo->pathCreate(storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/$strBackupLabel"));

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $self->{strPgControl},
            $strRepoPgControl,
            $lPgControlSize,
            undef,
            false,
            BACKUP_FILE_COPY,
            8192,
            8192,
            $strPgControlHash,
            undef,
            16785408,
            0,
            167854,
            0);

        # Accumulators should be same size as pg_control
        $self->testResult(($lSizeCurrent == $lPgControlSize && $lManifestSaveCurrent == $lPgControlSize), true,
            "file size in repo and repo size equal pg_control size");

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL,
            MANIFEST_SUBKEY_CHECKSUM, $strPgControlHash)}, true, "manifest updated for pg_control");

        # Neither backup.manifest nor backup.manifest.copy written because size threshold not met
        $self->testResult(sub {storageRepo()->exists("$strBackupPath/" . FILE_MANIFEST)}, false, "backup.manifest missing");
        $self->testResult(
            sub {storageRepo()->exists("$strBackupPath/" . FILE_MANIFEST . INI_COPY_EXT)}, false, "backup.manifest.copy missing");

        #---------------------------------------------------------------------------------------------------------------------------
        # No prior checksum, no compression, no page checksum, no extra, no delta, no hasReference
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            false,
            BACKUP_FILE_COPY,
            16777216,
            16777216,
            '1c7e00fd09b9dd11fc2966590b3e3274645dd031',
            undef,
            16785408,
            8192,
            167854,
            8192);

        # Accumulator includes size of pg_control and file. Manifest saved so ManifestSaveCurrent returns to 0
        $self->testResult(($lSizeCurrent == ($lPgControlSize + $lFileSize) && $lManifestSaveCurrent == 0), true,
            "repo size increased and ManifestSaveCurrent returns to 0");

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile,
            MANIFEST_SUBKEY_CHECKSUM, $strFileHash)}, true, "manifest updated for $strRepoFile");

        # Backup.manifest not written but backup.manifest.copy written because size threshold  met
        $self->testResult(sub {storageTest()->exists("$strBackupPath/" . FILE_MANIFEST . INI_COPY_EXT)}, true,
            'backup.manifest.copy exists in repo');
        $self->testResult(
            sub {storageRepo()->exists("$strBackupPath/" . FILE_MANIFEST)}, false, 'backup.manifest.copy missing in repo');

        #---------------------------------------------------------------------------------------------------------------------------
        #  Set up page checksum result
        $rResultExtra = {'valid' => true,'align' => true};

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            BACKUP_FILE_COPY,
            16777216,
            3646899,
            '1c7e00fd09b9dd11fc2966590b3e3274645dd031',
            $rResultExtra,
            16785408,
            16785408,
            167854,
            0);

        # File is compressed in repo so make sure repo-size added to manifest
        $self->testResult(sub {$oBackupManifest->test(
            MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REPO_SIZE, $lResultRepoSize)},
            true, "repo-size set");
        $self->testResult(sub {$oBackupManifest->test(
            MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, $rResultExtra->{bValid})},
            true, "checksum page set");

        # Set a section in  the manifest to ensure it is removed in the next test
        $oBackupManifest->set(
            MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1", MANIFEST_SUBKEY_CHECKSUM, $strResultCopyChecksum);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/$strFileName.1")},
            true, MANIFEST_TARGET_PGDATA . "/$strFileName.1 section exists in manifest - skip file");

        #---------------------------------------------------------------------------------------------------------------------------
        # Removed db file is removed from manifest
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            "$strFileDb.1",
            "$strRepoFile.1",
            $lFileSize,
            $strFileHash,
            false,
            BACKUP_FILE_SKIP,
            undef,
            undef,
            undef,
            undef,
            16785408,
            33562624,
            167854,
            0);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1")},
            false, "    $strRepoFile.1 section removed from manifest");

        # Add back the section
        $oBackupManifest->set(MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1");

        # Code coverage for code path when host not defined for logged message of skipped file
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            undef,
            $iLocalId,
            "$strFileDb.1",
            MANIFEST_TARGET_PGDATA . "/$strFileName.1",
            $lFileSize,
            $strFileHash,
            false,
            BACKUP_FILE_SKIP,
            undef,
            undef,
            undef,
            undef,
            16785408,
            50339840,
            167854,
            0);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1")},
            false, "    $strRepoFile.1 section removed from manifest on undef host");

        #---------------------------------------------------------------------------------------------------------------------------
        # Has reference - Code path to ensure reference is removed
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            false,
            BACKUP_FILE_COPY,
            16777216,
            16777216,
            '1c7e00fd09b9dd11fc2966590b3e3274645dd031',
            undef,
            16785408,
            67117056,
            167854,
            0);

        # Confirm reference to prior backup removed
        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/$strFileName.",
            MANIFEST_SUBKEY_REFERENCE)},
            false, "reference to prior backup in manifest removed");

        #---------------------------------------------------------------------------------------------------------------------------
        # BACKUP_FILE_NOOP

        # Calculate running counts
        my $lSizeCurrentAfter = $lSizeCurrent + $lFileSize;
        my $lManifestSaveCurrentAfter = $lManifestSaveCurrent + $lFileSize;

        # Increase manifest save size, so manifest will not be saved so counts can be tested
        $lManifestSaveSize = $lFileSize * 2;

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            false,
            BACKUP_FILE_NOOP,
            16777216,
            undef,
            '1c7e00fd09b9dd11fc2966590b3e3274645dd031',
            undef,
            16785408,
            83894272,
            $lManifestSaveSize,
            0);

        $self->testResult(($lSizeCurrent ==$lSizeCurrentAfter && $lManifestSaveCurrent == $lManifestSaveCurrentAfter),
            true, '    running counts updated');
    }

    ################################################################################################################################
    # This section for for code coverage that is not covered in the above tests
    if ($self->begin('backupManifestUpdate()'))
    {
        $oBackupManifest = new pgBackRest::Manifest("$strBackupPath/" . FILE_MANIFEST,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => 201409291});

        #---------------------------------------------------------------------------------------------------------------------------
        # Check BACKUP_FILE_RECOPY warning
        $iResultCopyResult = BACKUP_FILE_RECOPY;
        $lResultCopySize = 0;
        $lResultRepoSize = $lResultCopySize + 1;
        $strResultCopyChecksum = $strFileHash;
        $lSizeCurrent = 0;
        $lManifestSaveSize = $lFileSize * 2;
        $lManifestSaveCurrent = 0;
        $rResultExtra = undef;

        $self->testResult(sub {backupManifestUpdate(
            $oBackupManifest,
            undef,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            false,
            $iResultCopyResult,
            $lResultCopySize,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)}, "($lFileSize, $lFileSize)",
            'backup file recopy warning', {strLogExpect =>
            "WARN: resumed backup file $strRepoFile does not have expected checksum $strFileHash. The file will be recopied and" .
            " backup will continue but this may be an issue unless the resumed backup path in the repository is known to be" .
            " corrupted.\n" .
            "NOTE: this does not indicate a problem with the PostgreSQL page checksums."});

        # Check size code paths
        $self->testResult(
            $oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE, $lResultCopySize),
            true, "    copy size set");
        $self->testResult(
            $oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REPO_SIZE, $lResultRepoSize),
            true, "    repo size set");
        $self->testResult(
            $oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM, $strResultCopyChecksum),
            false, "    checksum not set since copy size 0");

        #---------------------------------------------------------------------------------------------------------------------------
        # Checksum page exception
        $iResultCopyResult = BACKUP_FILE_COPY;

        $self->testException(sub {backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            $iResultCopyResult,
            $lResultCopySize,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)},
            ERROR_ASSERT, "$strFileDb should have calculated page checksums");

        $rResultExtra->{valid} = false;
        $self->testException(sub {backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            $iResultCopyResult,
            $lResultCopySize + 1,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)},
            ERROR_ASSERT, "align flag should have been set for misaligned page");

        $rResultExtra->{align} = true;
        $self->testException(sub {backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            $iResultCopyResult,
            $lResultCopySize + 1,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)},
            ERROR_ASSERT, "align flag should have been set for misaligned page");

        $rResultExtra->{align} = false;
        $self->testResult(sub {backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            $iResultCopyResult,
            $lResultCopySize + 1,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)},
            "($lFileSize, $lFileSize)",
            'page misalignment warning - host defined', {strLogExpect =>
            "WARN: page misalignment in file $strHost:$strFileDb: file size " . ($lResultCopySize + 1) .
            " is not divisible by page size " . PG_PAGE_SIZE});

        $self->testResult(sub {backupManifestUpdate(
            $oBackupManifest,
            undef,
            $iLocalId,
            $strFileDb,
            $strRepoFile,
            $lFileSize,
            $strFileHash,
            true,
            $iResultCopyResult,
            $lResultCopySize + 1,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent)},
            "($lFileSize, $lFileSize)",
            'page misalignment warning - host not defined', {strLogExpect =>
            "WARN: page misalignment in file $strFileDb: file size " . ($lResultCopySize + 1) .
            " is not divisible by page size " . PG_PAGE_SIZE});
    }
}

1;
