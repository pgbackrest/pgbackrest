/***********************************************************************************************************************************
Test Support Command
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

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
            setenv(PGBACKREST_ENV "NO_STANZA", "bogus", true);
            setenv(PGBACKREST_ENV "RESET_COMPRESS_TYPE", "bogus", true);

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
                            "\"PGBACKREST_NO_STANZA\":{"
                                "\"err\":{"
                                    "\"msg\":\"option 'no-stanza' cannot be negated\","
                                    "\"type\":\"OptionInvalidError\""
                                "},"
                                "\"val\":\"bogus\""
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
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("!!!");
        {
            TEST_RESULT_VOID(cmdSupport(), "!!!");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
