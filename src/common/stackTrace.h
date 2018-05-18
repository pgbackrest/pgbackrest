/***********************************************************************************************************************************
Stack Trace Handler
***********************************************************************************************************************************/
#ifndef COMMON_STACKTRACE_H
#define COMMON_STACKTRACE_H

#include <sys/types.h>

#include "common/log.h"

/***********************************************************************************************************************************
Maximum size of a single parameter (including NULL terminator)
***********************************************************************************************************************************/
#define STACK_TRACE_PARAM_MAX                                       256

/***********************************************************************************************************************************
Macros to access internal functions
***********************************************************************************************************************************/
#define STACK_TRACE_PUSH(logLevel)                                                                                                 \
    stackTracePush(__FILE__, __func__, logLevel)

#ifdef NDEBUG
    #define STACK_TRACE_POP()                                                                                                      \
        stackTracePop();
#else
    #define STACK_TRACE_POP()                                                                                                      \
        stackTracePop(__FILE__, __func__);
#endif

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
#ifdef WITH_BACKTRACE
    void stackTraceInit(const char *exe);
#endif

#ifndef NDEBUG
    void stackTraceTestStart();
    void stackTraceTestStop();
    bool stackTraceTest();
#endif

LogLevel stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel);

#ifdef NDEBUG
    void stackTracePop();
#else
    void stackTracePop(const char *fileName, const char *functionName);
#endif

size_t stackTraceToZ(char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine);
void stackTraceParamLog();
const char *stackTraceParam();
char *stackTraceParamBuffer(const char *param);
void stackTraceParamAdd(size_t bufferSize);

void stackTraceClean(unsigned int tryDepth);

#endif
