/***********************************************************************************************************************************
Represent Short Strings as Integers
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/assert.h"
#include "common/debug.h"
#include "common/type/stringId.h"

/**********************************************************************************************************************************/
StringId strIdFromZN(const char *const buffer, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(size > 0);

    switch (size)
    {
        case 1:
            FUNCTION_TEST_RETURN((uint64_t)buffer[0]);

        case 2:
            FUNCTION_TEST_RETURN((uint64_t)buffer[0] | (uint64_t)buffer[1] << 8);

        case 3:
            FUNCTION_TEST_RETURN((uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16);

        case 4:
            FUNCTION_TEST_RETURN(
                (uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16 | (uint64_t)buffer[3] << 24);

        case 5:
            FUNCTION_TEST_RETURN(
                (uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16 | (uint64_t)buffer[3] << 24 |
                (uint64_t)buffer[4] << 32);

        case 6:
            FUNCTION_TEST_RETURN(
                (uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16 | (uint64_t)buffer[3] << 24 |
                (uint64_t)buffer[4] << 32 | (uint64_t)buffer[5] << 40);

        case 7:
            FUNCTION_TEST_RETURN(
                (uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16 | (uint64_t)buffer[3] << 24 |
                (uint64_t)buffer[4] << 32 | (uint64_t)buffer[5] << 40 | (uint64_t)buffer[6] << 48);

        default:
            FUNCTION_TEST_RETURN(
                (uint64_t)buffer[0] | (uint64_t)buffer[1] << 8 | (uint64_t)buffer[2] << 16 | (uint64_t)buffer[3] << 24 |
                (uint64_t)buffer[4] << 32 | (uint64_t)buffer[5] << 40 | (uint64_t)buffer[6] << 48 | (uint64_t)buffer[7] << 56);
    }
}

/**********************************************************************************************************************************/
#define STR_ID_TO_ZN_IDX(idx)                                                                                                      \
    buffer[idx] = (char)(strId & 0xFF);                                                                                            \
    strId >>= 8;                                                                                                                   \
                                                                                                                                   \
    if (strId == 0)                                                                                                                \
        FUNCTION_TEST_RETURN(idx + 1)

size_t
strIdToZN(StringId strId, char *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, strId);
        FUNCTION_TEST_PARAM(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(strId != 0);
    ASSERT(buffer != NULL);

    // Char 1-7
    STR_ID_TO_ZN_IDX(0);
    STR_ID_TO_ZN_IDX(1);
    STR_ID_TO_ZN_IDX(2);
    STR_ID_TO_ZN_IDX(3);
    STR_ID_TO_ZN_IDX(4);
    STR_ID_TO_ZN_IDX(5);
    STR_ID_TO_ZN_IDX(6);

    // Char 8
    buffer[7] = (char)(strId & 0xFF);
    ASSERT(strId >> 8 == 0);

    FUNCTION_TEST_RETURN(8);
}

/**********************************************************************************************************************************/
String *
strIdToStr(const StringId strId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, strId);
    FUNCTION_TEST_END();

    char buffer[STRING_ID_MAX + 1];
    buffer[strIdToZN(strId, buffer)] = '\0';

    FUNCTION_TEST_RETURN(strNew(buffer));
}

/**********************************************************************************************************************************/
size_t
strIdToZ(const StringId strId, char *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, strId);
        FUNCTION_TEST_PARAM(VOID, buffer);
    FUNCTION_TEST_END();

    size_t size = strIdToZN(strId, buffer);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(size);
}

/**********************************************************************************************************************************/
size_t
strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize)
{
    ASSERT(bufferSize > STRING_ID_MAX);
    (void)bufferSize;

    return strIdToZ(strId, buffer);
}
