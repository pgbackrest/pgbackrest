/***********************************************************************************************************************************
Test Restore Command
***********************************************************************************************************************************/
#include "common/compress/gzip/compress.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/io.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Start a protocol server to test the protocol directly
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(
        strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = strNew("20190509F");
        const String *repoFile1 = strNew("pg_data/testfile");

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("sparse-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                true, 0x10000000000UL, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "zero sparse 1TB file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("sparse-zero")).size, 0x10000000000UL, "    check size");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("normal-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, NULL),
            true, "zero-length file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("normal-zero")).size, 0, "    check size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a compressed encrypted repo file
        StorageWrite *ceRepoFile = storageNewWriteNP(
            storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(repoFileReferenceFull), strPtr(repoFile1)));
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(ceRepoFile));
        ioFilterGroupAdd(filterGroup, gzipCompressNew(3, false));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("badpass"), NULL));

        storagePutNP(ceRepoFile, BUFSTRDEF("acefile"));

        TEST_ERROR(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("ffffffffffffffffffffffffffffffffffffffff"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            ChecksumError,
            "error restoring 'normal': actual checksum 'd1cd8a7d11daa26814b93eb604e1d49ab4b43770' does not match expected checksum"
                " 'ffffffffffffffffffffffffffffffffffffffff'");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            true, "copy file");

        StorageInfo info = storageInfoNP(storagePg(), strNew("normal"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 7, "    check size");
        TEST_RESULT_UINT(info.mode, 0600, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432154, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("normal"))))), "acefile", "    check contents");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a repo file
        storagePutNP(
            storageNewWriteNP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(repoFileReferenceFull), strPtr(repoFile1))),
            BUFSTRDEF("atestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta missing");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(4);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing");

        ioBufferSizeSet(oldBufferSize);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            false, "sha1 delta force existing");

        // Change the existing file so it no longer matches by size
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        // Change the existing file so it no longer matches by content
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, content differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, timestamp differs");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432153, true, true, NULL),
            true, "delta force existing, timestamp after copy time");

        // Change the existing file to zero-length
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF(""));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing, content differs");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        info = storageInfoNP(storagePg(), strNew("protocol"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 9, "    check size");
        TEST_RESULT_UINT(info.mode, 0677, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432100, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("protocol"))))), "atestfile", "    check contents");

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, NULL);

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(restoreProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdRestore()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Locality error
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--pg1-host=pg1");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // PGDATA missing
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(cmdRestore(), PathMissingError, "$PGDATA directory '%s/pg' does not exist", testPath());

        // Create PGDATA
        storagePathCreateNP(storagePgWrite(), strNew("pg"));

        // postmaster.pid is present
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("postmaster.pid")), NULL);

        TEST_ERROR_FMT(
            cmdRestore(), PostmasterRunningError,
            "unable to restore while PostgreSQL is running\n"
                "HINT: presence of 'postmaster.pid' in '%s/pg' indicates PostgreSQL is running.\n"
                "HINT: remove 'postmaster.pid' only if PostgreSQL is not running.",
            testPath());

        storageRemoveP(storagePgWrite(), strNew("postmaster.pid"), .errorOnMissing = true);

        // Error when no backups are present then add backups to backup.info
        // -------------------------------------------------------------------------------------------------------------------------
        String *infoBackupStr = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR), harnessInfoChecksum(infoBackupStr)),
            "write backup info");

        TEST_ERROR_FMT(cmdRestore(), BackupSetInvalidError, "no backup sets to restore");

        infoBackupStr = strNew
        (
            "[backup:current]\n"
            "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"diff\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":null,\"backup-archive-stop\":null,"
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\","
            "\"20161219-212741F_20161219-212803D\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"incr\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR), harnessInfoChecksum(infoBackupStr)),
            "write backup info");

        // Error on invalid backup set
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=BOGUS");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), BackupSetInvalidError, "backup set BOGUS is not valid");

        // Error on missing backup set
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F_20161219-212803D");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdRestore(), FileMissingError,
            "unable to load backup manifest file '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest' or"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest.copy':\n"
            "FileMissingError: unable to open missing file"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest' for read\n"
            "FileMissingError: unable to open missing file"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest.copy' for read",
            testPath(), testPath(), testPath(), testPath());

        // Write a valid manifest
        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20161219-212741F_20161219-212918I\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"incr\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-compress=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "master=true\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        InfoManifest *manifest = infoManifestNewLoad(ioBufferReadNew(contentLoad));

        // InfoManifest *manifest = infoManifestNewInternal();
        // InfoManifestTarget targetBase = {.name = INFO_MANIFEST_TARGET_PGDATA_STR, .path = pgPath};
        // infoManifestTargetAdd(manifest, &targetBase);
        // InfoManifestPath pathBase = {.name = pgPath};
        // infoManifestPathAdd(manifest, &pathBase);
        // InfoManifestFile fileVersion = {.name = STRDEF(INFO_MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION), .primary = false};
        // infoManifestFileAdd(manifest, &fileVersion);

        infoManifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" INFO_MANIFEST_FILE))));

        // PGDATA directory does not look valid
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "restore --delta with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "--delta set to false");
        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                        " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                        " exist in the destination directories the restore will be aborted.\n"
                    "P00   INFO: restore backup set 20161219-212741F_20161219-212918I",
                testPath())));

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("backup.manifest")), NULL);
        TEST_RESULT_VOID(cmdRestore(), "restore --delta with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew("backup.manifest"), .errorOnMissing = true);
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--force");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "restore --force with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "--force set to false");
        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                        " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                        " exist in the destination directories the restore will be aborted.\n"
                    "P00   INFO: restore backup set 20161219-212741F_20161219-212918I",
                testPath())));

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew(PG_FILE_PGVERSION)), NULL);
        TEST_RESULT_VOID(cmdRestore(), "restore --force with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew(PG_FILE_PGVERSION), .errorOnMissing = true);
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");

        // Error on mismatched label
        // -------------------------------------------------------------------------------------------------------------------------
        const String *oldLabel = manifest->data.backupLabel;
        manifest->data.backupLabel = STRDEF("20161219-212741F");

        infoManifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" INFO_MANIFEST_FILE))));

        TEST_ERROR(
            cmdRestore(), FormatError,
            "requested backup '20161219-212741F_20161219-212918I' and manifest label '20161219-212741F' do not match\n"
            "HINT: this indicates some sort of corruption (at the very least paths have been renamed).");

        manifest->data.backupLabel = oldLabel;

        infoManifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" INFO_MANIFEST_FILE))));

        // SUCCESS TEST FOR COVERAGE -- WILL BE REMOVED / MODIFIED AT SOME POINT
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F_20161219-212918I");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
