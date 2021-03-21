/***********************************************************************************************************************************
Represent Short Strings as Integers

It is often useful to represent indentifiers as strings when they cannot easily be represented as an enum/integer, e.g. because they
are distributed among a number of unrelated modules or need to be passed to remote processes. Strings are also more helpful in
debugging since they can be recognized without cross-referencing the source. However, strings are awkward to work with in code since
they cannot be directly used in switch statments leading to less efficient if-else structures.

A StringId encodes a short string into an integer so it can be used in switch statements but may also be readily converted back into
a string for debugging purposes. The provided STR*ID*() macros allow a StringId to be defined as characters to make it easier to
ensure that a StringId is unique within a certain namespace, e.g. all storage drivers.

The provided macros should be used when declaring a StringId. For example, STR5ID3('a', 'b', 'c') declares a three character
StringId with 5-bit encoding. STR6ID4('A', 'B', '1', '2') declares a four character string with 6-bit encoding. These macros reduce
to integers (uint16_t, uint32_t, and uint64_t depending on size) so they can be used directly in switch statements or #define'd or
generally used anywhere integers are valid.

Functions are also provided for converting back and forth to be used for debugging and runtime generation of StringIds.
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
#define STRING_ID_MAX                                               13

/***********************************************************************************************************************************
StringId typedef to make them more recognizable in the code
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
Number of bits to use for encoding. The number of bits affects the character set that can be used.
***********************************************************************************************************************************/
typedef enum
{
    stringIdBit5 = 0,                                               // 5-bit
    stringIdBit6 = 1,                                               // 6-bit
} StringIdBit;

/***********************************************************************************************************************************
Macros for encoding StringIds

The general pattern of the macros is STR[BITS]ID[SIZE](). So, STR5ID2('a', 'b') encodes a two character string in 5 bits.

NOTE: the macros do not contain any error handling so invalid characters for the selected encoding will reduce to 0, which is
rendered as !. This means that strIdToStr(STR5ID3('a', 'A', 'a')) == "a!a" and STR5ID3('a', 'A', 'a') == STR5ID3('a', 'B', 'a'). If
the invalid characters are at the end then the StringId will appear truncated when converted back to a string.
***********************************************************************************************************************************/
// 5-bit StringId - encodes a-z and -
// ---------------------------------------------------------------------------------------------------------------------------------
// Identify a-z and transform to a = 1, b = 2, ...
#define STRID_LOWER(c)                                              ((c > 0x60 && c < 0x7B) * (c - 0x60))
// Identify - and transform to 27
#define STRID_DASH(c)                                               ((c == 0x2D) * (c - 0x12))
// Identify 1-4 and transform to 1 = 28, 2 = 29, ...
#define STRID_NUMBER4(c)                                            ((c > 0x30 && c < 0x35) * (c - 21))

// Identify and transform valid 5-bit characters
#define STR5IDC(c)                                                  (STRID_LOWER(c) | STRID_DASH(c) | STRID_NUMBER4(c))

// Transformation for each string size
#define STR5ID1(c1)                                                                                                                \
    ((uint16_t)STR5IDC(c1) << 4)
#define STR5ID2(c1, c2)                                                                                                            \
    ((uint16_t)STR5IDC(c1) << 4 | (uint16_t)STR5IDC(c2) << 9)
#define STR5ID3(c1, c2, c3)                                                                                                        \
    ((uint32_t)STR5IDC(c1) << 4 | (uint32_t)STR5IDC(c2) << 9 | (uint32_t)STR5IDC(c3) << 14)
#define STR5ID4(c1, c2, c3, c4)                                                                                                    \
    ((uint32_t)STR5IDC(c1) << 4 | (uint32_t)STR5IDC(c2) << 9 | (uint32_t)STR5IDC(c3) << 14 | (uint32_t)STR5IDC(c4) << 19)
#define STR5ID5(c1, c2, c3, c4, c5)                                                                                                \
    ((uint32_t)STR5IDC(c1) << 4 | (uint32_t)STR5IDC(c2) << 9 | (uint32_t)STR5IDC(c3) << 14 | (uint32_t)STR5IDC(c4) << 19 |         \
     (uint32_t)STR5IDC(c5) << 24)
#define STR5ID6(c1, c2, c3, c4, c5, c6)                                                                                            \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29)
#define STR5ID7(c1, c2, c3, c4, c5, c6, c7)                                                                                        \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34)
#define STR5ID8(c1, c2, c3, c4, c5, c6, c7, c8)                                                                                    \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34 | (uint64_t)STR5IDC(c8) << 39)
#define STR5ID9(c1, c2, c3, c4, c5, c6, c7, c8, c9)                                                                                \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34 | (uint64_t)STR5IDC(c8) << 39 |       \
     (uint64_t)STR5IDC(c9) << 44)
#define STR5ID10(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                                                                          \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34 | (uint64_t)STR5IDC(c8) << 39 |       \
     (uint64_t)STR5IDC(c9) << 44 | (uint64_t)STR5IDC(c10) << 49)
#define STR5ID11(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)                                                                     \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34 | (uint64_t)STR5IDC(c8) << 39 |       \
     (uint64_t)STR5IDC(c9) << 44 | (uint64_t)STR5IDC(c10) << 49 | (uint64_t)STR5IDC(c11) << 54)
