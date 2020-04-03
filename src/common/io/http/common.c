/***********************************************************************************************************************************
Http Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/time.h"

/***********************************************************************************************************************************
Convert the time using the format specified in https://tools.ietf.org/html/rfc7231#section-7.1.1.1 which is used by HTTP 1.1 (the
only version we support).
***********************************************************************************************************************************/
time_t
httpLastModifiedToTime(const String *lastModified)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, lastModified);
    FUNCTION_TEST_END();

    time_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Find the month
        static const char *monthList[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        const char *month = strPtr(strSubN(lastModified, 8, 3));
        unsigned int monthIdx = 0;

        for (; monthIdx < sizeof(monthList) / sizeof(char *); monthIdx++)
        {
            if (strcmp(month, monthList[monthIdx]) == 0)
                break;
        }

        if (monthIdx == sizeof(monthList) / sizeof(char *))
            THROW_FMT(FormatError, "invalid month '%s'", month);

        // Convert to time_t
        result = epochFromParts(
            cvtZToInt(strPtr(strSubN(lastModified, 12, 4))), (int)monthIdx + 1, cvtZToInt(strPtr(strSubN(lastModified, 5, 2))),
            cvtZToInt(strPtr(strSubN(lastModified, 17, 2))), cvtZToInt(strPtr(strSubN(lastModified, 20, 2))),
            cvtZToInt(strPtr(strSubN(lastModified, 23, 2))), 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
httpUriEncode(const String *uri, bool path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, uri);
        FUNCTION_TEST_PARAM(BOOL, path);
    FUNCTION_TEST_END();

    String *result = NULL;

    // Encode if the string is not null
    if (uri != NULL)
    {
        result = strNew("");

        // Iterate all characters in the string
        for (unsigned uriIdx = 0; uriIdx < strSize(uri); uriIdx++)
        {
            char uriChar = strPtr(uri)[uriIdx];

            // These characters are reproduced verbatim
            if ((uriChar >= 'A' && uriChar <= 'Z') || (uriChar >= 'a' && uriChar <= 'z') || (uriChar >= '0' && uriChar <= '9') ||
                uriChar == '_' || uriChar == '-' || uriChar == '~' || uriChar == '.' || (path && uriChar == '/'))
            {
                strCatChr(result, uriChar);
            }
            // All other characters are hex-encoded
            else
                strCatFmt(result, "%%%02X", (unsigned char)uriChar);
        }
    }

    FUNCTION_TEST_RETURN(result);
}
