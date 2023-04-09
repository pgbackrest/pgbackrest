/***********************************************************************************************************************************
Test Support Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Test storage
    const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // Common config
    HRN_STORAGE_PUT_Z(
        storageTest, "pgbackrest.conf",
        "[global]\n"
        "repo1-path=" TEST_PATH "/repo1\n"
        "repo1-block=y\n"
        "\n"
        "[global:backup]\n"
        "start-fast=y\n"
        "\n"
        "[test2]\n"
        "pg1-path=" TEST_PATH "/test2-pg1\n"
        "recovery-option=key1=value1\n"
        "recovery-option=key2=value2\n"
        "\n"
        "[test1]\n"
        "pg1-path=" TEST_PATH "/test1-pg1\n");

    StringList *const argListCommon = strLstNew();
    hrnCfgArgRawZ(argListCommon, cfgOptConfig, TEST_PATH "/pgbackrest.conf");

    // *****************************************************************************************************************************
    if (testBegin("cmdSupport()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no env, no config");
        {
            TEST_RESULT_STR_Z(
                cmdSupportRender(strLstSize(argListCommon), strLstPtr(argListCommon)),
                // {uncrustify_off - indentation}
                "{"
                    "\"cfg\":{"
                        "\"env\":{},"
                        "\"file\":null"
                    "},"
                    "\"stanza\":{"
                    "}"
                "}",
                // {uncrustify_on}
                "render");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("env and config");
        {
            hrnCfgEnvRawZ(cfgOptBufferSize, "64KiB");
            setenv(PGBACKREST_ENV "BOGUS", "bogus", true);
            setenv(PGBACKREST_ENV "NO_ONLINE", "bogus", true);
            setenv(PGBACKREST_ENV "DB_INCLUDE", "db1:db2", true);
            setenv(PGBACKREST_ENV "RESET_COMPRESS_TYPE", "bogus", true);

            const StringList *argListFull = hrnCfgLoad(cfgCmdSupport, argListCommon, (HrnCfgLoadParam){.log = true});

            TEST_RESULT_STR_Z(
                cmdSupportRender(strLstSize(argListFull), strLstPtr(argListFull)),
                // {uncrustify_off - indentation}
                "{"
                    "\"cfg\":{"
                        "\"env\":{"
                            "\"PGBACKREST_BOGUS\":{"
                                "\"warn\":\"invalid option\""
                            "},"
                            "\"PGBACKREST_BUFFER_SIZE\":{"
                                "\"val\":\"64KiB\""
                            "},"
                            "\"PGBACKREST_DB_INCLUDE\":{"
                                "\"val\":["
                                    "\"db1\","
                                    "\"db2\""
                                "]"
                            "},"
                            "\"PGBACKREST_NO_ONLINE\":{"
                                "\"val\":\"bogus\","
                                "\"warn\":\"invalid negate option\""
                            "},"
                            "\"PGBACKREST_RESET_COMPRESS_TYPE\":{"
                                "\"val\":\"bogus\","
                                "\"warn\":\"invalid reset option\""
                            "}"
                        "},"
                        "\"file\":{"
                            "\"global\":{"
                                "\"repo1-block\":{"
                                    "\"val\":\"y\""
                                "},"
                                "\"repo1-path\":{"
                                    "\"val\":\"" TEST_PATH "/repo1\""
                                "}"
                            "},"
                            "\"global:backup\":{"
                                "\"start-fast\":{"
                                    "\"val\":\"y\""
                                "}"
                            "},"
                            "\"test1\":{"
                                "\"pg1-path\":{"
                                    "\"val\":\"" TEST_PATH "/test1-pg1\""
                                "}"
                            "},"
                            "\"test2\":{"
                                "\"pg1-path\":{"
                                    "\"val\":\"" TEST_PATH "/test2-pg1\""
                                "},"
                                "\"recovery-option\":{"
                                    "\"val\":["
                                        "\"key1=value1\","
                                        "\"key2=value2\""
                                    "]"
                                "}"
                            "}"
                        "}"
                    "},"
                    "\"stanza\":{"
                        "\"test1\":{"
                        "},"
                        "\"test2\":{"
                        "}"
                    "}"
                "}",
                // {uncrustify_on}
                "render");

            TEST_RESULT_LOG(
                "P00   WARN: environment contains invalid option 'bogus'\n"
                "P00   WARN: environment contains invalid negate option 'no-online'\n"
                "P00   WARN: environment contains invalid reset option 'reset-compress-type'");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("!!!");
        {
            TEST_RESULT_VOID(cmdSupport(strLstSize(argListCommon), strLstPtr(argListCommon)), "!!!");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
