/***********************************************************************************************************************************
Build Common
***********************************************************************************************************************************/
#include "common/type/stringId.h"

#include "build/common/render.h"

/**********************************************************************************************************************************/
String *
bldStrId(const char *const buffer)
{
    StringId result = strIdFromZ(buffer);

    return strNewFmt("STRID%u(\"%s\", 0x%" PRIx64 ")", (unsigned int)(result & STRING_ID_BIT_MASK) + 5, buffer, result);
}
