/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#ifndef COMMON_REGEXP_H
#define COMMON_REGEXP_H

/***********************************************************************************************************************************
RegExp object
***********************************************************************************************************************************/
#define REGEXP_TYPE                                                 RegExp
#define REGEXP_PREFIX                                               regExp

typedef struct RegExp RegExp;

#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
RegExp *regExpNew(const String *expression);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Match on a regular expression
bool regExpMatch(RegExp *this, const String *string);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Get pointer to the last match.  NULL if there was no match.
const char *regExpMatchPtr(RegExp *this);

// Get size of the last match.  0 if there was no match.
size_t regExpMatchSize(RegExp *this);

// Get the last match as a String.  NULL if there was no match.
String *regExpMatchStr(RegExp *this);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Match a regular expression in one call for brevity
bool regExpMatchOne(const String *expression, const String *string);

// Return the common prefix of a regular expression, if it has one. The common prefix consists of fixed characters that must always
// be found at the beginning of the string to be matched. Escaped characters will not be included in the prefix. If there is no
// usable prefix then NULL is returned.
String *regExpPrefix(const String *expression);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void regExpFree(RegExp *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_REGEXP_TYPE                                                                                                   \
    RegExp *
#define FUNCTION_LOG_REGEXP_FORMAT(value, buffer, bufferSize)                                                                      \
    objToLog(value, "RegExp", buffer, bufferSize)

#endif
