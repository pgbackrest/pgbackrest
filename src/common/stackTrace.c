/***********************************************************************************************************************************
Stack Trace Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LIBBACKTRACE
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

#ifdef HAVE_LIBBACKTRACE
    struct backtrace_state *backTraceState;                         // Backtrace state struct
#endif
} stackTraceLocal;


/**********************************************************************************************************************************/
#ifdef DEBUG

static struct StackTraceTestLocal
{
    bool testFlag;                                        // Don't log in parameter logging functions to avoid recursion
} stackTraceTestLocal = {.testFlag = true};

FV_EXTERN void
stackTraceTestStart(void)
{
    stackTraceTestLocal.testFlag = true;
}

FV_EXTERN void
stackTraceTestStop(void)
{
    stackTraceTestLocal.testFlag = false;
}

FV_EXTERN bool
stackTraceTest(void)
{
    return stackTraceTestLocal.testFlag;
}

FV_EXTERN void
stackTraceTestFileLineSet(unsigned int fileLine)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].fileLine = fileLine;
}

#endif

/**********************************************************************************************************************************/
FV_EXTERN LogLevel
stackTracePush(const char *fileName, const char *functionName, LogLevel functionLogLevel)
{
    ASSERT(stackTraceLocal.stackSize < STACK_TRACE_MAX - 1);

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

FV_EXTERN const char *
stackTraceParam()
{
    return stackTraceParamIdx(stackTraceLocal.stackSize - 1);
}

/**********************************************************************************************************************************/
FV_EXTERN char *
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
FV_EXTERN void
stackTraceParamAdd(size_t bufferSize)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];

    if (!data->paramOverflow)
        data->paramSize += bufferSize;
}

/**********************************************************************************************************************************/
FV_EXTERN void
stackTraceParamLog(void)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].paramLog = true;
}

/**********************************************************************************************************************************/
#ifdef DEBUG

FV_EXTERN void
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

FV_EXTERN void
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
#ifdef HAVE_LIBBACKTRACE

typedef struct BackTraceDumpData
{
    bool firstCall;
    bool firstLine;
    int stackIdx;
    size_t result;
    char *const buffer;
    const size_t bufferSize;
} BackTraceDumpData;

static int
backTraceDump(void *const dataVoid, const uintptr_t pc, const char *fileName, const int fileLine, const char *const functionName)
{
    (void)(pc);
    BackTraceDumpData *const data = dataVoid;

    if (data->stackIdx < 0)
        return true;

    if (fileName == NULL || fileLine == 0 || functionName == NULL) // {uncovered - !!!}
    {
        if (data->firstCall) // {uncovered - !!!}
            return true; // {uncovered - !!!}

        data->firstCall = false; // {uncovered - !!!}
        return false; // {uncovered - !!!}
    }

    data->firstCall = false;
    const StackTraceData *const traceData = &stackTraceLocal.stack[data->stackIdx];

    if (strcmp(functionName, traceData->functionName) == 0) // {uncovered - !!!}
    {
        const char *const src = strstr(traceData->fileName, "src/"); // {uncovered - !!!}

        data->result += stackTraceFmt( // {uncovered - !!!}
            data->buffer, data->bufferSize, data->result, "%s%s:%s:%d:(%s)", data->firstLine ? "" : "\n", // {uncovered - !!!}
            src != NULL ? src + 4 : traceData->fileName, functionName, fileLine, stackTraceParamIdx(data->stackIdx)); // {uncovered - !!!}

        data->stackIdx--; // {uncovered - !!!}
    }
    else
    {
        const char *const src = strstr(fileName, "src/");

        if (src != NULL)
            fileName = src + 4;

        if (strcmp(fileName, "common/error.c") == 0) // {uncovered - !!!}
            return false; // {uncovered - !!!}

        data->result += stackTraceFmt(
            data->buffer, data->bufferSize, data->result, "%s%s:%s:%d:(no parameters available)", data->firstLine ? "" : "\n",
            fileName, functionName, fileLine);
    }

    data->firstLine = false;

    return strcmp(functionName, "main") == 0;
}

static void
backTraceCallbackError(void *data, const char *msg, int errnum)
{
    (void)data;
    (void)msg;
    (void)errnum;
}

#endif

static size_t
stackTraceToZDefault(
    char *const buffer, const size_t bufferSize, const char *const fileName, const char *const functionName,
    const unsigned int fileLine)
{
    const char *param = "test build required for parameters";
    int stackIdx = stackTraceLocal.stackSize - 1;
    size_t result = 0;

    // If the current function passed in is the same as the top function on the stack then use the parameters for that function
    if (stackTraceLocal.stackSize > 0 && strcmp(fileName, stackTraceLocal.stack[stackIdx].fileName) == 0 && // {uncovered - !!!}
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
            StackTraceData *traceData = &stackTraceLocal.stack[stackIdx];

            result += stackTraceFmt(buffer, bufferSize, result, "\n%s:%s", traceData->fileName, traceData->functionName);

            if (traceData->fileLine > 0)
                result += stackTraceFmt(buffer, bufferSize, result, ":%u", traceData->fileLine);

            result += stackTraceFmt(buffer, bufferSize, result, ":(%s)", stackTraceParamIdx(stackIdx));
        }
    }

    return result;
}

FV_EXTERN size_t
stackTraceToZ(
    char *const buffer, const size_t bufferSize, const char *const fileName, const char *const functionName,
    const unsigned int fileLine)
{
#ifdef HAVE_LIBBACKTRACE
    BackTraceDumpData data =
    {
        .firstCall = true,
        .firstLine = true,
        .stackIdx = stackTraceLocal.stackSize - 1,
        .result = 0,
        .buffer = buffer,
        .bufferSize = bufferSize,
    };

    if (stackTraceLocal.backTraceState == NULL)
        stackTraceLocal.backTraceState = backtrace_create_state(NULL, false, NULL, NULL);

    backtrace_full(stackTraceLocal.backTraceState, 2, backTraceDump, backTraceCallbackError, &data);

    if (data.result == 0)
        return stackTraceToZDefault(buffer, bufferSize, fileName, functionName, fileLine);

    return data.result;
#else
    return stackTraceToZDefault(buffer, bufferSize, fileName, functionName, fileLine);
#endif
}

/**********************************************************************************************************************************/
FV_EXTERN void
stackTraceClean(const unsigned int tryDepth, const bool fatal)
{
    (void)fatal;                                                    // Cleanup is the same for fatal errors

    while (stackTraceLocal.stackSize > 0 && stackTraceLocal.stack[stackTraceLocal.stackSize - 1].tryDepth >= tryDepth)
        stackTraceLocal.stackSize--;
}
