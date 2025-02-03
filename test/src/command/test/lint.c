/***********************************************************************************************************************************
Code Linter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/regExp.h"
#include "build/common/render.h"
#include "command/test/lint.h"
#include "common/log.h"
#include "storage/posix/storage.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Validate StringId values
***********************************************************************************************************************************/
static unsigned int
lintStrId(const String *const source)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, source);
    FUNCTION_LOG_END();

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check all StringIds
        RegExp *const strIdExp = regExpNew(STRDEF("STRID(5|6)\\([^)]+\\)"));
        const char *sourcePtr = strZ(source);

        while (regExpMatch(strIdExp, STRDEF(sourcePtr)))
        {
            const String *const match = regExpMatchStr(strIdExp, STRDEF(sourcePtr));
            const StringList *const paramList = strLstNewSplitZ(
                strLstGet(strLstNewSplitZ(strSubN(match, 0, strSize(match) - 1), "("), 1), ",");
            const String *const param = strTrim(strLstGet(paramList, 0));

            sourcePtr = regExpMatchPtr(strIdExp, STRDEF(sourcePtr)) + strSize(match);

            // Skip macro definitions
            if (strEqZ(match, "STRID5(str, strId)") || strEqZ(match, "STRID6(str, strId)"))
                continue;

            // Skip test strings
            if (strBeginsWithZ(param, "\\\"") && strEndsWithZ(param, "\\\""))
                continue;

            // Skip test values
            if (strLstSize(paramList) > 1 && strBeginsWithZ(strTrim(strLstGet(paramList, 1)), "TEST_"))
                continue;

            // Param must begin and end with a quote
            if (strZ(param)[0] != '"' || strZ(param)[strSize(param) - 1] != '"')
            {
                LOG_WARN_FMT("'%s' must have quotes around string parameter '%s'", strZ(match), strZ(param));
                result++;

                continue;
            }

            // Check validity of the string
            const String *const expected = bldStrId(strZ(strSubN(param, 1, strSize(param) - 2)));

            if (!strEq(match, expected))
            {
                LOG_WARN_FMT("'%s' should be '%s'", strZ(match), strZ(expected));
                result++;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
void
lintAll(const String *const pathRepo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
    FUNCTION_LOG_END();

    ASSERT(pathRepo != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *const storageRepo = storagePosixNewP(pathRepo);
        StorageIterator *const repoItr = storageNewItrP(
            storageRepo, NULL, .level = storageInfoLevelType, .recurse = true, .expression = STRDEF("\\.(c|h)$"));

        while (storageItrMore(repoItr))
        {
            const StorageInfo info = storageItrNext(repoItr);
            const String *const source = strNewBuf(storageGetP(storageNewReadP(storageRepo, info.name)));

            unsigned int errorTotal = lintStrId(source);

            if (errorTotal > 0)
                THROW_FMT(FormatError, "%u linter error(s) in '%s' (see warnings above)", errorTotal, strZ(info.name));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
