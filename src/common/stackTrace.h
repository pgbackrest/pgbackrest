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
#ifdef WITH_BACKTRACE
    void stackTraceInit(const char *exe);
#endif

#ifndef NDEBUG
    void stackTraceTestStart(void);
    void stackTraceTestStop(void);
    bool stackTraceTest(void);

    // Set line number for the current function on the stack
    void stackTraceTestFileLineSet(unsigned int fileLine);
#endif

LogLevel stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel);

#ifdef NDEBUG
    void stackTracePop(void);
#else
    void stackTracePop(const char *fileName, const char *functionName, bool test);
#endif

size_t stackTraceToZ(char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine);
void stackTraceParamLog(void);
const char *stackTraceParam(void);
char *stackTraceParamBuffer(const char *param);
void stackTraceParamAdd(size_t bufferSize);

void stackTraceClean(unsigned int tryDepth);

#endif
