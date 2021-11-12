/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#ifndef COMMON_EXIT_H
#define COMMON_EXIT_H

#include <signal.h>

/***********************************************************************************************************************************
Signal type
***********************************************************************************************************************************/
typedef enum
{
    signalTypeNone = 0,
    signalTypeHup = SIGHUP,
    signalTypeInt = SIGINT,
    signalTypeTerm = SIGTERM,
} SignalType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Setup signal handlers
void exitInit(void);

// Error on SIGTERM? Defaults to true.
void exitErrorOnSigTerm(bool errorOnSigTerm);

// Do cleanup and return result code
int exitSafe(int result, bool error, SignalType signalType);

#endif
