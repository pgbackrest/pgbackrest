/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#ifndef COMMAND_COMMAND_H
#define COMMAND_COMMAND_H

#include "common/type/stringList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Capture time at the very start of main so total time is more accurate
FN_EXTERN void cmdInit(void);

// Begin the command
FN_EXTERN void cmdBegin(void);

// Get the command options as a string. This is output in cmdBegin() if the log level is high enough but can also be used for
// debugging or error reporting later on.
FN_EXTERN const String *cmdOption(void);

// End the command
FN_EXTERN void cmdEnd(int code, const String *errorMessage);

// Execute an async process. This function will only return in the calling process and the implementation is platform dependent.
FN_EXTERN void cmdAsyncExec(const char *command, const StringList *commandExec);

#endif
