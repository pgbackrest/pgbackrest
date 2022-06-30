/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#ifndef COMMAND_EXIT_H
#define COMMAND_EXIT_H

#include <stdbool.h>
#include <signal.h>

/***********************************************************************************************************************************
Signal type
***********************************************************************************************************************************/
typedef enum
{
#ifndef _WIN32
    signalTypeNone = 0,
    signalTypeHup = SIGHUP,
    signalTypeInt = SIGINT,
    signalTypeTerm = SIGTERM,
#else
    signalTypeNone = -1,
    // Provided, for compatibility
    signalTypeHup = CTRL_CLOSE_EVENT,
    signalTypeInt = CTRL_C_EVENT,
    signalTypeTerm = CTRL_CLOSE_EVENT,
    // Windows Signals
    signalTypeCtrlC = CTRL_C_EVENT,
    signalTypeCtrlBreak = CTRL_BREAK_EVENT,
    signalTypeCtrlClose = CTRL_CLOSE_EVENT,
    signalTypeCtrlLogoff = CTRL_LOGOFF_EVENT,
    signalTypeCtrlShutdown = CTRL_SHUTDOWN_EVENT,
#endif
} SignalType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Setup signal handlers
void exitInit(void);

// Do cleanup and return result code
int exitSafe(int result, bool error, SignalType signalType);

#endif
