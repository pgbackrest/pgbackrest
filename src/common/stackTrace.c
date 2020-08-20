/***********************************************************************************************************************************
Stack Trace Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef WITH_BACKTRACE
    #include <backtrace.h>
    #include <backtrace-supported.h>
#endif

#include "common/assert.h"
#include "common/error.h"
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Max call stack depth
***********************************************************************************************************************************/
#define STACK_TRACE_MAX                                             128

/***********************************************************************************************************************************
Track stack trace
***********************************************************************************************************************************/
static int stackSize = 0;

typedef struct StackTraceData
{
    const char *fileName;
    const char *functionName;
    unsigned int fileLine;
    LogLevel functionLogLevel;
    unsigned int tryDepth;

    bool paramOverflow;
    bool paramLog;
    char *param;
    size_t paramSize;
} StackTraceData;

static StackTraceData stackTrace[STACK_TRACE_MAX];

/***********************************************************************************************************************************
Buffer to hold function parameters
***********************************************************************************************************************************/
static char functionParamBuffer[32 * 1024];

struct backtrace_state *backTraceState = NULL;

/**********************************************************************************************************************************/
#ifdef WITH_BACKTRACE

void
stackTraceInit(const char *exe)
{
    if (backTraceState == NULL)
        backTraceState = backtrace_create_state(exe, false, NULL, NULL);
}

static int
backTraceCallback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function)
{
    (void)(data);
    (void)(pc);
    (void)(filename);
    (void)(function);

    if (stackSize > 0)
        stackTrace[stackSize - 1].fileLine = (unsigned int)lineno;

    return 1;
}

static void
backTraceCallbackError(void *data, const char *msg, int errnum)
{
    (void)data;
    (void)msg;
    (void)errnum;
}

#endif

/**********************************************************************************************************************************/
#ifndef NDEBUG

bool stackTraceTestFlag = true;

void
stackTraceTestStart(void)
{
    stackTraceTestFlag = true;
}

void
stackTraceTestStop(void)
{
    stackTraceTestFlag = false;
}

bool
stackTraceTest(void)
{
    return stackTraceTestFlag;
}

void
stackTraceTestFileLineSet(unsigned int fileLine)
{
    stackTrace[stackSize - 1].fileLine = fileLine;
}

#endif

/**********************************************************************************************************************************/
LogLevel
stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel)
{
    ASSERT(stackSize < STACK_TRACE_MAX - 1);

    // Get line number from backtrace if available
#ifdef WITH_BACKTRACE
    backtrace_full(backTraceState, 2, backTraceCallback, backTraceCallbackError, NULL);
#endif

    // Set function info
    StackTraceData *data = &stackTrace[stackSize];

    *data = (StackTraceData)
    {
        .fileName = fileName,
        .functionName = functionName,
        .tryDepth = errorTryDepth(),
    };

    // Set param pointer
    if (stackSize == 0)
    {
        data->param = functionParamBuffer;
        data->functionLogLevel = functionLogLevel;
    }
    else
    {
        StackTraceData *dataPrior = &stackTrace[stackSize - 1];

        data->param = dataPrior->param + dataPrior->paramSize + 1;

        // Log level cannot be lower than the prior function
        if (functionLogLevel < dataPrior->functionLogLevel)
            data->functionLogLevel = dataPrior->functionLogLevel;
        else
            data->functionLogLevel = functionLogLevel;
    }

    stackSize++;

    return data->functionLogLevel;
}

/**********************************************************************************************************************************/
static const char *
stackTraceParamIdx(int stackIdx)
{
    ASSERT(stackSize > 0);
    ASSERT(stackIdx < stackSize);

    StackTraceData *data = &stackTrace[stackIdx];

    if (data->paramLog)
    {
        if (data->paramOverflow)
            return "buffer full - parameters not available";

        if (data->paramSize == 0)
            return "void";

        return data->param;
    }

    // If no parameters return the log level required to get them
    #define LOG_LEVEL_REQUIRED " log level required for parameters"
    return data->functionLogLevel == logLevelTrace ? "trace" LOG_LEVEL_REQUIRED : "debug" LOG_LEVEL_REQUIRED;
}

const char *
stackTraceParam()
{
    return stackTraceParamIdx(stackSize - 1);
}

