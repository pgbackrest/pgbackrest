/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#ifndef COMMAND_COMMAND_H
#define COMMAND_COMMAND_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cmdBegin(bool logOption);
void cmdEnd(int code, const String *errorMessage);

#endif
