/***********************************************************************************************************************************
String Handler Extensions
***********************************************************************************************************************************/
#ifndef BUILD_COMMON_STRING_H
#define BUILD_COMMON_STRING_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Upper-case entire string
String *strUpper(String *this);

// Replace a substring with another string
String *strReplace(String *this, const String *replace, const String *with);

#endif
