/***********************************************************************************************************************************
Regular Expression Handler Extensions
***********************************************************************************************************************************/
#ifndef BUILD_COMMON_REGEXP_H
#define BUILD_COMMON_REGEXP_H

#include "common/regExp.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get pointer to the last match. NULL if there was no match.
const char *regExpMatchPtr(RegExp *this, const String *string);

// Get the last match as a String. NULL if there was no match.
String *regExpMatchStr(RegExp *this, const String *string);

#endif
