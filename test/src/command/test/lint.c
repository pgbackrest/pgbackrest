/***********************************************************************************************************************************
Code Linter
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

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
        RegExp *const strIdExp = regExpNew(STRDEF("STRID(5|6)(S){0,1}\\([^)]+\\)"));
        const char *sourcePtr = strZ(source);

        while (regExpMatch(strIdExp, STRDEF(sourcePtr)))
        {
            const String *const match = regExpMatchStr(strIdExp, STRDEF(sourcePtr));
            const StringList *const paramList = strLstNewSplitZ(
                strLstGet(strLstNewSplitZ(strSubN(match, 0, strSize(match) - 1), "("), 1), ",");
            const String *const param = strTrim(strLstGet(paramList, 0));

            sourcePtr = regExpMatchPtr(strIdExp, STRDEF(sourcePtr)) + strSize(match);

            // Skip macro definitions
            if (strEqZ(match, "STRID5(str, strId)") || strEqZ(match, "STRID5S(str, seq, strId)") ||
                strEqZ(match, "STRID6(str, strId)") || strEqZ(match, "STRID6S(str, seq, strId)"))
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
            const String *expected;

            if (strBeginsWithZ(match, "STRID5" "(") || strBeginsWithZ(match, "STRID6" "("))
                expected = bldStrId(strZ(strSubN(param, 1, strSize(param) - 2)));
            else
            {
                expected = bldStrIdSeq(
                    strZ(strSubN(param, 1, strSize(param) - 2)), cvtZToUInt(strZ(strTrim(strLstGet(paramList, 1)))));
            }

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

/***********************************************************************************************************************************
Detect characters that could be used to hide code, e.g. invisible, zero-width, bidirectional, or homoglyph characters. Source must
be 7-bit ASCII with tab and newline as the only permitted control characters. Intentional non-ASCII byte values must be written as
\xNN escapes so they remain visible in the source.
***********************************************************************************************************************************/
static unsigned int
lintChar(const String *const source)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, source);
    FUNCTION_LOG_END();

    unsigned int result = 0;

    const unsigned char *const data = (const unsigned char *)strZ(source);
    const size_t size = strSize(source);
    unsigned int line = 1;

    for (size_t idx = 0; idx < size; idx++)
    {
        const unsigned char chr = data[idx];

        // Count lines for reporting
        if (chr == '\n')
        {
            line++;
            continue;
        }

        // Tab is the only other permitted control character
        if (chr == '\t')
            continue;

        // Flag any byte outside printable 7-bit ASCII. Invisible, bidirectional, and homoglyph characters all live here, so they
        // are rejected wholesale and intentional non-ASCII must be written with \xNN escapes.
        if (chr < 0x20 || chr > 0x7E)
        {
            // Decode the code point for reporting on a best-effort basis. Validation is unnecessary since the character is rejected
            // regardless, so the byte count is taken from the lead byte and limited to the bytes available.
            unsigned int seqSize = 1;

            if (chr >= 0xF0)
                seqSize = 4;
            else if (chr >= 0xE0)
                seqSize = 3;
            else if (chr >= 0xC0)
                seqSize = 2;

            if (seqSize > size - idx)
                seqSize = (unsigned int)(size - idx);

            unsigned int codePoint = chr;

            if (seqSize > 1)
            {
                codePoint = chr & (0xFF >> (seqSize + 1));

                for (unsigned int seqIdx = 1; seqIdx < seqSize; seqIdx++)
                    codePoint = (codePoint << 6) | (data[idx + seqIdx] & 0x3F);
            }

            LOG_WARN_FMT("line %u contains disallowed character U+%04X (source must be 7-bit ASCII)", line, codePoint);
            result++;

            idx += seqSize - 1;
        }
    }

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Files that are exempt from the content checks. Binary files are unscannable places to hide content, so each entry must be a
deliberate, reviewable decision.
***********************************************************************************************************************************/
static const char *const lintFileSkipList[] =
{
    "doc/resource/logo.png",                                        // Project logo
    "test/data/filecopy.table.bin",                                 // Binary test fixture
    "doc/resource/git-history.cache",                               // Generated from git history, contains non-ASCII author names
};

static bool
lintFileSkip(const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    bool result = false;

    for (unsigned int skipIdx = 0; skipIdx < LENGTH_OF(lintFileSkipList); skipIdx++)
    {
        if (strEqZ(name, lintFileSkipList[skipIdx]))
        {
            result = true;
            break;
        }
    }

    FUNCTION_TEST_RETURN(BOOL, result);
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
            storageRepo, NULL, .level = storageInfoLevelType, .recurse = true, .sortOrder = sortOrderAsc);

        while (storageItrMore(repoItr))
        {
            const StorageInfo info = storageItrNext(repoItr);

            // Skip anything that is not a file and files that are exempt from the content checks
            if (info.type != storageTypeFile || lintFileSkip(info.name))
                continue;

            // Read the file, skipping any that cannot be read. Committed files are always readable after checkout, so this only
            // occurs from local filesystem state, and readability is enforced by the build rather than the linter.
            const Buffer *content = NULL;

            TRY_BEGIN()
            {
                content = storageGetP(storageNewReadP(storageRepo, info.name));
            }
            CATCH(FileOpenError)
            {
                // File cannot be read, so there is nothing to check
            }
            TRY_END();

            if (content == NULL)
                continue;

            unsigned int errorTotal = 0;

            // A binary file cannot be scanned for hidden content, so it is not allowed unless added to lintFileSkipList
            if (memchr(bufPtrConst(content), '\0', bufUsed(content)) != NULL)
            {
                LOG_WARN("unexpected binary file (add to the linter skip list if intentional)");
                errorTotal = 1;
            }
            // Otherwise the file must be 7-bit ASCII text, with the StringId check applied to C source
            else
            {
                const String *const source = strNewBuf(content);

                errorTotal = lintChar(source);

                // The StringId check applies to C source, including hand-written .c.inc, but not generated or vendored includes
                if (strEndsWithZ(info.name, ".c") || strEndsWithZ(info.name, ".h") ||
                    (strEndsWithZ(info.name, ".c.inc") && !strEndsWithZ(info.name, ".auto.c.inc") &&
                     !strEndsWithZ(info.name, ".vendor.c.inc")))
                {
                    errorTotal += lintStrId(source);
                }
            }

            if (errorTotal > 0)
                THROW_FMT(FormatError, "%u linter error(s) in '%s' (see warnings above)", errorTotal, strZ(info.name));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