#define STR5ID12(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12)                                                                \
    ((uint64_t)STR5IDC(c1) << 4 | (uint64_t)STR5IDC(c2) << 9 | (uint64_t)STR5IDC(c3) << 14 | (uint64_t)STR5IDC(c4) << 19 |         \
     (uint64_t)STR5IDC(c5) << 24 | (uint64_t)STR5IDC(c6) << 29 | (uint64_t)STR5IDC(c7) << 34 | (uint64_t)STR5IDC(c8) << 39 |       \
     (uint64_t)STR5IDC(c9) << 44 | (uint64_t)STR5IDC(c10) << 49 | (uint64_t)STR5IDC(c11) << 54 | (uint64_t)STR5IDC(c12) << 59)

// 6-bit StringId - encodes a-z, A-Z, 0-9 and -
// ---------------------------------------------------------------------------------------------------------------------------------
// Identify 0-9 and transform to 0 = 28, 1 = 29, ...
#define STRID_NUMBER(c)                                             ((c > 0x2F && c < 0x3A) * (c - 20))
// Identify A-Z and transform to A = 38, B = 39, ...
#define STRID_UPPER(c)                                              ((c > 0x40 && c < 0x5B) * (c - 27))

// Identify and transform valid 6-bit characters
#define STR6IDC(c)                                                                                                                 \
    (STRID_LOWER(c) | STRID_DASH(c) | STRID_NUMBER(c) | STRID_UPPER(c))

// Transformation for each string size
#define STR6ID1(c1)                                                                                                                \
    (stringIdBit6 | (uint16_t)STR6IDC(c1) << 4)
#define STR6ID2(c1, c2)                                                                                                            \
    (stringIdBit6 | (uint16_t)STR6IDC(c1) << 4 | (uint16_t)STR6IDC(c2) << 10)
#define STR6ID3(c1, c2, c3)                                                                                                        \
    (stringIdBit6 | (uint32_t)STR6IDC(c1) << 4 | (uint32_t)STR6IDC(c2) << 10 | (uint32_t)STR6IDC(c3) << 16)
#define STR6ID4(c1, c2, c3, c4)                                                                                                    \
    (stringIdBit6 | (uint32_t)STR6IDC(c1) << 4 | (uint32_t)STR6IDC(c2) << 10 | (uint32_t)STR6IDC(c3) << 16 |                       \
     (uint32_t)STR6IDC(c4) << 22)
#define STR6ID5(c1, c2, c3, c4, c5)                                                                                                \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28)
#define STR6ID6(c1, c2, c3, c4, c5, c6)                                                                                            \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28 | (uint64_t)STR6IDC(c6) << 34)
#define STR6ID7(c1, c2, c3, c4, c5, c6, c7)                                                                                        \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28 | (uint64_t)STR6IDC(c6) << 34 | (uint64_t)STR6IDC(c7) << 40)
#define STR6ID8(c1, c2, c3, c4, c5, c6, c7, c8)                                                                                    \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28 | (uint64_t)STR6IDC(c6) << 34 | (uint64_t)STR6IDC(c7) << 40 |       \
     (uint64_t)STR6IDC(c8) << 46)
#define STR6ID9(c1, c2, c3, c4, c5, c6, c7, c8, c9)                                                                                \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28 | (uint64_t)STR6IDC(c6) << 34 | (uint64_t)STR6IDC(c7) << 40 |       \
     (uint64_t)STR6IDC(c8) << 46 | (uint64_t)STR6IDC(c9) << 52)
#define STR6ID10(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                                                                          \
    (stringIdBit6 | (uint64_t)STR6IDC(c1) << 4 | (uint64_t)STR6IDC(c2) << 10 | (uint64_t)STR6IDC(c3) << 16 |                       \
     (uint64_t)STR6IDC(c4) << 22 | (uint64_t)STR6IDC(c5) << 28 | (uint64_t)STR6IDC(c6) << 34 | (uint64_t)STR6IDC(c7) << 40 |       \
     (uint64_t)STR6IDC(c8) << 46 | (uint64_t)STR6IDC(c9) << 52 | (uint64_t)STR6IDC(c10) << 58)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert N chars to StringId, If the string is longer than the allowable length for the selected encoding then the StringID will
// be marked as "partial" and will have a '+' appended whenever it is converted back to a string.
StringId strIdFromZN(const StringIdBit bit, const char *const buffer, const size_t size);

// Convert String to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromStr(const StringIdBit bit, const String *const str)
{
    return strIdFromZN(bit, strZ(str), strSize(str));
}

// Convert zero-terminted string to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromZ(const StringIdBit bit, const char *const str)
{
    return strIdFromZN(bit, str, strlen(str));
}

// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to String
String *strIdToStr(const StringId strId);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
size_t strIdToZ(const StringId strId, char *const buffer);

/***********************************************************************************************************************************
Debug function for generate StringId and comment !!!
***********************************************************************************************************************************/
#ifdef DEBUG
    void strIdGenerate(const char *const buffer) __attribute__((__noreturn__));
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
