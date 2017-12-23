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
String *strNew(const char *string);
String *strNewFmt(const char *format, ...);
String *strNewSzN(const char *string, size_t size);

String *strCat(String *this, const char *cat);
String *strCatFmt(String *this, const char *format, ...);
String *strDup(const String *this);
bool strEq(const String *this1, const String *this2);
const char *strPtr(const String *this);
size_t strSize(const String *this);
String *strTrim(String *this);

void strFree(String *this);

#endif
