/***********************************************************************************************************************************
Test List Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "harness/config.h"
#include "harness/info.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create storage object for writing to test locations when a stanza is not set
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // The tests expect the timezone to be UTC
    hrnTzSet("UTC");

    // *****************************************************************************************************************************
    if (testBegin("listRender()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdList, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no stanzas exist");

        HRN_STORAGE_PATH_CREATE(storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP, .comment = "create repo backup path");

        TEST_RESULT_STR_Z(listRender(), "No backups exist in the repository.\n", "no stanzas");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza path exists but backup.info does not");

        HRN_STORAGE_PATH_CREATE(storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/demo", .comment = "create stanza path");

        TEST_RESULT_STR_Z(listRender(), "No backups exist in the repository.\n", "no " INFO_BACKUP_FILE);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("requested stanza does not exist");

        StringList *argListStanza = strLstDup(argList);
        hrnCfgArgRawZ(argListStanza, cfgOptStanza, "bogus");
        HRN_CFG_LOAD(cfgCmdList, argListStanza);

        TEST_RESULT_STR_Z(listRender(), "No backups exist in the repository.\n", "stanza not found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backups on a single repo");

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/demo/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=202209061\n"
            "db-control-version=1300\n"
            "db-id=2\n"
            "db-system-id=6569239123849665680\n"
            "db-version=\"15\"\n"
            "\n"
            "[backup:current]\n"
            // Full backup, which has no prior backup and does not have its WAL copied into it
            "20260819-010003F={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000003\","
            "\"backup-error\":false,\"backup-info-repo-size\":132000000000,\"backup-info-repo-size-delta\":132000000000,"
            "\"backup-info-size\":400000000000,\"backup-info-size-delta\":400000000000,"
            "\"backup-lsn-start\":\"2F728/88000130\",\"backup-lsn-stop\":\"2F729/F957E7E8\","
            "\"backup-timestamp-start\":1770000000,\"backup-timestamp-stop\":1770003900,"
            "\"backup-type\":\"full\",\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            // Differential backup with errors detected and its WAL copied into it
            "20260819-010003F_20260820-010004D={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"000000010000000000000005\",\"backup-archive-stop\":\"000000010000000000000006\","
            "\"backup-error\":true,\"backup-info-repo-size\":185000000000,\"backup-info-repo-size-delta\":53500000000,"
            "\"backup-info-size\":400000000000,\"backup-info-size-delta\":145000000000,"
            "\"backup-lsn-start\":\"2F7A2/CC000130\",\"backup-lsn-stop\":\"2F7A3/D3F87C68\","
            "\"backup-prior\":\"20260819-010003F\",\"backup-reference\":[\"20260819-010003F\"],"
            "\"backup-timestamp-start\":1770086400,\"backup-timestamp-stop\":1770087892,"
            "\"backup-type\":\"diff\",\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            // Incremental backup that occupies no space in the repo, so the ratio cannot be calculated
            "20260819-010003F_20260821-010005I={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"000000010000000000000008\",\"backup-archive-stop\":\"000000010000000000000009\","
            "\"backup-error\":false,\"backup-info-repo-size\":185000000000,\"backup-info-repo-size-delta\":0,"
            "\"backup-info-size\":400000000000,\"backup-info-size-delta\":7683000000,"
            "\"backup-lsn-start\":\"2F7C8/A80780E8\",\"backup-lsn-stop\":\"2F7C8/B9D71858\","
            "\"backup-prior\":\"20260819-010003F_20260820-010004D\","
            "\"backup-reference\":[\"20260819-010003F\",\"20260819-010003F_20260820-010004D\"],"
            "\"backup-timestamp-start\":1770172800,\"backup-timestamp-stop\":1770173043,"
            "\"backup-type\":\"incr\",\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            // Full backup made by a version that recorded neither the WAL range, the LSN range, nor whether errors were detected
            "20260822-010006F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":null,\"backup-archive-stop\":null,"
            "\"backup-info-repo-size\":512,\"backup-info-repo-size-delta\":512,"
            "\"backup-info-size\":1024,\"backup-info-size-delta\":1024,"
            "\"backup-timestamp-start\":1770259200,\"backup-timestamp-stop\":1770259230,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            // Incremental backup that depends on a backup which does not record the WAL range
            "20260822-010006F_20260822-020009I={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"00000001000000000000000B\",\"backup-archive-stop\":\"00000001000000000000000B\","
            "\"backup-error\":false,\"backup-info-repo-size\":1024,\"backup-info-repo-size-delta\":512,"
            "\"backup-info-size\":2048,\"backup-info-size-delta\":1024,"
            "\"backup-prior\":\"20260822-010006F\",\"backup-reference\":[\"20260822-010006F\"],"
            "\"backup-timestamp-start\":1770345600,\"backup-timestamp-stop\":1770345720,"
            "\"backup-type\":\"incr\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            // Incremental backup that refers to a database and a prior backup which are both missing from backup.info
            "20260823-010007F_20260823-020008I={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"00000002000000000000000D\",\"backup-archive-stop\":\"00000002000000000000000D\","
            "\"backup-error\":false,\"backup-info-repo-size\":1024,\"backup-info-repo-size-delta\":512,"
            "\"backup-info-size\":2048,\"backup-info-size-delta\":1024,"
            "\"backup-prior\":\"20260823-010007F\",\"backup-reference\":[\"20260823-010007F\"],"
            "\"backup-timestamp-start\":1770432000,\"backup-timestamp-stop\":1770432060,"
            "\"backup-type\":\"incr\",\"db-id\":3,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679"
            ",\"db-version\":\"9.6\"}\n"
            "2={\"db-catalog-version\":202209061,\"db-control-version\":1300,\"db-system-id\":6569239123849665680"
            ",\"db-version\":\"15\"}\n",
            .comment = "put backup info to file - demo, repo1");

        HRN_CFG_LOAD(cfgCmdList, argList);

        TEST_RESULT_STR_Z(
            listRender(),
            "STANZA 'demo'\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " Repo   Version  ID                                 Recovery Time             Mode  WAL "
            "Mode  TLI  Time     Data     Zratio  Start LSN       Stop LSN        Status\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " repo1  15       20260819-010003F                   2026-02-02 03:45:00+0000  full  "
            "ARCHIVE   1/0    1h:5m  372.5GB    3.03  2F728/88000130  2F729/F957E7E8  OK\n"
            " repo1  15       20260819-010003F_20260820-010004D  2026-02-03 03:04:52+0000  diff  "
            "STREAM    1/1  24m:52s    135GB    2.71  2F7A2/CC000130  2F7A3/D3F87C68  ERROR\n"
            " repo1  15       20260819-010003F_20260821-010005I  2026-02-04 02:44:03+0000  incr  "
            "ARCHIVE   1/1    4m:3s    7.2GB       -  2F7C8/A80780E8  2F7C8/B9D71858  OK\n"
            " repo1  9.6      20260822-010006F                   2026-02-05 02:40:30+0000  full  "
            "ARCHIVE   -        30s      1KB    2.00  -               -               -\n"
            " repo1  9.6      20260822-010006F_20260822-020009I  2026-02-06 02:42:00+0000  incr  "
            "ARCHIVE   1/0    2m:0s      1KB    2.00  -               -               OK\n"
            " repo1  -        20260823-010007F_20260823-020008I  2026-02-07 02:41:00+0000  incr  "
            "ARCHIVE   2/0    1m:0s      1KB    2.00  -               -               OK\n",
            "single repo");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza option limits output to the requested stanza");

        argListStanza = strLstDup(argList);
        hrnCfgArgRawZ(argListStanza, cfgOptStanza, "demo");
        HRN_CFG_LOAD(cfgCmdList, argListStanza);

        TEST_RESULT_STR_Z(
            listRender(),
            "STANZA 'demo'\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " Repo   Version  ID                                 Recovery Time             Mode  WAL "
            "Mode  TLI  Time     Data     Zratio  Start LSN       Stop LSN        Status\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " repo1  15       20260819-010003F                   2026-02-02 03:45:00+0000  full  "
            "ARCHIVE   1/0    1h:5m  372.5GB    3.03  2F728/88000130  2F729/F957E7E8  OK\n"
            " repo1  15       20260819-010003F_20260820-010004D  2026-02-03 03:04:52+0000  diff  "
            "STREAM    1/1  24m:52s    135GB    2.71  2F7A2/CC000130  2F7A3/D3F87C68  ERROR\n"
            " repo1  15       20260819-010003F_20260821-010005I  2026-02-04 02:44:03+0000  incr  "
            "ARCHIVE   1/1    4m:3s    7.2GB       -  2F7C8/A80780E8  2F7C8/B9D71858  OK\n"
            " repo1  9.6      20260822-010006F                   2026-02-05 02:40:30+0000  full  "
            "ARCHIVE   -        30s      1KB    2.00  -               -               -\n"
            " repo1  9.6      20260822-010006F_20260822-020009I  2026-02-06 02:42:00+0000  incr  "
            "ARCHIVE   1/0    2m:0s      1KB    2.00  -               -               OK\n"
            " repo1  -        20260823-010007F_20260823-020008I  2026-02-07 02:41:00+0000  incr  "
            "ARCHIVE   2/0    1m:0s      1KB    2.00  -               -               OK\n",
            "stanza option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backups on a second repo");

        StringList *argListRepo = strLstNew();
        hrnCfgArgKeyRawZ(argListRepo, cfgOptRepoPath, 1, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argListRepo, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        HRN_CFG_LOAD(cfgCmdList, argListRepo);

        // The full backup is also in repo2, along with a backup of a stanza that is only in repo2
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_BACKUP "/demo/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=202209061\n"
            "db-control-version=1300\n"
            "db-id=1\n"
            "db-system-id=6569239123849665680\n"
            "db-version=\"15\"\n"
            "\n"
            "[backup:current]\n"
            "20260819-010003F={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000003\","
            "\"backup-error\":false,\"backup-info-repo-size\":132000000000,\"backup-info-repo-size-delta\":132000000000,"
            "\"backup-info-size\":400000000000,\"backup-info-size-delta\":400000000000,"
            "\"backup-lsn-start\":\"2F728/88000130\",\"backup-lsn-stop\":\"2F729/F957E7E8\","
            "\"backup-timestamp-start\":1770000000,\"backup-timestamp-stop\":1770003900,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":202209061,\"db-control-version\":1300,\"db-system-id\":6569239123849665680"
            ",\"db-version\":\"15\"}\n",
            .comment = "put backup info to file - demo, repo2");

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_BACKUP "/alpha/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=202209061\n"
            "db-control-version=1300\n"
            "db-id=1\n"
            "db-system-id=6569239123849665681\n"
            "db-version=\"15\"\n"
            "\n"
            "[backup:current]\n"
            "20260810-010001F={\"backrest-format\":5,\"backrest-version\":\"2.60.0\","
            "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000001\","
            "\"backup-error\":false,\"backup-info-repo-size\":1024,\"backup-info-repo-size-delta\":1024,"
            "\"backup-info-size\":2048,\"backup-info-size-delta\":2048,"
            "\"backup-lsn-start\":\"1/1000028\",\"backup-lsn-stop\":\"1/10000F8\","
            "\"backup-timestamp-start\":1769000000,\"backup-timestamp-stop\":1769000010,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":202209061,\"db-control-version\":1300,\"db-system-id\":6569239123849665681"
            ",\"db-version\":\"15\"}\n",
            .comment = "put backup info to file - alpha, repo2");

        TEST_RESULT_STR_Z(
            listRender(),
            "STANZA 'alpha'\n"
            "------------------------------------------------------------------------------------------"
            "----------------------------------------\n"
            " Repo   Version  ID                Recovery Time             Mode  WAL Mode  TLI  Time  "
            "Data  Zratio  Start LSN  Stop LSN   Status\n"
            "------------------------------------------------------------------------------------------"
            "----------------------------------------\n"
            " repo2  15       20260810-010001F  2026-01-21 12:53:30+0000  full  ARCHIVE   1/0   10s   "
            "2KB    2.00  1/1000028  1/10000F8  OK\n"
            "\n"
            "STANZA 'demo'\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " Repo   Version  ID                                 Recovery Time             Mode  WAL "
            "Mode  TLI  Time     Data     Zratio  Start LSN       Stop LSN        Status\n"
            "------------------------------------------------------------------------------------------"
            "-------------------------------------------------------------------------\n"
            " repo1  15       20260819-010003F                   2026-02-02 03:45:00+0000  full  "
            "ARCHIVE   1/0    1h:5m  372.5GB    3.03  2F728/88000130  2F729/F957E7E8  OK\n"
            " repo2  15       20260819-010003F                   2026-02-02 03:45:00+0000  full  "
            "ARCHIVE   1/0    1h:5m  372.5GB    3.03  2F728/88000130  2F729/F957E7E8  OK\n"
            " repo1  15       20260819-010003F_20260820-010004D  2026-02-03 03:04:52+0000  diff  "
            "STREAM    1/1  24m:52s    135GB    2.71  2F7A2/CC000130  2F7A3/D3F87C68  ERROR\n"
            " repo1  15       20260819-010003F_20260821-010005I  2026-02-04 02:44:03+0000  incr  "
            "ARCHIVE   1/1    4m:3s    7.2GB       -  2F7C8/A80780E8  2F7C8/B9D71858  OK\n"
            " repo1  9.6      20260822-010006F                   2026-02-05 02:40:30+0000  full  "
            "ARCHIVE   -        30s      1KB    2.00  -               -               -\n"
            " repo1  9.6      20260822-010006F_20260822-020009I  2026-02-06 02:42:00+0000  incr  "
            "ARCHIVE   1/0    2m:0s      1KB    2.00  -               -               OK\n"
            " repo1  -        20260823-010007F_20260823-020008I  2026-02-07 02:41:00+0000  incr  "
            "ARCHIVE   2/0    1m:0s      1KB    2.00  -               -               OK\n",
            "all repos");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo option limits output to the requested repo");

        StringList *argListRepoOne = strLstDup(argListRepo);
        hrnCfgArgRawZ(argListRepoOne, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdList, argListRepoOne);

        TEST_RESULT_STR_Z(
            listRender(),
            "STANZA 'alpha'\n"
            "------------------------------------------------------------------------------------------"
            "----------------------------------------\n"
            " Repo   Version  ID                Recovery Time             Mode  WAL Mode  TLI  Time  "
            "Data  Zratio  Start LSN  Stop LSN   Status\n"
            "------------------------------------------------------------------------------------------"
            "----------------------------------------\n"
            " repo2  15       20260810-010001F  2026-01-21 12:53:30+0000  full  ARCHIVE   1/0   10s   "
            "2KB    2.00  1/1000028  1/10000F8  OK\n"
            "\n"
            "STANZA 'demo'\n"
            "------------------------------------------------------------------------------------------"
            "------------------------------------------------------\n"
            " Repo   Version  ID                Recovery Time             Mode  WAL Mode  TLI  Time   "
            "Data     Zratio  Start LSN       Stop LSN        Status\n"
            "------------------------------------------------------------------------------------------"
            "------------------------------------------------------\n"
            " repo2  15       20260819-010003F  2026-02-02 03:45:00+0000  full  ARCHIVE   1/0  1h:5m  "
            "372.5GB    3.03  2F728/88000130  2F729/F957E7E8  OK\n",
            "repo option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("type option limits output to the requested type");

        StringList *argListType = strLstDup(argListRepo);
        hrnCfgArgRawZ(argListType, cfgOptType, "diff");
        HRN_CFG_LOAD(cfgCmdList, argListType);

        TEST_RESULT_STR_Z(
            listRender(),
            "STANZA 'demo'\n"
            "------------------------------------------------------------------------------------------"
            "-----------------------------------------------------------------------\n"
            " Repo   Version  ID                                 Recovery Time             Mode  WAL "
            "Mode  TLI  Time     Data   Zratio  Start LSN       Stop LSN        Status\n"
            "------------------------------------------------------------------------------------------"
            "-----------------------------------------------------------------------\n"
            " repo1  15       20260819-010003F_20260820-010004D  2026-02-03 03:04:52+0000  diff  "
            "STREAM    1/1  24m:52s  135GB    2.71  2F7A2/CC000130  2F7A3/D3F87C68  ERROR\n",
            "type option");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdList()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo3");
        HRN_CFG_LOAD(cfgCmdList, argList);

        HRN_STORAGE_PATH_CREATE(storageTest, TEST_PATH "/repo3/" STORAGE_PATH_BACKUP, .comment = "create repo backup path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no backups exist");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        const String *stdoutFile = STRDEF(TEST_PATH "/stdout.list");

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdList();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        // Check output of list command stored in file
        TEST_STORAGE_GET(storageTest, strZ(stdoutFile), "No backups exist in the repository.\n", .remove = true);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
