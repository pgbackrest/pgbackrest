/***********************************************************************************************************************************
Stack Trace Handler
***********************************************************************************************************************************/
#ifndef COMMON_STACKTRACE_H
#define COMMON_STACKTRACE_H

#include <stdbool.h>
#include <sys/types.h>

#include "common/logLevel.h"

/***********************************************************************************************************************************
Maximum size of a single parameter (including NULL terminator)
***********************************************************************************************************************************/
#define STACK_TRACE_PARAM_MAX                                       4096

/***********************************************************************************************************************************
Macros to access internal functions
***********************************************************************************************************************************/
#define STACK_TRACE_PUSH(logLevel)                                                                                                 \
    stackTracePush(__FILE__, __func__, logLevel)

#ifdef DEBUG
    #define STACK_TRACE_POP(test)                                                                                                  \
        stackTracePop(__FILE__, __func__, test);
#else
    #define STACK_TRACE_POP(test)                                                                                                  \
        stackTracePop();
#endif

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
// Backtrace init
#ifdef WITH_BACKTRACE
    void stackTraceInit(const char *exe);
#endif

#ifdef DEBUG
    // Enable/disable test function logging
    FV_EXTERN void stackTraceTestStart(void);
    FV_EXTERN void stackTraceTestStop(void);
    FV_EXTERN bool stackTraceTest(void);

    // Set line number for the current function on the stack
    FV_EXTERN void stackTraceTestFileLineSet(unsigned int fileLine);
#else
    // Must always be valid since it might be needed to compile (though not used) during profiling
    #define stackTraceTestFileLineSet(fileLine)
#endif

// Push a new function onto the trace stack
FV_EXTERN LogLevel stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel);

// Pop a function from the trace stack
#ifdef DEBUG
    FV_EXTERN void stackTracePop(const char *fileName, const char *functionName, bool test);
#else
    FV_EXTERN void stackTracePop(void);
#endif

// Generate the stack trace
FV_EXTERN size_t stackTraceToZ(
    char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine);

// Mark that parameters are being logged -- if none appear then the function is void
FV_EXTERN void stackTraceParamLog(void);

// Get parameters for the top function on the stack
FV_EXTERN const char *stackTraceParam(void);

// Get the next location where a parameter can be added in the param buffer
FV_EXTERN char *stackTraceParamBuffer(const char *param);

// Add a parameter to the function on the top of the stack
FV_EXTERN void stackTraceParamAdd(size_t bufferSize);

// Clean the stack at and below the try level. Called by the error to cleanup the stack when an exception occurs.
FV_EXTERN void stackTraceClean(unsigned int tryDepth, bool fatal);

#endif
