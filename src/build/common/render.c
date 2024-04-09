/***********************************************************************************************************************************
Build Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>

#include "build/common/render.h"
#include "common/type/stringId.h"

/**********************************************************************************************************************************/
String *
bldStrId(const char *const buffer)
{
    StringId result = strIdFromZ(buffer);

    return strNewFmt("STRID%u(\"%s\", 0x%" PRIx64 ")", (unsigned int)(result & STRING_ID_BIT_MASK) + 5, buffer, result);
}

/**********************************************************************************************************************************/
String *
bldEnum(const char *const prefix, const String *const value)
{
    String *const result = strNew();
    const char *const valuePtr = strZ(value);
    bool upper = false;

    // Add prefix and set first non-prefix letter to upper
    if (prefix != NULL)
    {
        strCatZ(result, prefix);
        upper = true;
    }

    // Add remaining letters removing dashes and upper-casing the letter after the dash
    for (unsigned int valueIdx = 0; valueIdx < strSize(value); valueIdx++)
    {
        strCatChr(result, upper ? (char)toupper(valuePtr[valueIdx]) : valuePtr[valueIdx]);
        upper = false;

        if (valuePtr[valueIdx + 1] == '-')
        {
            upper = true;
            valueIdx++;
        }
    }

    return result;
}
