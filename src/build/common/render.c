/***********************************************************************************************************************************
Build Common
***********************************************************************************************************************************/
#include "common/type/stringId.h"

#include "build/common/render.h"

/**********************************************************************************************************************************/
String *
bldStrId(const char *const buffer)
{
    StringId result = 0;

    TRY_BEGIN()
    {
        result = strIdFromZ(stringIdBit5, buffer);
    }
    CATCH_ANY()
    {
        result = strIdFromZ(stringIdBit6, buffer);
    }
    TRY_END();

    return strNewFmt("STRID%u(\"%s\", 0x%" PRIx64 ")", (unsigned int)(result & STRING_ID_BIT_MASK) + 5, buffer, result);
}
