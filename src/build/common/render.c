/***********************************************************************************************************************************
Build Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/render.h"
#include "common/type/stringId.h"

/**********************************************************************************************************************************/
String *
bldStrId(const char *const buffer)
{
    StringId result = strIdFromZ(buffer);

    return strNewFmt("STRID%u(\"%s\", 0x%" PRIx64 ")", (unsigned int)(result & STRING_ID_BIT_MASK) + 5, buffer, result);
}
