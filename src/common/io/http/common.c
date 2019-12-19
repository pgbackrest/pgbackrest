/***********************************************************************************************************************************
Http Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/debug.h"
#include "common/io/http/common.h"

/**********************************************************************************************************************************/
time_t
httpCvtTime(const String *time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, time);
    FUNCTION_TEST_END();

    time_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Find the month
        static const char *monthList[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        const char *month = strPtr(strSubN(time, 8, 3));
        unsigned int monthIdx = 0;

        for (; monthIdx < sizeof(monthList) / sizeof(char *); monthIdx++)
        {
            if (strcmp(month, monthList[monthIdx]) == 0)
                break;
        }

        if (monthIdx == sizeof(monthList) / sizeof(char *))
            THROW_FMT(FormatError, "invalid month '%s'", month);

        // Convert to time_t
        struct tm timeStruct =
        {
            .tm_year = cvtZToInt(strPtr(strSubN(time, 12, 4))) - 1900,
            .tm_mon = (int)monthIdx,
            .tm_mday = cvtZToInt(strPtr(strSubN(time, 5, 2))),
            .tm_hour = cvtZToInt(strPtr(strSubN(time, 17, 2))),
            .tm_min = cvtZToInt(strPtr(strSubN(time, 20, 2))),
            .tm_sec = cvtZToInt(strPtr(strSubN(time, 23, 2))),
        };

        result = cvtTimeStructGmtToTime(&timeStruct);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Encode string to conform with URI specifications

If a path is being encoded then / characters won't be encoded.
***********************************************************************************************************************************/
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
