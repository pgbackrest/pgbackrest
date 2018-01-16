/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRING_H
#define COMMON_TYPE_STRING_H

/***********************************************************************************************************************************
String object
***********************************************************************************************************************************/
typedef struct String String;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
#include "common/type/buffer.h"

String *strNew(const char *string);
String *strNewBuf(const Buffer *buffer);
String *strNewFmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
String *strNewN(const char *string, size_t size);

String *strBase(const String *this);
bool strBeginsWith(const String *this, const String *beginsWith);
bool strBeginsWithZ(const String *this, const char *beginsWith);
String *strCat(String *this, const char *cat);
String *strCatFmt(String *this, const char *format, ...) __attribute__((format(printf, 2, 3)));
String *strDup(const String *this);
bool strEndsWith(const String *this, const String *endsWith);
bool strEndsWithZ(const String *this, const char *endsWith);
bool strEq(const String *this, const String *compare);
bool strEqZ(const String *this, const char *compare);
const char *strPtr(const String *this);
size_t strSize(const String *this);
String *strTrim(String *this);

void strFree(String *this);

#endif
