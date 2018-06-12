/***********************************************************************************************************************************
String Handler
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
size_t strSize(const String *this);
String *strSub(const String *this, size_t start);
String *strSubN(const String *this, size_t start, size_t size);
String *strTrim(String *this);
int strChr(const String *this, char chr);
String *strTrunc(String *this, int idx);

void strFree(String *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strToLog(const String *this, char *buffer, size_t bufferSize);

#define FUNCTION_DEBUG_CONST_STRING_TYPE                                                                                           \
    const String *
#define FUNCTION_DEBUG_CONST_STRING_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_DEBUG_STRING_FORMAT(value, buffer, bufferSize)

#define FUNCTION_DEBUG_STRING_TYPE                                                                                                 \
    String *
#define FUNCTION_DEBUG_STRING_FORMAT(value, buffer, bufferSize)                                                                    \
    strToLog(value, buffer, bufferSize)

#define FUNCTION_DEBUG_STRINGP_TYPE                                                                                                \
    const String **
#define FUNCTION_DEBUG_STRINGP_FORMAT(value, buffer, bufferSize)                                                                   \
    ptrToLog(value, "String **", buffer, bufferSize)

#endif
