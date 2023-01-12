/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#ifndef COMMON_REGEXP_H
#define COMMON_REGEXP_H

/***********************************************************************************************************************************
RegExp object
***********************************************************************************************************************************/
typedef struct RegExp RegExp;

#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN RegExp *regExpNew(const String *expression);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Match on a regular expression
FN_EXTERN bool regExpMatch(RegExp *this, const String *string);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Match a regular expression in one call for brevity
FN_EXTERN bool regExpMatchOne(const String *expression, const String *string);

// Return the common prefix of a regular expression, if it has one. The common prefix consists of fixed characters that must always
// be found at the beginning of the string to be matched. Escaped characters will not be included in the prefix. If there is no
// usable prefix then NULL is returned.
FN_EXTERN String *regExpPrefix(const String *expression);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
regExpFree(RegExp *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_REGEXP_TYPE                                                                                                   \
    RegExp *
#define FUNCTION_LOG_REGEXP_FORMAT(value, buffer, bufferSize)                                                                      \
    objNameToLog(value, "RegExp", buffer, bufferSize)

#endif
