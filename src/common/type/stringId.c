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
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGID, strId);
    FUNCTION_TEST_END();

    // One byte encodes one char so the buffer need be no larger than the size of an unsigned 64-bit integer plus the terminator
    char buffer[sizeof(uint64_t) + 1];

    // Use the log function to get the zero-terminated string
    strIdToLog(strId, buffer, sizeof(buffer));

    FUNCTION_TEST_RETURN(strNew(buffer));
}

/**********************************************************************************************************************************/
StringId strIdFromStr(const String *const str)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, str);
    FUNCTION_TEST_END();

    ASSERT(str != NULL);
    ASSERT(strSize(str) > 0);

    const char *const ptr = strZ(str);

    switch (strSize(str))
    {
        case 1:
            FUNCTION_TEST_RETURN((uint64_t)ptr[0]);

        case 2:
            FUNCTION_TEST_RETURN((uint64_t)ptr[0] | (uint64_t)ptr[1] << 8);

        case 3:
            FUNCTION_TEST_RETURN((uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16);

        case 4:
            FUNCTION_TEST_RETURN((uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16 | (uint64_t)ptr[3] << 24);

        case 5:
            FUNCTION_TEST_RETURN(
                (uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16 | (uint64_t)ptr[3] << 24 |
                (uint64_t)ptr[4] << 32);

        case 6:
            FUNCTION_TEST_RETURN(
                (uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16 | (uint64_t)ptr[3] << 24 |
                (uint64_t)ptr[4] << 32 | (uint64_t)ptr[5] << 40);

        case 7:
            FUNCTION_TEST_RETURN(
                (uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16 | (uint64_t)ptr[3] << 24 |
                (uint64_t)ptr[4] << 32 | (uint64_t)ptr[5] << 40 | (uint64_t)ptr[6] << 48);

        default:
            FUNCTION_TEST_RETURN(
                (uint64_t)ptr[0] | (uint64_t)ptr[1] << 8 | (uint64_t)ptr[2] << 16 | (uint64_t)ptr[3] << 24 |
                (uint64_t)ptr[4] << 32 | (uint64_t)ptr[5] << 40 | (uint64_t)ptr[6] << 48 | (uint64_t)ptr[7] << 56);
    }
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
