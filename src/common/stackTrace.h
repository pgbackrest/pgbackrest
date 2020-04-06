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

#ifdef NDEBUG
    #define STACK_TRACE_POP(test)                                                                                                  \
        stackTracePop();
#else
    #define STACK_TRACE_POP(test)                                                                                                  \
        stackTracePop(__FILE__, __func__, test);
#endif

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
// Backtrace init
#ifdef WITH_BACKTRACE
    void stackTraceInit(const char *exe);
#endif

#ifndef NDEBUG
    // Enable/disable test function logging
    void stackTraceTestStart(void);
    void stackTraceTestStop(void);
    bool stackTraceTest(void);

    // Set line number for the current function on the stack
    void stackTraceTestFileLineSet(unsigned int fileLine);
#endif

// Push a new function onto the trace stack
LogLevel stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel);

// Pop a function from the trace stack
#ifdef NDEBUG
    void stackTracePop(void);
#else
    void stackTracePop(const char *fileName, const char *functionName, bool test);
#endif

// Generate the stack trace
size_t stackTraceToZ(char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine);

// Mark that parameters are being logged -- it none appear then the function is void
void stackTraceParamLog(void);

// Get parameters for the top function on the stack
const char *stackTraceParam(void);

// Get the next location where a parameter can be added in the param buffer
char *stackTraceParamBuffer(const char *param);

// Add a parameter to the function on the top of the stack
void stackTraceParamAdd(size_t bufferSize);

// Clean the stack at and below the try level. Called by the error to cleanup the stack when an exception occurs.
void stackTraceClean(unsigned int tryDepth);

#endif
