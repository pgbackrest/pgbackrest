/***********************************************************************************************************************************
HTTP Common
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
static const char *const httpCommonMonthList[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *const httpCommonDayList[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

FN_EXTERN time_t
httpDateToTime(const String *const lastModified)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, lastModified);
    FUNCTION_TEST_END();

    // Find the month
    const char *const month = strZ(lastModified) + 8;
    unsigned int monthIdx = 0;

    for (; monthIdx < LENGTH_OF(httpCommonMonthList); monthIdx++)
    {
        if (strncmp(month, httpCommonMonthList[monthIdx], 3) == 0)
            break;
    }

    if (monthIdx == LENGTH_OF(httpCommonMonthList))
        THROW_FMT(FormatError, "invalid month '%.3s'", month);

    // Convert to time_t
    FUNCTION_TEST_RETURN(
        TIME,
        epochFromParts(
            cvtZSubNToInt(strZ(lastModified), 12, 4), (int)monthIdx + 1, cvtZSubNToInt(strZ(lastModified), 5, 2),
            cvtZSubNToInt(strZ(lastModified), 17, 2), cvtZSubNToInt(strZ(lastModified), 20, 2),
            cvtZSubNToInt(strZ(lastModified), 23, 2), 0));
}

FN_EXTERN String *
httpDateFromTime(const time_t time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME, time);
    FUNCTION_TEST_END();

    struct tm timePart;
    gmtime_r(&time, &timePart);

    FUNCTION_TEST_RETURN(
        STRING,
        strNewFmt(
            "%s, %02d %s %04d %02d:%02d:%02d GMT", httpCommonDayList[timePart.tm_wday], timePart.tm_mday,
            httpCommonMonthList[timePart.tm_mon], timePart.tm_year + 1900, timePart.tm_hour, timePart.tm_min,
            timePart.tm_sec));
}

/**********************************************************************************************************************************/
FN_EXTERN String *
httpUriDecode(const String *const uri)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, uri);
    FUNCTION_TEST_END();

    String *result = NULL;

    // Decode if the string is not null
    if (uri != NULL)
    {
        result = strNew();

        // Iterate all characters in the string
        for (unsigned uriIdx = 0; uriIdx < strSize(uri); uriIdx++)
        {
            char uriChar = strZ(uri)[uriIdx];

            // Convert escaped characters
            if (uriChar == '%')
            {
                // Sequence must be exactly three characters (% and two hex digits)
                if (strSize(uri) - uriIdx < 3)
                    THROW_FMT(FormatError, "invalid escape sequence length in '%s'", strZ(uri));

                // Convert hex digits
                uriChar = (char)cvtZSubNToUIntBase(strZ(uri), uriIdx + 1, 2, 16);

                // Skip to next character or escape
                uriIdx += 2;
            }

            strCatChr(result, uriChar);
        }
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
httpUriEncode(const String *const uri, const bool path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, uri);
        FUNCTION_TEST_PARAM(BOOL, path);
    FUNCTION_TEST_END();

    String *result = NULL;

    // Encode if the string is not null
    if (uri != NULL)
    {
        result = strNew();

        // Iterate all characters in the string
        for (unsigned uriIdx = 0; uriIdx < strSize(uri); uriIdx++)
        {
            const char uriChar = strZ(uri)[uriIdx];

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

    FUNCTION_TEST_RETURN(STRING, result);
}
