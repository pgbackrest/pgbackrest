/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRING_LIST_H
#define COMMON_TYPE_STRING_LIST_H

#include "common/type/string.h"

/***********************************************************************************************************************************
String list type
***********************************************************************************************************************************/
typedef struct StringList StringList;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StringList *strLstNew();
StringList *strLstAdd(StringList *this, String *string);
String *strLstGet(const StringList *this, unsigned int listIdx);
String *strLstCat(StringList *this, const char *separator);
const char **strLstPtr(const StringList *this);
unsigned int strLstSize(const StringList *this);
void strLstFree(StringList *this);

#endif
