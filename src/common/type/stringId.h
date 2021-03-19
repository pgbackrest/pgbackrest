/***********************************************************************************************************************************
Represent Short Strings as Integers

!!!
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGID_H
#define COMMON_TYPE_STRINGID_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Maximum number of characters in a StringId. This is a safe buffer size when calling strIdToZN. If the buffer needs to be
zero-terminated then an extra byte will be needed.
***********************************************************************************************************************************/
#define STRING_ID_MAX                                               8

/***********************************************************************************************************************************
!!! AND EXPLAIN WHY THIS IS A TYPEDEF SINCE WE DO NOT NORMALLY TYPEDEF STANDARD TYPES
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
#define STRID_6BIT                                                  1

#define STRID_LOWER(c)                                              (((c & 0xE0) == 0x60) * (c - 97))
#define STRID_DASH(c)                                               ((c == 0x2D) * (c - 19))
#define STRID_NUMBER(c)                                             (((c & 0xF0) == 0x30) * (c - 21))
#define STRID_UPPER(c)                                              (((c & 0xE0) == 0x40 && c != 0x5F) * (c - 28))
#define STRID_USCORE(c)                                             ((c == 0x5F) * (c - 32))

#define STRIDC(c)                                                                                                                  \
    (STRID_LOWER(c) | STRID_DASH(c) | STRID_USCORE(c) | STRID_NUMBER(c) | STRID_UPPER(c))

#define STR6ID1(c1)                                                 ((uint16_t)STRIDC(c1) << 4 | STRID_6BIT)
#define STR6ID2(c1, c2)                                             (STR6ID1(c1) | (uint16_t)STRIDC(c2) << 10)
#define STR6ID3(c1, c2, c3)                                         ((uint32_t)STR6ID2(c1, c2) | (uint32_t)STRIDC(c3) << 16)
#define STR6ID4(c1, c2, c3, c4)                                     (STR6ID3(c1, c2, c3) | (uint32_t)STRIDC(c4) << 22)
#define STR6ID5(c1, c2, c3, c4, c5)                                 ((uint64_t)STR6ID4(c1, c2, c3, c4) | (uint64_t)STRIDC(c5) << 28)
#define STR6ID6(c1, c2, c3, c4, c5, c6)                             (STR6ID5(c1, c2, c3, c4, c5) | (uint64_t)STRIDC(c6) << 34)
#define STR6ID7(c1, c2, c3, c4, c5, c6, c7)                         (STR6ID6(c1, c2, c3, c4, c5, c6) | (uint64_t)STRIDC(c7) << 40)
#define STR6ID8(c1, c2, c3, c4, c5, c6, c7, c8)                                                                                    \
    (STR6ID7(c1, c2, c3, c4, c5, c6, c7) | (uint64_t)STRIDC(c8) << 46)
#define STR6ID9(c1, c2, c3, c4, c5, c6, c7, c8, c9)                                                                                \
    (STR6ID8(c1, c2, c3, c4, c5, c6, c7, c8) | (uint64_t)STRIDC(c9) << 52)
#define STR6ID10(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                                                                          \
    (STR6ID9(c1, c2, c3, c4, c5, c6, c7, c8, c9) | (uint64_t)STRIDC(c10) << 58)

#define STRID1(c1)                                                  (uint8_t)c1
#define STRID2(c1, c2)                                              ((uint16_t)c1 | (uint16_t)c2 << 8)
#define STRID3(c1, c2, c3)                                          ((uint32_t)c1 | (uint32_t)c2 << 8 | (uint32_t)c3 << 16)
#define STRID4(c1, c2, c3, c4)                                                                                                     \
    ((uint32_t)c1 | (uint32_t)c2 << 8 | (uint32_t)c3 << 16 | (uint32_t)c4 << 24)
#define STRID5(c1, c2, c3, c4, c5)                                                                                                 \
    ((uint64_t)c1 | (uint64_t)c2 << 8 | (uint64_t)c3 << 16 | (uint64_t)c4 << 24 | (uint64_t)c5 << 32)
#define STRID6(c1, c2, c3, c4, c5, c6)                                                                                             \
    ((uint64_t)c1 | (uint64_t)c2 << 8 | (uint64_t)c3 << 16 | (uint64_t)c4 << 24 | (uint64_t)c5 << 32 | (uint64_t)c6 << 40)
#define STRID7(c1, c2, c3, c4, c5, c6, c7)                                                                                         \
    ((uint64_t)c1 | (uint64_t)c2 << 8 | (uint64_t)c3 << 16 | (uint64_t)c4 << 24 | (uint64_t)c5 << 32 | (uint64_t)c6 << 40 |        \
        (uint64_t)c7 << 48)
#define STRID8(c1, c2, c3, c4, c5, c6, c7, c8)                                                                                     \
    ((uint64_t)c1 | (uint64_t)c2 << 8 | (uint64_t)c3 << 16 | (uint64_t)c4 << 24 | (uint64_t)c5 << 32 | (uint64_t)c6 << 40 |        \
        (uint64_t)c7 << 48 | (uint64_t)c8 << 56)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to String
String *strIdToStr(const StringId strId);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
size_t strIdToZ(const StringId strId, char *const buffer);

// Convert N chars to StringId
StringId strIdFromZN(const char *const buffer, const size_t size);

// Convert String to StringId
__attribute__((always_inline)) static inline StringId
strIdFromStr(const String *const str)
{
    return strIdFromZN(strZ(str), strSize(str));
}

// Convert zero-terminted string to StringId
__attribute__((always_inline)) static inline StringId
strIdFromZ(const char *const str)
{
    return strIdFromZN(str, strlen(str));
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
