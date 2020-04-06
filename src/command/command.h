/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#ifndef COMMAND_COMMAND_H
#define COMMAND_COMMAND_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Capture time at the very start of main so total time is more accurate
void cmdInit(void);

// Begin the command
void cmdBegin(bool logOption);

// End the command
void cmdEnd(int code, const String *errorMessage);

#endif
