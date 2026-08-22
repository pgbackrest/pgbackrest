/***********************************************************************************************************************************
Repository Format
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/format.h"
#include "version.h"

/**********************************************************************************************************************************/
FN_EXTERN void
repoFormatValidate(const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    // A format newer than this version can read requires an upgrade. Do not suggest a version since this version cannot know which
    // version added the format.
    if (format > REPOSITORY_FORMAT_MAX)
    {
        THROW_FMT(
            FormatError,
            "repository format %u requires a newer version of " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format %d to %d.",
            format, REPOSITORY_FORMAT_MIN, REPOSITORY_FORMAT_MAX);
    }

    // A format older than this version can read requires an older version to migrate the repository
    if (format < REPOSITORY_FORMAT_MIN)
    {
        THROW_FMT(
            FormatError,
            "repository format %u is no longer supported by " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format %d to %d.",
            format, REPOSITORY_FORMAT_MIN, REPOSITORY_FORMAT_MAX);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN HashType
repoFormatDigest(const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(STRING_ID, format >= REPOSITORY_FORMAT_6 ? hashTypeSha256 : hashTypeSha1);
}
