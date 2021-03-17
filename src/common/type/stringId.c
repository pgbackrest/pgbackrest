/***********************************************************************************************************************************
Represent Short Strings as Integers
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/assert.h"
#include "common/type/stringId.h"

/**********************************************************************************************************************************/
String *
strIdToStr(const StringId strId)
{
    // !!! EXPLAIN THIS BUFFER SIZE
    char buffer[sizeof(uint64_t) + 1];

    strIdToLog(strId, buffer, sizeof(buffer));

    return strNew(buffer);
}

/**********************************************************************************************************************************/
size_t
strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize)
{
    ASSERT(bufferSize > 0);

    unsigned int strIdx = 0;

    for (; strIdx < bufferSize - 1; strIdx++)
    {
        char next = (char)((strId >> (strIdx * 8)) & 0xFF);

        if (!next)
            break;

        buffer[strIdx] = next;
    }

    buffer[strIdx] = '\0';

    return strIdx;
}
