/***********************************************************************************************************************************
String Handler Extensions
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_STRING_H
#define TEST_HARNESS_STRING_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Upper-case entire string
String *hrnStrUpper(String *this);

// Replace a substring with another string. Returns the number of replacements made.
unsigned int hrnStrReplace(String *this, const String *replace, const String *with);

#endif
