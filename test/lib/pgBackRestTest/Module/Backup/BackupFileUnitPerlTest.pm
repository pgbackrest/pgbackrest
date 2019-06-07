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
        # Copy pg_control and confirm manifestUpdate does not save the manifest yet
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($self->{strPgControl}, MANIFEST_FILE_PGCONTROL, $lPgControlSize, undef, false, $strBackupLabel, false,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lPgControlTime, true, undef, false, false, undef);

        $self->testResult(sub {storageTest()->exists($strPgControlRepo)}, true, 'pg_control file exists in repo');

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strPgControlHash &&
            $lResultCopySize == $lPgControlSize && $lResultRepoSize == $lPgControlSize), true,
            'pg_control file copied to repo successfully');

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            $self->{strPgControl},
            $strRepoPgControl,
            $lPgControlSize,
            undef,
            false,
            $iResultCopyResult,
            $lResultCopySize,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent);

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
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, undef, false, $strBackupLabel, false,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef);

        $self->testResult(sub {storageTest()->exists($strFileRepo)}, true, 'non-compressed file exists in repo');

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize && $lResultRepoSize == $lFileSize), true,
            'file copied to repo successfully');

        $self->testResult(sub {storageRepo()->exists("${strFileRepo}.gz")}, false, "${strFileRepo}.gz missing");

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
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
            $lManifestSaveCurrent);

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

        storageTest()->remove($strFileRepo);
        storageTest()->remove($strPgControlRepo);

        #---------------------------------------------------------------------------------------------------------------------------
        # No prior checksum, yes compression, yes page checksum, no extra, no delta, no hasReference
        # $self->testException(sub {backupFile($strFileDb, $strRepoFile, $lFileSize, undef, true,
        #     $strBackupLabel, true, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef)}, ERROR_ASSERT,
        #     "iWalId is required in Backup::Filter::PageChecksum->new");

        # Build the lsn start parameter to pass to the extra function
        my $hStartLsnParam =
        {
            iWalId => 0xFFFF,
            iWalOffset => 0xFFFF,
        };

        #---------------------------------------------------------------------------------------------------------------------------
        # No prior checksum, yes compression, yes page checksum, yes extra, no delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, undef, true, $strBackupLabel, true,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, $hStartLsnParam, false, false, undef);

        $self->testResult(sub {storageTest()->exists("$strFileRepo.gz")}, true, 'compressed file exists in repo');

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultRepoSize == $lRepoFileCompressSize && $rResultExtra->{bValid}), true, 'file copied to repo successfully');

        # Only the compressed version of the file exists
        $self->testResult(sub {storageRepo()->exists("$strFileRepo")}, false, "only compressed version exists");

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
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
            $lManifestSaveCurrent);

        # File is compressed in repo so make sure repo-size added to manifest
        $self->testResult(sub {$oBackupManifest->test(
            MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REPO_SIZE, $lResultRepoSize)},
            true, "repo-size set");
        $self->testResult(sub {$oBackupManifest->test(
            MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, $rResultExtra->{bValid})},
            true, "checksum page set");

        # Save the compressed file for later test
        executeTest('mv ' . "$strFileRepo.gz $strFileRepo.gz.SAVE");

        #---------------------------------------------------------------------------------------------------------------------------
        # Add a segment number for bChecksumPage code coverage
        executeTest('cp ' . "$strFileDb $strFileDb.1");

        # No prior checksum, no compression, yes page checksum, yes extra, no delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile("$strFileDb.1", "$strRepoFile.1", $lFileSize, undef, true, $strBackupLabel, false,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, $hStartLsnParam, false, false, undef);

        $self->testResult(sub {storageTest()->exists("$strFileRepo.1")}, true, 'non-compressed segment file exists in repo');

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultRepoSize == $lFileSize && $rResultExtra->{bValid}), true, 'segment file copied to repo successfully');

        # Set a section in  the manifest to ensure it is removed in the next test
        $oBackupManifest->set(
            MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1", MANIFEST_SUBKEY_CHECKSUM, $strResultCopyChecksum);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/$strFileName.1")},
            true, MANIFEST_TARGET_PGDATA . "/$strFileName.1 section exists in manifest");

        #---------------------------------------------------------------------------------------------------------------------------
        # Remove the db file and try to back it up
        storageTest()->remove("$strFileDb.1");

        # No prior checksum, no compression, no page checksum, no extra, No delta, no hasReference, no db file
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile("$strFileDb.1", "$strRepoFile.1", $lFileSize, undef, false, $strBackupLabel,
            false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_SKIP && !defined($strResultCopyChecksum) &&
            !defined($lResultRepoSize) && !defined($lResultCopySize)), true, "db file missing - $strRepoFile.1 file skipped");

        # Delta not set so file still exists in repo
        $self->testResult(sub {storageTest()->exists("$strFileRepo.1")}, true, '    delta not set - file exists in repo');

        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
            $iLocalId,
            "$strFileDb.1",
            "$strRepoFile.1",
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
            $lManifestSaveCurrent);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, "$strRepoFile.1")},
            false, "    $strRepoFile.1 section removed from manifest");

        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, no hasReference, no db file
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile("$strFileDb.1", MANIFEST_TARGET_PGDATA . "/$strFileName.1", $lFileSize, $strFileHash, false, $strBackupLabel,
            false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_SKIP && !defined($strResultCopyChecksum) &&
            !defined($lResultRepoSize)), true, "db file missing - delta $strRepoFile.1 file skipped");

        $self->testResult(sub {storageTest()->exists("$strFileRepo.1")}, false, '   delta set - file removed from repo');

        # Code path for host not defined for logged message of skipped file
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            undef,
            $iLocalId,
            "$strFileDb.1",
            MANIFEST_TARGET_PGDATA . "/$strFileName.1",
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
            $lManifestSaveCurrent);

        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, no hasReference, no db file,
        # do not ignoreMissing
        $self->testException(sub {backupFile("$strFileDb.1", "$strRepoFile.1", $lFileSize, $strFileHash,
            false, $strBackupLabel, false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, false, undef, true, false, undef)},
            ERROR_FILE_MISSING, "unable to open '$strFileDb.1': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        # Restore the compressed file
        executeTest('mv ' . "$strFileRepo.gz.SAVE $strFileRepo.gz");

        # Yes prior checksum, yes compression, no page checksum, no extra, yes delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, $strFileHash, false, $strBackupLabel,
            true, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_CHECKSUM && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize), true, 'db checksum and repo same - no copy file');

        #---------------------------------------------------------------------------------------------------------------------------
        # DB Checksum mismatch
        storageTest()->remove("$strFileRepo", {bIgnoreMissing => true});
        # Save the compressed file for later test
        executeTest('mv ' . "$strFileRepo.gz $strFileRepo.gz.SAVE");

        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, $strFileHash . "ff", false,
            $strBackupLabel, false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize && $lResultRepoSize == $lFileSize), true, 'db checksum mismatch - copy file');

        #---------------------------------------------------------------------------------------------------------------------------
        # DB file size mismatch
        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize + 1, $strFileHash, false, $strBackupLabel, false,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize && $lResultRepoSize == $lFileSize), true, 'db file size mismatch - copy file');

        #---------------------------------------------------------------------------------------------------------------------------
        # Repo mismatch

        # Restore the compressed file as if non-compressed so checksum won't match
        executeTest('cp ' . "$strFileRepo.gz.SAVE $strFileRepo");

        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, $strFileHash, false, $strBackupLabel, false,
            cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_RECOPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize && $lResultRepoSize == $lFileSize), true, 'repo checksum mismatch - recopy file');

        # Restore the compressed file
        executeTest('mv ' . "$strFileRepo.gz.SAVE $strFileRepo.gz");

        # Yes prior checksum, yes compression, no page checksum, no extra, no delta, no hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize + 1, $strFileHash, false,
            $strBackupLabel, true, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_RECOPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize), true, 'repo size mismatch - recopy file');

        #---------------------------------------------------------------------------------------------------------------------------
        # Has reference
        # Set a reference in the manifest to ensure it is removed after backupManifestUpdate
        $oBackupManifest->set(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REFERENCE, BOGUS);

        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REFERENCE,
            BOGUS)}, true, "$strRepoFile reference section exists in manifest");

        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, yes hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize + 1, $strFileHash, false,
            $strBackupLabel, false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, true, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize && $lResultRepoSize == $lFileSize), true, 'db file size mismatch has reference - copy');

        # Code path to ensure reference is removed
        ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
            $oBackupManifest,
            $strHost,
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
            $lManifestSaveCurrent);

        # Confirm reference to prior backup removed
        $self->testResult(sub {$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/$strFileName.",
            MANIFEST_SUBKEY_REFERENCE)},
            false, "reference to prior backup in manifest removed");

        #---------------------------------------------------------------------------------------------------------------------------
        # BACKUP_FILE_NOOP
        # Yes prior checksum, no compression, no page checksum, no extra, yes delta, yes hasReference
        ($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
            backupFile($strFileDb, $strRepoFile, $lFileSize, $strFileHash, false,
            $strBackupLabel, false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, true, undef);

        $self->testResult(($iResultCopyResult == BACKUP_FILE_NOOP && $strResultCopyChecksum eq $strFileHash &&
            $lResultCopySize == $lFileSize), true, 'db file same has reference - noop');

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
            $iResultCopyResult,
            $lResultCopySize,
            $lResultRepoSize,
            $strResultCopyChecksum,
            $rResultExtra,
            $lSizeTotal,
            $lSizeCurrent,
            $lManifestSaveSize,
            $lManifestSaveCurrent);

        $self->testResult(($lSizeCurrent ==$lSizeCurrentAfter && $lManifestSaveCurrent == $lManifestSaveCurrentAfter),
            true, '    running counts updated');

        #---------------------------------------------------------------------------------------------------------------------------
        # Remove file from repo. No reference so should hard error since this means sometime between the building of the manifest
        # for the aborted backup, the file went missing from the aborted backup dir.
        storageTest()->remove("$strFileRepo", {bIgnoreMissing => true});

        $self->testException(sub {backupFile($strFileDb, $strRepoFile, $lFileSize, $strFileHash,
            false, $strBackupLabel, false, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, true, false, undef)},
            ERROR_FILE_MISSING, "unable to open '$strFileRepo': No such file or directory");
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
        # Checkum page exception
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

        $rResultExtra->{bValid} = false;
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
            ERROR_ASSERT, "bAlign flag should have been set for misaligned page");

        $rResultExtra->{bAlign} = true;
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
            ERROR_ASSERT, "bAlign flag should have been set for misaligned page");

        $rResultExtra->{bAlign} = false;
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
