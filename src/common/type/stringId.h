/***********************************************************************************************************************************
Represent Short Strings as Integers

!!!
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGID_H
#define COMMON_TYPE_STRINGID_H

#include <stddef.h>
#include <stdint.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
!!! AND EXPLAIN WHY THIS IS A TYPEDEF SINCE WE DO NOT NORMALLY TYPEDEF STANDARD TYPES
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
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
// Convert a string id back to a String, usually for logging
String *strIdToStr(const StringId strId);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize);

#define FUNCTION_LOG_STRINGID_TYPE                                                                                                 \
    StringId
#define FUNCTION_LOG_STRINGID_FORMAT(value, buffer, bufferSize)                                                                    \
    strIdToLog(value, buffer, bufferSize)

#endif
