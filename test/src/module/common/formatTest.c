/***********************************************************************************************************************************
Test Repository Format
***********************************************************************************************************************************/
#include "version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("repoFormatValidate()"))
    {
        TEST_ERROR(
            repoFormatValidate(REPOSITORY_FORMAT_MIN - 1), FormatError,
            "repository format 4 is no longer supported by " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");
        TEST_ERROR(
            repoFormatValidate(REPOSITORY_FORMAT_MAX + 1), FormatError,
            "repository format 7 requires a newer version of " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");

        TEST_RESULT_VOID(repoFormatValidate(REPOSITORY_FORMAT_5), "format 5 is readable");
        TEST_RESULT_VOID(repoFormatValidate(REPOSITORY_FORMAT_6), "format 6 is readable");
    }

    // *****************************************************************************************************************************
    if (testBegin("repoFormatDigest()"))
    {
        TEST_RESULT_UINT(repoFormatDigest(REPOSITORY_FORMAT_5), hashTypeSha1, "format 5 derives with sha1");
        TEST_RESULT_UINT(repoFormatDigest(REPOSITORY_FORMAT_6), hashTypeSha256, "format 6 derives with sha256");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
