/***********************************************************************************************************************************
Test Info Helper
***********************************************************************************************************************************/
#include "common/format/format.h"
#include "storage/helper.h"

#include "harness/config.h"
#include "harness/info.h"
#include "harness/infoHelper.h"
#include "harness/postgres.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("infoArchiveRepoIdx() and infoBackupRepoIdx()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Keep cached info between test statements
        hrnInfoHelperStatementClearSet(false);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("set and save new info");

        InfoArchive *infoArchive = infoArchiveNew(PG_VERSION_14, 6569239123849665679, REPOSITORY_FORMAT_5, cipherSpecMapNew());
        InfoBackup *infoBackup = infoBackupNew(
            PG_VERSION_14, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_14), REPOSITORY_FORMAT_5, NULL);

        TEST_RESULT_VOID(infoArchiveRepoIdxSet(0, infoArchive), "set archive info");
        TEST_RESULT_VOID(infoBackupRepoIdxSet(0, infoBackup), "set backup info");
        TEST_RESULT_BOOL(cfgOptionIdxTest(cfgOptRepoCipherRotate, 0), false, "repo1 format set");
        TEST_RESULT_VOID(infoArchiveRepoIdxSave(0), "save archive info");
        TEST_RESULT_VOID(infoBackupRepoIdxSave(0), "save backup info");
        TEST_RESULT_PTR(infoArchiveRepoIdx(0), infoArchive, "archive info is cached");
        TEST_RESULT_PTR(infoBackupRepoIdxP(0), infoBackup, "backup info is cached");
        TEST_RESULT_BOOL(storageExistsP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR), true, "archive info exists");
        TEST_RESULT_BOOL(storageExistsP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR), true, "backup info exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on set when info is cached");

        TEST_ERROR(
            infoArchiveRepoIdxSet(0, infoArchiveNew(PG_VERSION_14, 6569239123849665679, REPOSITORY_FORMAT_5, cipherSpecMapNew())),
            AssertError, "archive info is already cached");
        TEST_ERROR(
            infoBackupRepoIdxSet(
                0,
                infoBackupNew(PG_VERSION_14, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_14), REPOSITORY_FORMAT_5, NULL)),
            AssertError, "backup info is already cached");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("load and cache info");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"14\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"14\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            "[backup:current]\n"
            "20201116-200000F={\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605556800,\"backup-timestamp-stop\":1605556802,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=202107181\n"
            "db-control-version=1300\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"14\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":202107181,\"db-control-version\":1300,\"db-system-id\":6569239123849665679"
            ",\"db-version\":\"14\"}\n");

        TEST_ASSIGN(infoArchive, infoArchiveRepoIdx(1), "load archive info");
        TEST_RESULT_PTR(infoArchiveRepoIdx(1), infoArchive, "archive info is cached");
        TEST_RESULT_STR_Z(infoArchiveId(infoArchive), "14-1", "archive id");

        TEST_ERROR(
            cfgOptionIdxTest(cfgOptRepoCipherRotate, 1), AssertError,
            "repository format must be set before accessing option 'repo2-cipher-rotate'");
        TEST_ASSIGN(infoBackup, infoBackupRepoIdxP(1), "load backup info");
        TEST_RESULT_BOOL(cfgOptionIdxTest(cfgOptRepoCipherRotate, 1), false, "repo2 format set");
        TEST_RESULT_PTR(infoBackupRepoIdxP(1), infoBackup, "backup info is cached");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 1, "backup set without manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reconstruct backup info");

        TEST_RESULT_PTR(infoBackupRepoIdxP(1, .reconstruct = true), infoBackup, "reconstruct backup info");
        TEST_RESULT_LOG("P00   WARN: backup '20201116-200000F' missing manifest removed from backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 0, "backup set removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("harness error when cached info does not match the file");

        TEST_ERROR(
            infoBackupRepoIdxP(1), AssertError,
            "cached backup info for repo2 does not match the backup.info file\n"
            "HINT: was the file changed outside the info helper or the cached info changed and not saved?\n"
            "HINT: load the configuration again to load the file again.");

        TEST_RESULT_VOID(infoBackupRepoIdxSave(1), "save backup info");
        TEST_RESULT_PTR(infoBackupRepoIdxP(1, .reconstruct = true), infoBackup, "reconstruct saved backup info");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"14\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"14\"}\n");

        TEST_ERROR(
            infoArchiveRepoIdx(1), AssertError,
            "cached archive info for repo2 does not match the archive.info file\n"
            "HINT: was the file changed outside the info helper or the cached info changed and not saved?\n"
            "HINT: load the configuration again to load the file again.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("load info again after the configuration is loaded");

        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_STR_Z(infoArchiveId(infoArchiveRepoIdx(1)), "14-2", "archive info loaded from file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("harness clears cached info at the start of each test statement");

        hrnInfoHelperStatementClearSet(true);

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"14\"}\n");

        TEST_RESULT_STR_Z(infoArchiveId(infoArchiveRepoIdx(1)), "14-1", "archive info loaded from file");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
