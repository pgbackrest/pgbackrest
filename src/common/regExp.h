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
Functions
***********************************************************************************************************************************/
RegExp *regExpNew(const String *expression);
bool regExpMatch(RegExp *this, const String *string);
void regExpFree(RegExp *this);

bool regExpMatchOne(const String *expression, const String *string);
String *regExpPrefix(const String *expression);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_REGEXP_TYPE                                                                                                   \
    RegExp *
#define FUNCTION_LOG_REGEXP_FORMAT(value, buffer, bufferSize)                                                                      \
    objToLog(value, "RegExp", buffer, bufferSize)

#endif
