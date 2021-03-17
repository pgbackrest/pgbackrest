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
    // One byte encodes one char so the buffer need be no larger than the size of an unsigned 64-bit integer plus the terminator
    char buffer[sizeof(uint64_t) + 1];

    // Use the log function to get the zero-terminated string
    strIdToLog(strId, buffer, sizeof(buffer));

    return strNew(buffer);
}

/**********************************************************************************************************************************/
size_t
strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize)
{
    ASSERT(strId != 0);
    ASSERT(buffer != NULL);
    ASSERT(bufferSize > 0);

    // Decode each byte to a character
    unsigned int strIdx = 0;

    for (; strIdx < bufferSize - 1; strIdx++)
    {
        // Get next character
        char next = (char)((strId >> (strIdx * 8)) & 0xFF);

        // Break if there are no more characters
        if (!next)
            break;

        // Assign the character
        buffer[strIdx] = next;
    }

    // Zero terminate the string
    buffer[strIdx] = '\0';

    // Return length
    return strIdx;
}