/**********************************************************************************************************************************/
char *
stackTraceParamBuffer(const char *paramName)
{
    ASSERT(stackSize > 0);

    StackTraceData *data = &stackTrace[stackSize - 1];
    size_t paramNameSize = strlen(paramName);

    // Make sure that adding this parameter will not overflow the buffer
    if ((size_t)(data->param - functionParamBuffer) + data->paramSize + paramNameSize + 4 >
        sizeof(functionParamBuffer) - (STACK_TRACE_PARAM_MAX * 2))
    {
        // Set overflow to true
        data->paramOverflow = true;

        // There's no way to stop the parameter from being formatted so we reserve a space at the end where the format can safely
        // take place and not disturb the rest of the buffer.  Hopefully overflows just won't happen but we need to be prepared in
        // case of runaway recursion or some other issue that fills the buffer because we don't want a segfault.
        return functionParamBuffer + sizeof(functionParamBuffer) - STACK_TRACE_PARAM_MAX;
    }

    // Add a comma if a parameter is already in the list
    if (data->paramSize != 0)
    {
        data->param[data->paramSize++] = ',';
        data->param[data->paramSize++] = ' ';
    }

    // Add the parameter name
    strcpy(data->param + data->paramSize, paramName);
    data->paramSize += paramNameSize;

    // Add param/value separator
    data->param[data->paramSize++] = ':';
    data->param[data->paramSize++] = ' ';

    return data->param + data->paramSize;
}

/**********************************************************************************************************************************/
void
stackTraceParamAdd(size_t bufferSize)
{
    ASSERT(stackSize > 0);

    StackTraceData *data = &stackTrace[stackSize - 1];

    if (!data->paramOverflow)
        data->paramSize += bufferSize;
}

/**********************************************************************************************************************************/
void
stackTraceParamLog(void)
{
    ASSERT(stackSize > 0);

    stackTrace[stackSize - 1].paramLog = true;
}

/**********************************************************************************************************************************/
#ifdef NDEBUG

void
stackTracePop(void)
{
    stackSize--;
}

#else

void
stackTracePop(const char *fileName, const char *functionName, bool test)
{
    ASSERT(stackSize > 0);

    if (!test || stackTraceTest())
    {
        stackSize--;

        StackTraceData *data = &stackTrace[stackSize];

        if (strcmp(data->fileName, fileName) != 0 || strcmp(data->functionName, functionName) != 0)
            THROW_FMT(AssertError, "popping %s:%s but expected %s:%s", fileName, functionName, data->fileName, data->functionName);
    }
}

#endif

/***********************************************************************************************************************************
Stack trace format
***********************************************************************************************************************************/
static size_t
stackTraceFmt(char *buffer, size_t bufferSize, size_t bufferUsed, const char *format, ...)
{
    va_list argumentList;
    va_start(argumentList, format);
    int result = vsnprintf(
        buffer + bufferUsed, bufferUsed < bufferSize ? bufferSize - bufferUsed : 0, format, argumentList);
    va_end(argumentList);

    return (size_t)result;
}

/**********************************************************************************************************************************/
size_t
stackTraceToZ(char *buffer, size_t bufferSize, const char *fileName, const char *functionName, unsigned int fileLine)
{
    size_t result = 0;
    const char *param = "test build required for parameters";
    int stackIdx = stackSize - 1;

    // If the current function passed in is the same as the top function on the stack then use the parameters for that function
    if (stackSize > 0 && strcmp(fileName, stackTrace[stackIdx].fileName) == 0 &&
        strcmp(functionName, stackTrace[stackIdx].functionName) == 0)
    {
        param = stackTraceParamIdx(stackSize - 1);
        stackIdx--;
    }

    // Output the current function
    result = stackTraceFmt(
        buffer, bufferSize, 0, "%.*s:%s:%u:(%s)", (int)(strlen(fileName) - 2), fileName, functionName, fileLine, param);

    // Output stack if there is anything on it
    if (stackIdx >= 0)
    {
        // If the function passed in was not at the top of the stack then some functions are missing
        if (stackIdx == stackSize - 1)
            result += stackTraceFmt(buffer, bufferSize, result, "\n    ... function(s) omitted ...");

        // Output the rest of the stack
        for (; stackIdx >= 0; stackIdx--)
        {
            StackTraceData *data = &stackTrace[stackIdx];

            result += stackTraceFmt(
                buffer, bufferSize, result, "\n%.*s:%s", (int)(strlen(data->fileName) - 2), data->fileName, data->functionName);

            if (data->fileLine > 0)
                result += stackTraceFmt(buffer, bufferSize, result, ":%u", data->fileLine);

            result += stackTraceFmt(buffer, bufferSize, result, ":(%s)", stackTraceParamIdx(stackIdx));
        }
    }

    return result;
}

/**********************************************************************************************************************************/
void
stackTraceClean(unsigned int tryDepth)
{
    while (stackSize > 0 && stackTrace[stackSize - 1].tryDepth >= tryDepth)
        stackSize--;
}
