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
        "repo2-block=y\n"
        "\n"
        "[test1]\n"
        "pg1-path=" TEST_PATH "/test1-pg1\n"
        "\n"
        "[test2]\n"
        "pg1-path=" TEST_PATH "/test2-pg1\n");

    StringList *const argListCommon = strLstNew();
    hrnCfgArgRawZ(argListCommon, cfgOptConfig, TEST_PATH "/pgbackrest.conf");

    // *****************************************************************************************************************************
    if (testBegin("cmdSupport()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no env, no config");
        {
            TEST_RESULT_STR_Z(
                cmdSupportRender(),
                "{"
                    "\"cfg\":{"
                        "\"env\":{},"
                        "\"file\":null"
                    "}"
                "}",
                "render");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("env and config");
        {
            hrnCfgEnvRawZ(cfgOptBufferSize, "64KiB");
            setenv(PGBACKREST_ENV "BOGUS", "bogus", true);
            setenv(PGBACKREST_ENV "NO_ONLINE", "bogus", true);
            // setenv(PGBACKREST_ENV "NO_STANZA", "bogus", true);
            setenv(PGBACKREST_ENV "RESET_COMPRESS_TYPE", "bogus", true);

            StringList *argList = strLstDup(argListCommon);
            HRN_CFG_LOAD(cfgCmdSupport, argList);

            TEST_RESULT_STR_Z(
                cmdSupportRender(),
                "{"
                    "\"cfg\":{"
                        "\"env\":{"
                            "\"PGBACKREST_BOGUS\":{"
                                "\"val\":\"bogus\","
                                "\"warn\":\"invalid option\""
                            "},"
                            "\"PGBACKREST_BUFFER_SIZE\":{"
                                "\"val\":\"64KiB\""
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
                        "\"file\":null"
                    "}"
                "}",
                "render");

            TEST_RESULT_LOG(
                "P00   WARN: environment contains invalid option 'bogus'\n"
                "P00   WARN: environment contains invalid negate option 'no-online'\n"
                "P00   WARN: environment contains invalid reset option 'reset-compress-type'");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("!!!");
        {
            TEST_RESULT_VOID(cmdSupport(), "!!!");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
