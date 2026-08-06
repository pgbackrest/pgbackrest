/***********************************************************************************************************************************
Regular Expression Handler Extensions
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_REGEXP_H
#define TEST_HARNESS_REGEXP_H

#include "common/regExp.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get pointer to the last match. NULL if there was no match.
const char *hrnRegExpMatchPtr(RegExp *this, const String *string);

// Get the last match as a String. NULL if there was no match.
String *hrnRegExpMatchStr(RegExp *this, const String *string);

#endif
