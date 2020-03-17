/***********************************************************************************************************************************
Primitive Data Types
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_PRIMITIVE_H
#define COMMON_TYPE_PRIMITIVE_H

/***********************************************************************************************************************************
Primitive objects
***********************************************************************************************************************************/
typedef struct PrmUInt64 PrmUInt64;

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
PrmUInt64 *prmUInt64New(uint64_t value);
uint64_t prmUInt64(const PrmUInt64 *this);

/***********************************************************************************************************************************
Fields that are common between dynamically allocated and constant primitives

There is nothing user-accessible here but this construct allows constant primitives to be created and then handled by the same
functions that process dynamically allocated primitives.
***********************************************************************************************************************************/
typedef struct PrmUInt64Const
{
    uint64_t value;
} PrmUInt64Const;

/***********************************************************************************************************************************
Macros for constant primitives

Frequently used constant primitives can be declared with these macros at compile time rather than dynamically at run time.
***********************************************************************************************************************************/
// Create a UInt64 primitive constant inline from a uint64_t
#define PRMUINT64(valueParam)                                                                                                      \
    ((const PrmUInt64 *)&(const UInt64Const){.value = valueParam})

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *prmUInt64ToLog(const PrmUInt64 *this);

#define FUNCTION_LOG_PRM_UINT64_TYPE                                                                                               \
    PrmUInt64 *
#define FUNCTION_LOG_PRM_UINT64_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, prmUInt64ToLog, buffer, bufferSize)

#endif
