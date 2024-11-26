/***********************************************************************************************************************************
Test Manifest Command
***********************************************************************************************************************************/
#include "storage/helper.h"

#include "command/backup/common.h"
#include "command/backup/protocol.h"
#include "command/stanza/create.h"
#include "storage/posix/storage.h"

#include "common/harnessBackup.h"
#include "common/harnessConfig.h"
#include "common/harnessPostgres.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // The tests expect the timezone to be UTC
    hrnTzSet("UTC");

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_BACKUP_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, LENGTH_OF(testLocalHandlerList));

    // Test storage
    const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // Common config
    HRN_STORAGE_PUT_Z(
        storageTest, "pgbackrest.conf",
        "[global]\n"
        "repo1-path=" TEST_PATH "/repo1\n"
        "repo1-retention-full=999\n"
        "repo1-bundle=y\n"
        "repo1-block=y\n"
        "repo1-block-size-super-full=32KiB\n"
        "repo2-path="TEST_PATH "/repo2\n"
        "repo2-retention-full=999\n"
        "repo2-cipher-type=aes-256-cbc\n"
        "repo2-cipher-pass=" TEST_CIPHER_PASS "\n"
        "repo2-bundle=y\n"
        "repo2-block=y\n"
        "\n"
        "archive-check=n\n"
        "stop-auto=y\n"
        "compress-type=none\n"
        "\n"
        "[test]\n"
        "pg1-path=" TEST_PATH "/pg1\n");

    StringList *const argListCommon = strLstNew();
    hrnCfgArgRawZ(argListCommon, cfgOptStanza, "test");
    hrnCfgArgRawZ(argListCommon, cfgOptConfig, TEST_PATH "/pgbackrest.conf");

    // Disable most logging
    harnessLogLevelSet(logLevelWarn);

    // *****************************************************************************************************************************
    if (testBegin("cmdManifest()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza create");
        {
            StringList *argList = strLstDup(argListCommon);
            hrnCfgArgRawBool(argList, cfgOptOnline, false);
            HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

            // Create pg_control and run stanza-create
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);
            TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full backup with block incr");
        {
            const time_t backupTimeStart = BACKUP_EPOCH;

            // Version file that will not be updated after the full backup
            HRN_STORAGE_PUT_Z(
                storagePgWrite(), PG_PATH_BASE "/" PG_FILE_PGVERSION, PG_VERSION_95_Z, .timeModified = backupTimeStart);

            // Zeroed file large enough to use block incr
            Buffer *relation = bufNew(8 * 8192);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            StringList *argList = strLstDup(argListCommon);
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Backup to repo1
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true, .fullIncrNoOp = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup repo1");

            // Backup to repo2
            hrnCfgArgRawZ(argList, cfgOptRepo, "2");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            hrnBackupPqScriptP(
                PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true, .fullIncrNoOp = true,
                .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup repo2");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("diff backup with block incr");
        {
            const time_t backupTimeStart = BACKUP_EPOCH + 100000;

            // Zeroed file large enough to use block incr
            Buffer *relation = bufNew(12 * 8192);
            memset(bufPtr(relation), 0, bufSize(relation));
            memset(bufPtr(relation) + 2 * 8192, 1, 4 * 8192);
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            StringList *argList = strLstDup(argListCommon);
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Backup to repo1
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup repo1");

            // Munge the pg_control checksum since it will vary by architecture
            Manifest *manifest = manifestLoadFile(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/20191002-070640F_20191003-105320D/" BACKUP_MANIFEST_FILE),
                cipherTypeNone, NULL);

            ManifestFile file = manifestFileFind(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL));
            file.checksumSha1 = bufPtr(bufNewDecode(encodingHex, STRDEF("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
            manifestFileUpdate(manifest, &file);

            manifestSave(
                manifest,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        STRDEF(STORAGE_REPO_BACKUP "/20191002-070640F_20191003-105320D/" BACKUP_MANIFEST_FILE))));

            // Backup to repo2
            hrnCfgArgRawZ(argList, cfgOptRepo, "2");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            hrnBackupPqScriptP(
                PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup repo2");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest block map");
        {
            TEST_TITLE("single file block map from full");

            StringList *argList = strLstDup(argListCommon);
            hrnCfgArgRawZ(argList, cfgOptFilter, MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/1/2");
            hrnCfgArgRawZ(argList, cfgOptSet, "20191002-070640F");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F\n"
                "reference: 20191002-070640F\n"
                "type: full\n"
                "time: start: 2019-10-02 07:06:40, stop: 2019-10-02 21:40:27, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/1/2\n"
                "      size: 64KB, repo 64KB\n"
                "      checksum: 1adc95bebe9eea8c112d40cd04ab7a8d75c4f961\n"
                "      bundle: 1\n"
                "      block: size 8KB, map size 58B, checksum size 6B\n"
                "      block delta:\n"
                "        reference: 20191002-070640F/bundle/1, read: 1/64KB, superBlock: 2/64KB, block: 8/64KB\n"
                "        total read: 1/64KB, superBlock: 2/64KB, block: 8/64KB\n",
                "repo 1 text");

            hrnCfgArgRawZ(argList, cfgOptOutput, "json");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                // {uncrustify_off - indentation}
                "{"
                    "\"label\":\"20191002-070640F\","
                    "\"reference\":["
                        "\"20191002-070640F\""
                    "],"
                    "\"type\":\"full\","
                    "\"time\":{"
                        "\"start\":1570000000,"
                        "\"copy\":1570000001,"
                        "\"stop\":1570052427"
                    "},"
                    "\"bundle\":{"
                        "\"bundle\":true,"
                        "\"raw\":true"
                    "},"
                    "\"block\":{"
                        "\"block\":true"
                    "},"
                    "\"fileList\":["
                        "{"
                            "\"name\":\"pg_data/base/1/2\","
                            "\"size\":65536,"
                            "\"checksum\":\"1adc95bebe9eea8c112d40cd04ab7a8d75c4f961\","
                            "\"repo\":{"
                                "\"size\":65594"
                            "},"
                            "\"bundle\":{"
                                "\"id\":1,"
                                "\"offset\":8195"
                            "},"
                            "\"block\":{"
                                "\"size\":8192,"
                                "\"map\":{"
                                    "\"size\":58,"
                                    "\"delta\":["
                                        "{"
                                            "\"reference\":0,"
                                            "\"read\":{"
                                                "\"total\":1,"
                                                "\"size\":65536"
                                            "},"
                                            "\"superBlock\":{"
                                                "\"total\":2,"
                                                "\"size\":65536"
                                            "},"
                                            "\"block\":{"
                                                "\"total\":8"
                                            "}"
                                        "}"
                                    "]"
                                "},"
                                "\"checksum\":{"
                                    "\"size\":6"
                                "}"
                            "}"
                        "}"
                    "]"
                "}",
                // {uncrustify_on}
                "repo 1 json");

            TEST_RESULT_VOID(jsonToVar(cmdManifestRender()), "check json");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("multiple files from diff");

            argList = strLstDup(argListCommon);
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F_20191003-105320D\n"
                "reference: 20191002-070640F, 20191002-070640F_20191003-105320D\n"
                "type: diff\n"
                "time: start: 2019-10-03 10:53:20, stop: 2019-10-04 01:27:07, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/1/2\n"
                "      size: 96KB, repo 64KB\n"
                "      checksum: d4976e362696a43fb09e7d4e780d7d9352a2ec2e\n"
                "      bundle: 1\n"
                "      block: size 8KB, map size 99B, checksum size 6B\n"
                "\n"
                "  - pg_data/base/PG_VERSION\n"
                "      reference: 20191002-070640F\n"
                "      size: 3B, repo 3B\n"
                "      checksum: 06d06bb31b570b94d7b4325f511f853dbe771c21\n"
                "      bundle: 1\n"
                "\n"
                "  - pg_data/global/pg_control\n"
                "      size: 8KB, repo 8KB\n"
                "      checksum: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                "      bundle: 1\n",
                "repo 1 text");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("reference from diff");

            argList = strLstDup(argListCommon);
            hrnCfgArgRawZ(argList, cfgOptReference, "20191002-070640F");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F_20191003-105320D\n"
                "reference: 20191002-070640F, 20191002-070640F_20191003-105320D\n"
                "type: diff\n"
                "time: start: 2019-10-03 10:53:20, stop: 2019-10-04 01:27:07, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/PG_VERSION\n"
                "      reference: 20191002-070640F\n"
                "      size: 3B, repo 3B\n"
                "      checksum: 06d06bb31b570b94d7b4325f511f853dbe771c21\n"
                "      bundle: 1\n",
                "repo 1 text");

            hrnCfgArgRawZ(argList, cfgOptOutput, "json");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                // {uncrustify_off - indentation}
                "{"
                    "\"label\":\"20191002-070640F_20191003-105320D\","
                    "\"reference\":["
                        "\"20191002-070640F\","
                        "\"20191002-070640F_20191003-105320D\""
                    "],"
                    "\"type\":\"diff\","
                    "\"time\":{"
                        "\"start\":1570100000,"
                        "\"copy\":1570100001,"
                        "\"stop\":1570152427"
                    "},"
                    "\"bundle\":{"
                        "\"bundle\":true,"
                        "\"raw\":true"
                    "},"
                    "\"block\":{"
                        "\"block\":true"
                    "},"
                    "\"fileList\":["
                        "{"
                            "\"name\":\"pg_data/base/PG_VERSION\","
                            "\"reference\":\"20191002-070640F\","
                            "\"size\":3,"
                            "\"checksum\":\"06d06bb31b570b94d7b4325f511f853dbe771c21\","
                            "\"repo\":{"
                                "\"size\":3"
                            "},"
                            "\"bundle\":{"
                                "\"id\":1,"
                                "\"offset\":8192"
                            "}"
                        "}"
                    "]"
                "}",
                // {uncrustify_on}
                "repo 2 test");

            TEST_RESULT_VOID(jsonToVar(cmdManifestRender()), "check json");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("latest reference from diff");

            argList = strLstDup(argListCommon);
            hrnCfgArgRawZ(argList, cfgOptReference, "latest");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F_20191003-105320D\n"
                "reference: 20191002-070640F, 20191002-070640F_20191003-105320D\n"
                "type: diff\n"
                "time: start: 2019-10-03 10:53:20, stop: 2019-10-04 01:27:07, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/1/2\n"
                "      size: 96KB, repo 64KB\n"
                "      checksum: d4976e362696a43fb09e7d4e780d7d9352a2ec2e\n"
                "      bundle: 1\n"
                "      block: size 8KB, map size 99B, checksum size 6B\n"
                "\n"
                "  - pg_data/global/pg_control\n"
                "      size: 8KB, repo 8KB\n"
                "      checksum: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                "      bundle: 1\n",
                "repo 1 text");
            hrnCfgArgRawZ(argList, cfgOptOutput, "json");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                // {uncrustify_off - indentation}
                "{"
                    "\"label\":\"20191002-070640F_20191003-105320D\","
                    "\"reference\":["
                        "\"20191002-070640F\","
                        "\"20191002-070640F_20191003-105320D\""
                    "],"
                    "\"type\":\"diff\","
                    "\"time\":{"
                        "\"start\":1570100000,"
                        "\"copy\":1570100001,"
                        "\"stop\":1570152427"
                    "},"
                    "\"bundle\":{"
                        "\"bundle\":true,"
                        "\"raw\":true"
                    "},"
                    "\"block\":{"
                        "\"block\":true"
                    "},"
                    "\"fileList\":["
                        "{"
                            "\"name\":\"pg_data/base/1/2\","
                            "\"size\":98304,"
                            "\"checksum\":\"d4976e362696a43fb09e7d4e780d7d9352a2ec2e\","
                            "\"repo\":{"
                                "\"size\":65635"
                            "},"
                            "\"bundle\":{"
                                "\"id\":1,"
                                "\"offset\":8192"
                            "},"
                            "\"block\":{"
                                "\"size\":8192,"
                                "\"map\":{"
                                    "\"size\":99"
                                "},"
                                "\"checksum\":{"
                                    "\"size\":6"
                                "}"
                            "}"
                        "},"
                        "{"
                            "\"name\":\"pg_data/global/pg_control\","
                            "\"size\":8192,"
                            "\"checksum\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
                            "\"repo\":{"
                                "\"size\":8192"
                            "},"
                            "\"bundle\":{"
                                "\"id\":1,"
                                "\"offset\":0"
                            "}"
                        "}"
                    "]"
                "}",
                // {uncrustify_on}
                "repo 2 test");

            TEST_RESULT_VOID(jsonToVar(cmdManifestRender()), "check json");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("single file block map from diff");

            argList = strLstDup(argListCommon);
            hrnCfgArgRawZ(argList, cfgOptFilter, MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/1/2");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F_20191003-105320D\n"
                "reference: 20191002-070640F, 20191002-070640F_20191003-105320D\n"
                "type: diff\n"
                "time: start: 2019-10-03 10:53:20, stop: 2019-10-04 01:27:07, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/1/2\n"
                "      size: 96KB, repo 64KB\n"
                "      checksum: d4976e362696a43fb09e7d4e780d7d9352a2ec2e\n"
                "      bundle: 1\n"
                "      block: size 8KB, map size 99B, checksum size 6B\n"
                "      block delta:\n"
                "        reference: 20191002-070640F/bundle/1, read: 1/64KB, superBlock: 2/64KB, block: 4/32KB\n"
                "        reference: 20191002-070640F_20191003-105320D/bundle/1, read: 1/64KB, superBlock: 1/64KB, block: 8/64KB\n"
                "        total read: 2/128KB, superBlock: 3/128KB, block: 12/96KB\n",
                "repo 1 text");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("single file block map delta from diff");

            argList = strLstDup(argListCommon);
            hrnCfgArgRawZ(argList, cfgOptFilter, MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/1/2");
            hrnCfgArgRawZ(argList, cfgOptRepo, "2");
            hrnCfgArgRawZ(argList, cfgOptPg, "1");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                "label: 20191002-070640F_20191003-105320D\n"
                "reference: 20191002-070640F, 20191002-070640F_20191003-105320D\n"
                "type: diff\n"
                "time: start: 2019-10-03 10:53:20, stop: 2019-10-04 01:27:07, duration: 14:33:47\n"
                "bundle: true\n"
                "block: true\n"
                "\n"
                "file list:\n"
                "  - pg_data/base/1/2\n"
                "      size: 96KB, repo 64.1KB\n"
                "      checksum: d4976e362696a43fb09e7d4e780d7d9352a2ec2e\n"
                "      bundle: 1\n"
                "      block: size 8KB, map size 104B, checksum size 6B\n"
                "      block delta: file is up-to-date\n",
                "repo 2 test");

            hrnCfgArgRawZ(argList, cfgOptOutput, "json");
            HRN_CFG_LOAD(cfgCmdManifest, argList);

            TEST_RESULT_STR_Z(
                cmdManifestRender(),
                // {uncrustify_off - indentation}
                "{"
                    "\"label\":\"20191002-070640F_20191003-105320D\","
                    "\"reference\":["
                        "\"20191002-070640F\","
                        "\"20191002-070640F_20191003-105320D\""
                    "],"
                    "\"type\":\"diff\","
                    "\"time\":{"
                        "\"start\":1570100000,"
                        "\"copy\":1570100001,"
                        "\"stop\":1570152427"
                    "},"
                    "\"bundle\":{"
                        "\"bundle\":true,"
                        "\"raw\":true"
                    "},"
                    "\"block\":{"
                        "\"block\":true"
                    "},"
                    "\"fileList\":["
                        "{"
                            "\"name\":\"pg_data/base/1/2\","
                            "\"size\":98304,"
                            "\"checksum\":\"d4976e362696a43fb09e7d4e780d7d9352a2ec2e\","
                            "\"repo\":{"
                                "\"size\":65664"
                            "},"
                            "\"bundle\":{"
                                "\"id\":1,"
                                "\"offset\":8216"
                            "},"
                            "\"block\":{"
                                "\"size\":8192,"
                                "\"map\":{"
                                    "\"size\":104,"
                                    "\"delta\":null"
                                "},"
                                "\"checksum\":{"
                                    "\"size\":6"
                                "}"
                            "}"
                        "}"
                    "]"
                "}",
                // {uncrustify_on}
                "repo 2 test");

            TEST_RESULT_VOID(jsonToVar(cmdManifestRender()), "check json");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
