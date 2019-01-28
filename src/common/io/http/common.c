/***********************************************************************************************************************************
Http Common
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/http/common.h"

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
