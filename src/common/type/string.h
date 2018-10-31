/***********************************************************************************************************************************
String Handler

Strings are lightweight objects in that they do not have their own memory context, instead they exist in the current context in
which they are instantiated. If a string is needed outside the current memory context, the memory context must be switched to the
old context and then back. Below is a simplified example:

    String *result = NULL;     <--- is created in the current memory context  (referred to as "old context" below)
    MEM_CONTEXT_TEMP_BEGIN()   <--- begins a new temporary context
    {
        String *resultStr = strNewN("myNewStr"); <--- creates a string in the temporary memory context
        memContextSwitch(MEM_CONTEXT_OLD());  <--- switch to the old context so the duplication of the string is in that context
        result = strDup(resultStr);           <--- recreates a copy of the string in the old context where "result" was created.
        memContextSwitch(MEM_CONTEXT_TEMP()); <--- switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END(); <-- frees everything created inside this temporary memory context - i.e resultStr
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRING_H
#define COMMON_TYPE_STRING_H

/***********************************************************************************************************************************
String object
***********************************************************************************************************************************/
typedef struct String String;

#include "common/type/buffer.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
String *strNew(const char *string);
String *strNewBuf(const Buffer *buffer);
String *strNewFmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
String *strNewN(const char *string, size_t size);

String *strBase(const String *this);
bool strBeginsWith(const String *this, const String *beginsWith);
bool strBeginsWithZ(const String *this, const char *beginsWith);
String *strCat(String *this, const char *cat);
String *strCatFmt(String *this, const char *format, ...) __attribute__((format(printf, 2, 3)));
int strCmp(const String *this, const String *compare);
int strCmpZ(const String *this, const char *compare);
String *strDup(const String *this);
bool strEndsWith(const String *this, const String *endsWith);
bool strEndsWithZ(const String *this, const char *endsWith);
bool strEq(const String *this, const String *compare);
bool strEqZ(const String *this, const char *compare);
String *strFirstUpper(String *this);
String *strFirstLower(String *this);
String *strUpper(String *this);
String *strLower(String *this);
String *strPath(const String *this);
const char *strPtr(const String *this);
String *strQuote(const String *this, const String *quote);
String *strQuoteZ(const String *this, const char *quote);
String *strReplaceChr(String *this, char find, char replace);
size_t strSize(const String *this);
String *strSub(const String *this, size_t start);
String *strSubN(const String *this, size_t start, size_t size);
String *strTrim(String *this);
int strChr(const String *this, char chr);
String *strTrunc(String *this, int idx);

void strFree(String *this);

/***********************************************************************************************************************************
Helper function/macro for object logging
***********************************************************************************************************************************/
typedef String *(*StrObjToLogFormat)(const void *object);

size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize);

#define FUNCTION_DEBUG_STRING_OBJECT_FORMAT(object, formatFunc, buffer, bufferSize)                                                                    \
    strObjToLog(object, (StrObjToLogFormat)formatFunc, buffer, bufferSize)

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strToLog(const String *this);

#define FUNCTION_DEBUG_CONST_STRING_TYPE                                                                                           \
    const String *
#define FUNCTION_DEBUG_CONST_STRING_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_DEBUG_STRING_FORMAT(value, buffer, bufferSize)

#define FUNCTION_DEBUG_STRING_TYPE                                                                                                 \
    String *
#define FUNCTION_DEBUG_STRING_FORMAT(value, buffer, bufferSize)                                                                    \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(value, strToLog, buffer, bufferSize)

#define FUNCTION_DEBUG_STRINGP_TYPE                                                                                                \
    const String **
#define FUNCTION_DEBUG_STRINGP_FORMAT(value, buffer, bufferSize)                                                                   \
    ptrToLog(value, "String **", buffer, bufferSize)

#endif
