/***********************************************************************************************************************************
Test Annotate Command
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdAnnotate()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptStanza, "stanza1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info file missing");

        StringList *argListAnnotation = strLstDup(argList);
        hrnCfgArgRawZ(argListAnnotation, cfgOptAnnotation, "key1=value1");
        hrnCfgArgRawZ(argListAnnotation, cfgOptRepo, "1");
        hrnCfgArgRawZ(argListAnnotation, cfgOptSet, "20201116-200000F");
        HRN_CFG_LOAD(cfgCmdAnnotate, argListAnnotation);

        TEST_ERROR(
            cmdAnnotate(), CommandError, CFGCMD_ANNOTATE " command encountered 1 error(s), check the log file for details");

        TEST_RESULT_LOG(
            "P00  ERROR: [055]: repo1: unable to load info file '" TEST_PATH "/repo/backup/stanza1/backup.info' or"
            " '" TEST_PATH "/repo/backup/stanza1/backup.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/backup/stanza1/backup.info' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH
            "/repo/backup/stanza1/backup.info.copy' for read\n"
            "            HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "            HINT: has a stanza-create been performed?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("annotate backup success");

        // Add backup info
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[backup:current]\n"
            "20201116-200000F={\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
            "\"backup-error\":true,"
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605556800,\"backup-timestamp-stop\":1605556802,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":true,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089"
            ",\"db-version\":\"9.5\"}\n",
            .comment = "write backup info - stanza1, repo1");

        // Add backup manifest
        #define TEST_MANIFEST_HEADER                                                                                               \
            "[backup]\n"                                                                                                           \
            "backup-archive-start=\"000000010000000000000004\"\n"                                                                  \
            "backup-archive-stop=\"000000010000000000000004\"\n"                                                                   \
            "backup-label=\"20201116-200000F\"\n"                                                                                  \
            "backup-timestamp-copy-start=1605556800\n"                                                                             \
            "backup-timestamp-start=1605556800\n"                                                                                  \
            "backup-timestamp-stop=1605556802\n"                                                                                   \
            "backup-type=\"full\"\n"                                                                                               \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201510051\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=1\n"                                                                                                            \
            "db-system-id=6626363367545678089\n"                                                                                   \
            "db-version=\"9.5\"\n"                                                                                                 \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=true\n"                                                                                          \
            "option-archive-copy=false\n"                                                                                          \
            "option-backup-standby=false\n"                                                                                        \
            "option-buffer-size=16384\n"                                                                                           \
            "option-checksum-page=false\n"                                                                                         \
            "option-compress=false\n"                                                                                              \
            "option-compress-level=3\n"                                                                                            \
            "option-compress-level-network=3\n"                                                                                    \
            "option-delta=false\n"                                                                                                 \
            "option-hardlink=true\n"                                                                                               \
            "option-online=true\n"                                                                                                 \
            "option-process-max=32\n"                                                                                              \

        #define TEST_MANIFEST_TARGET                                                                                               \
            "\n"                                                                                                                   \
            "[backup:target]\n"                                                                                                    \
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"                                                                  \
            "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"                         \
            "pg_data/pg_stat={\"path\":\"../pg_stat\",\"type\":\"link\"}\n"                                                        \
            "pg_tblspc/1={\"path\":\"/tblspc/ts1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"       \
            "pg_tblspc/12={\"path\":\"/tblspc/ts12\",\"tablespace-id\":\"12\",\"tablespace-name\":\"ts12\",\"type\":\"link\"}\n"   \

        #define TEST_MANIFEST_DB                                                                                                   \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"                                                                 \
            "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"                                                             \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        #define TEST_MANIFEST_FILE                                                                                                 \
            "\n"                                                                                                                   \
            "[target:file]\n"                                                                                                      \
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\""                                        \
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"                      \
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"          \
                ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"                        \
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"  \
                ",\"timestamp\":1565282115}\n"                                                                                     \
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"           \
                ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"                              \
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"     \
                ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"                                   \
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"size\":4457"                     \
                ",\"timestamp\":1565282114}\n"                                                                                     \
            "pg_data/special={\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"                             \

        #define TEST_MANIFEST_FILE_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "mode=\"0600\"\n"                                                                                                      \
            "user=\"user1\"\n"                                                                                                     \

        #define TEST_MANIFEST_LINK                                                                                                 \
            "\n"                                                                                                                   \
            "[target:link]\n"                                                                                                      \
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"                                                                   \
            "pg_data/postgresql.conf={\"destination\":\"../pg_config/postgresql.conf\",\"group\":false,\"user\":\"user1\"}\n"      \

        #define TEST_MANIFEST_LINK_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:link:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "user=false\n"                                                                                                         \

        #define TEST_MANIFEST_PATH                                                                                                 \
            "\n"                                                                                                                   \
            "[target:path]\n"                                                                                                      \
            "pg_data={\"user\":\"user2\"}\n"                                                                                       \
            "pg_data/base={\"group\":\"group2\"}\n"                                                                                \
            "pg_data/base/16384={\"mode\":\"0750\"}\n"                                                                             \
            "pg_data/base/32768={}\n"                                                                                              \
            "pg_data/base/65536={\"user\":false}\n"                                                                                \

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=false\n"                                                                                                        \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"user1\"\n"                                                                                                     \

        // Create manifest file
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20201116-200000F/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "write backup manifest - stanza1, repo1");

        TEST_RESULT_VOID(cmdAnnotate(), "annotate 20201116-200000F backup set");
        TEST_RESULT_LOG("P00   INFO: backup set '20201116-200000F' to annotate found in repo1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("wrong backup label format");

        argListAnnotation = strLstDup(argList);
        hrnCfgArgRawZ(argListAnnotation, cfgOptAnnotation, "key1=value1");
        hrnCfgArgRawZ(argListAnnotation, cfgOptSet, "backuplabel");
        HRN_CFG_LOAD(cfgCmdAnnotate, argListAnnotation);

        TEST_ERROR(cmdAnnotate(), OptionInvalidValueError, "'backuplabel' is not a valid backup label format");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup set not found");

        argListAnnotation = strLstDup(argList);
        hrnCfgArgRawZ(argListAnnotation, cfgOptAnnotation, "key1=value1");
        hrnCfgArgRawZ(argListAnnotation, cfgOptSet, "20201116-200001F");
        HRN_CFG_LOAD(cfgCmdAnnotate, argListAnnotation);

        TEST_ERROR(cmdAnnotate(), BackupSetInvalidError, "no backup set to annotate found");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
