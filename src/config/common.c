/***********************************************************************************************************************************
Configuration Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/regExp.h"
#include "common/time.h"
#include "config/common.h"

/**********************************************************************************************************************************/
// Helper to get the multiplier based on the qualifier
static int64_t
cfgParseSizeQualifier(const char qualifier)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHAR, qualifier);
    FUNCTION_TEST_END();

    int64_t result;

    switch (qualifier)
    {
        case 'b':
            result = 1;
            break;

        case 'k':
            result = 1024;
            break;

        case 'm':
            result = 1024 * 1024;
            break;

        case 'g':
            result = 1024 * 1024 * 1024;
            break;

        case 't':
            result = 1024LL * 1024LL * 1024LL * 1024LL;
            break;

        case 'p':
            result = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
            break;

        default:
            THROW_FMT(AssertError, "'%c' is not a valid size qualifier", qualifier);
    }

    FUNCTION_TEST_RETURN(INT64, result);
}

int64_t
cfgParseSize(const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    // Lowercase the value
    String *valueLower = strLower(strDup(value));

    // Match the value against possible values
    if (regExpMatchOne(STRDEF("^[0-9]+(kib|kb|k|mib|mb|m|gib|gb|g|tib|tb|t|pib|pb|p|b)*$"), valueLower))
    {
        // Get the character array and size
        const char *strArray = strZ(valueLower);
        size_t size = strSize(valueLower);
        int chrPos = -1;

        // If there is a 'b' on the end, then see if the previous character is a number
        if (strArray[size - 1] == 'b')
        {
            // If the previous character is a number, then the letter to look at is 'b' which is the last position else it is in the
            // next to last position (e.g. kb - so the 'k' is the position of interest). Only need to test for <= 9 since the regex
            // enforces the format. Also allow an 'i' before the 'b'.
            if (strArray[size - 2] <= '9')
                chrPos = (int)(size - 1);
            else if (strArray[size - 2] == 'i')
                chrPos = (int)(size - 3);
            else
                chrPos = (int)(size - 2);
        }
        // Else if there is no 'b' at the end but the last position is not a number then it must be one of the letters, e.g. 'k'
        else if (strArray[size - 1] > '9')
            chrPos = (int)(size - 1);

        int64_t multiplier = 1;

        // If a letter was found calculate multiplier, else do nothing since assumed value is already in bytes
        if (chrPos != -1)
        {
            multiplier = cfgParseSizeQualifier(strArray[chrPos]);

            // Remove any letters
            strTruncIdx(valueLower, chrPos);
        }

        // Convert string to bytes
        const int64_t valueInt = cvtZToInt64(strZ(valueLower));

        if (valueInt > INT64_MAX / multiplier)
            THROW_FMT(FormatError, "value '%s' is out of range", strZ(value));

        strFree(valueLower);

        FUNCTION_TEST_RETURN(INT64, valueInt * multiplier);
    }

    THROW_FMT(FormatError, "value '%s' is not valid", strZ(value));
}

/**********************************************************************************************************************************/
int64_t
cfgParseTime(const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT64, (int64_t)(cvtZToDouble(strZ(value)) * MSEC_PER_SEC));
}
