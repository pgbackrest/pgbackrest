/***********************************************************************************************************************************
Represent Short Strings as Integers
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/assert.h"
#include "common/debug.h"
#include "common/type/stringId.h"

/**********************************************************************************************************************************/
StringId strIdFromZN(const StringIdBit bit, const char *const buffer, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, bit);
        FUNCTION_TEST_PARAM(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(size > 0);

    switch (bit)
    {
        case stringIdBit5:
        {
            static const uint8_t map[256] =
            {
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 27,  0,  0,
                 0, 28, 29, 30, 31,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
            };

            uint64_t result = stringIdBit5;

            switch (size)
            {
                default:
                {
                    if (size > 12)
                        result |= STRING_ID_PREFIX;

                    result |= (uint64_t)map[(uint8_t)buffer[11]] << 59;
                }

                case 11:
                    result |= (uint64_t)map[(uint8_t)buffer[10]] << 54;

                case 10:
                    result |= (uint64_t)map[(uint8_t)buffer[9]] << 49;

                case 9:
                    result |= (uint64_t)map[(uint8_t)buffer[8]] << 44;

                case 8:
                    result |= (uint64_t)map[(uint8_t)buffer[7]] << 39;

                case 7:
                    result |= (uint64_t)map[(uint8_t)buffer[6]] << 34;

                case 6:
                    result |= (uint64_t)map[(uint8_t)buffer[5]] << 29;

                case 5:
                    result |= (uint64_t)map[(uint8_t)buffer[4]] << 24;

                case 4:
                    result |= (uint64_t)map[(uint8_t)buffer[3]] << 19;

                case 3:
                    result |= (uint64_t)map[(uint8_t)buffer[2]] << 14;

                case 2:
                    result |= (uint64_t)map[(uint8_t)buffer[1]] << 9;

                case 1:
                    result |= (uint64_t)map[(uint8_t)buffer[0]] << 4;
            }

            FUNCTION_TEST_RETURN(result);
        }

        default:
        {
            ASSERT(bit == stringIdBit6);

            static const uint8_t map[256] =
            {
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 27,  0,  0,
                28, 29, 30, 31, 32, 33, 34, 35, 36, 37,  0,  0,  0,  0,  0,  0,
                 0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
                53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  0,  0,  0,  0,  0,
                 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
            };

            uint64_t result = stringIdBit6;

            switch (size)
            {
                default:
                {
                    if (size > 10)
                        result |= STRING_ID_PREFIX;

                    result |= (uint64_t)map[(uint8_t)buffer[9]] << 58;
                }

                case 9:
                    result |= (uint64_t)map[(uint8_t)buffer[8]] << 52;

                case 8:
                    result |= (uint64_t)map[(uint8_t)buffer[7]] << 46;

                case 7:
                    result |= (uint64_t)map[(uint8_t)buffer[6]] << 40;

                case 6:
                    result |= (uint64_t)map[(uint8_t)buffer[5]] << 34;

                case 5:
                    result |= (uint64_t)map[(uint8_t)buffer[4]] << 28;

                case 4:
                    result |= (uint64_t)map[(uint8_t)buffer[3]] << 22;

                case 3:
                    result |= (uint64_t)map[(uint8_t)buffer[2]] << 16;

                case 2:
                    result |= (uint64_t)map[(uint8_t)buffer[1]] << 10;

                case 1:
                    result |= (uint64_t)map[(uint8_t)buffer[0]] << 4;
            }

            FUNCTION_TEST_RETURN(result);
        }
    }
}

/**********************************************************************************************************************************/
size_t
strIdToZN(StringId strId, char *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, strId);
        FUNCTION_TEST_PARAM(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(strId != 0);
    ASSERT(buffer != NULL);

    bool prefix = strId & STRING_ID_PREFIX;
    StringIdBit bit = (StringIdBit)(strId & STRING_ID_BIT_MASK);
    strId >>= STRING_ID_HEADER_SIZE;

    switch (bit)
    {
        case stringIdBit5:
        {
            const char map[32] = "!abcdefghijklmnopqrstuvwxyz-1234";

            #define STR5ID_TO_ZN_IDX(idx)                                                                                          \
                buffer[idx] = map[strId & 0x1F];                                                                                   \
                strId >>= 5;                                                                                                       \
                                                                                                                                   \
                if (strId == 0)                                                                                                    \
                    FUNCTION_TEST_RETURN(idx + 1)

            // Char 1-11
            STR5ID_TO_ZN_IDX(0);
            STR5ID_TO_ZN_IDX(1);
            STR5ID_TO_ZN_IDX(2);
            STR5ID_TO_ZN_IDX(3);
            STR5ID_TO_ZN_IDX(4);
            STR5ID_TO_ZN_IDX(5);
            STR5ID_TO_ZN_IDX(6);
            STR5ID_TO_ZN_IDX(7);
            STR5ID_TO_ZN_IDX(8);
            STR5ID_TO_ZN_IDX(9);
            STR5ID_TO_ZN_IDX(10);

            // Char 12
            buffer[11] = map[strId & 0x1F];
            ASSERT(strId >> 5 == 0);

            if (prefix)
            {
                buffer[12] = '+';
                FUNCTION_TEST_RETURN(13);
            }

            FUNCTION_TEST_RETURN(12);
        }

        default:
        {
            CHECK(bit == stringIdBit6);

            const char map[64] = "!abcdefghijklmnopqrstuvwxyz-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

            #define STR6ID_TO_ZN_IDX(idx)                                                                                          \
                buffer[idx] = map[strId & 0x3F];                                                                                   \
                strId >>= 6;                                                                                                       \
                                                                                                                                   \
                if (strId == 0)                                                                                                    \
                    FUNCTION_TEST_RETURN(idx + 1)

            // Char 1-9
            STR6ID_TO_ZN_IDX(0);
            STR6ID_TO_ZN_IDX(1);
            STR6ID_TO_ZN_IDX(2);
            STR6ID_TO_ZN_IDX(3);
            STR6ID_TO_ZN_IDX(4);
            STR6ID_TO_ZN_IDX(5);
            STR6ID_TO_ZN_IDX(6);
            STR6ID_TO_ZN_IDX(7);
            STR6ID_TO_ZN_IDX(8);

            // Char 10
            buffer[9] = map[strId & 0x3F];
            ASSERT(strId >> 6 == 0);

            if (prefix)
            {
                buffer[10] = '+';
                FUNCTION_TEST_RETURN(11);
            }

            FUNCTION_TEST_RETURN(10);
        }
    }
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
