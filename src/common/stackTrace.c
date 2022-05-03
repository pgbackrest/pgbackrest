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
#include "common/macro.h"
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Max call stack depth
***********************************************************************************************************************************/
#define STACK_TRACE_MAX                                             128

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
// Stack trace function data
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

static struct StackTraceLocal
{
    int stackSize;                                                  // Stack size
    StackTraceData stack[STACK_TRACE_MAX];                          // Stack data
    char functionParamBuffer[32 * 1024];                            // Buffer to hold function parameters
} stackTraceLocal;

/**********************************************************************************************************************************/
#ifdef WITH_BACKTRACE

static struct StackTraceBackLocal
{
    struct backtrace_state *backTraceState;                         // Backtrace state struct
} stackTraceBackLocal;

void
stackTraceInit(const char *exe)
{
    if (stackTraceBackLocal.backTraceState == NULL)
        stackTraceBackLocal.backTraceState = backtrace_create_state(exe, false, NULL, NULL);
}

static int
backTraceCallback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function)
{
    (void)(data);
    (void)(pc);
    (void)(filename);
    (void)(function);

    if (stackTraceLocal.stackSize > 0)
        stackTraceLocal.stack[stackTraceLocal.stackSize - 1].fileLine = (unsigned int)lineno;

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
#ifdef DEBUG

static struct StackTraceTestLocal
{
    bool testFlag;                                        // Don't log in parameter logging functions to avoid recursion
} stackTraceTestLocal = {.testFlag = true};

void
stackTraceTestStart(void)
{
    stackTraceTestLocal.testFlag = true;
}

void
stackTraceTestStop(void)
{
    stackTraceTestLocal.testFlag = false;
}

bool
stackTraceTest(void)
{
    return stackTraceTestLocal.testFlag;
}

void
stackTraceTestFileLineSet(unsigned int fileLine)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].fileLine = fileLine;
}

#endif

/**********************************************************************************************************************************/
LogLevel
stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel)
{
    ASSERT(stackTraceLocal.stackSize < STACK_TRACE_MAX - 1);

    // Get line number from backtrace if available
#ifdef WITH_BACKTRACE
    backtrace_full(stackTraceBackLocal.backTraceState, 2, backTraceCallback, backTraceCallbackError, NULL);
#endif

    // Set function info
    StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize];

    *data = (StackTraceData)
    {
        .fileName = fileName,
        .functionName = functionName,
        .tryDepth = errorTryDepth(),
    };

    // Set param pointer
    if (stackTraceLocal.stackSize == 0)
    {
        data->param = stackTraceLocal.functionParamBuffer;
        data->functionLogLevel = functionLogLevel;
    }
    else
    {
        StackTraceData *dataPrior = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];

        data->param = dataPrior->param + dataPrior->paramSize + 1;

        // Log level cannot be lower than the prior function
        if (functionLogLevel < dataPrior->functionLogLevel)
            data->functionLogLevel = dataPrior->functionLogLevel;
        else
            data->functionLogLevel = functionLogLevel;
    }

    stackTraceLocal.stackSize++;

    return data->functionLogLevel;
}

/**********************************************************************************************************************************/
static const char *
stackTraceParamIdx(int stackIdx)
{
    ASSERT(stackTraceLocal.stackSize > 0);
    ASSERT(stackIdx < stackTraceLocal.stackSize);

    StackTraceData *data = &stackTraceLocal.stack[stackIdx];

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
    return stackTraceParamIdx(stackTraceLocal.stackSize - 1);
}

/**********************************************************************************************************************************/
char *
stackTraceParamBuffer(const char *paramName)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];
    size_t paramNameSize = strlen(paramName);

    // Make sure that adding this parameter will not overflow the buffer
    if ((size_t)(data->param - stackTraceLocal.functionParamBuffer) + data->paramSize + paramNameSize + 4 >
        sizeof(stackTraceLocal.functionParamBuffer) - (STACK_TRACE_PARAM_MAX * 2))
    {
        // Set overflow to true
        data->paramOverflow = true;

        // There's no way to stop the parameter from being formatted so we reserve a space at the end where the format can safely
        // take place and not disturb the rest of the buffer.  Hopefully overflows just won't happen but we need to be prepared in
        // case of runaway recursion or some other issue that fills the buffer because we don't want a segfault.
        return stackTraceLocal.functionParamBuffer + sizeof(stackTraceLocal.functionParamBuffer) - STACK_TRACE_PARAM_MAX;
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
    ASSERT(stackTraceLocal.stackSize > 0);

    StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];

    if (!data->paramOverflow)
        data->paramSize += bufferSize;
}

/**********************************************************************************************************************************/
void
stackTraceParamLog(void)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].paramLog = true;
}

/**********************************************************************************************************************************/
#ifdef DEBUG

void
stackTracePop(const char *fileName, const char *functionName, bool test)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    if (!test || stackTraceTest())
    {
        stackTraceLocal.stackSize--;

        StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize];

        if (strcmp(data->fileName, fileName) != 0 || strcmp(data->functionName, functionName) != 0)
            THROW_FMT(AssertError, "popping %s:%s but expected %s:%s", fileName, functionName, data->fileName, data->functionName);
    }
}

#else

void
stackTracePop(void)
{
    stackTraceLocal.stackSize--;
}

#endif

/***********************************************************************************************************************************
Stack trace format
***********************************************************************************************************************************/
__attribute__((format(printf, 4, 5))) static size_t
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
    int stackIdx = stackTraceLocal.stackSize - 1;

    // If the current function passed in is the same as the top function on the stack then use the parameters for that function
    if (stackTraceLocal.stackSize > 0 && strcmp(fileName, stackTraceLocal.stack[stackIdx].fileName) == 0 &&
        strcmp(functionName, stackTraceLocal.stack[stackIdx].functionName) == 0)
    {
        param = stackTraceParamIdx(stackTraceLocal.stackSize - 1);
        stackIdx--;
    }

    // Output the current function
    result = stackTraceFmt(buffer, bufferSize, 0, "%s:%s:%u:(%s)", fileName, functionName, fileLine, param);

    // Output stack if there is anything on it
    if (stackIdx >= 0)
    {
        // If the function passed in was not at the top of the stack then some functions are missing
        if (stackIdx == stackTraceLocal.stackSize - 1)
            result += stackTraceFmt(buffer, bufferSize, result, "\n    ... function(s) omitted ...");

        // Output the rest of the stack
        for (; stackIdx >= 0; stackIdx--)
        {
            StackTraceData *data = &stackTraceLocal.stack[stackIdx];

            result += stackTraceFmt(buffer, bufferSize, result, "\n%s:%s", data->fileName, data->functionName);

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
    while (stackTraceLocal.stackSize > 0 && stackTraceLocal.stack[stackTraceLocal.stackSize - 1].tryDepth >= tryDepth)
        stackTraceLocal.stackSize--;
}
