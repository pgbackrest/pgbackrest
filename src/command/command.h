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
void cmdBegin(void);

// Get the command options as a string. This is output in cmdBegin() if the log level is high enough but can also be used for
// debugging or error reporting later on.
const String *cmdOption(void);

// End the command
void cmdEnd(int code, const String *errorMessage);

#endif
