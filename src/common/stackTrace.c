/***********************************************************************************************************************************
Stack Trace Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LIBBACKTRACE
    #include <backtrace-supported.h>
    #include <backtrace.h>
#endif

#include "common/assert.h"
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

FN_EXTERN void
stackTraceTestStart(void)
{
    stackTraceTestLocal.testFlag = true;
}

FN_EXTERN void
stackTraceTestStop(void)
{
    stackTraceTestLocal.testFlag = false;
}

FN_EXTERN bool
stackTraceTest(void)
{
    return stackTraceTestLocal.testFlag;
}

FN_EXTERN void
stackTraceTestFileLineSet(unsigned int fileLine)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].fileLine = fileLine;
}

#endif

/**********************************************************************************************************************************/
FN_EXTERN LogLevel
stackTracePush(const char *const fileName, const char *const functionName, const LogLevel functionLogLevel)
{
    ASSERT(stackTraceLocal.stackSize < STACK_TRACE_MAX - 1);

    // Set function info
    StackTraceData *const data = &stackTraceLocal.stack[stackTraceLocal.stackSize];

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
        StackTraceData *const dataPrior = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];

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
stackTraceParamIdx(const int stackIdx)
{
    ASSERT(stackTraceLocal.stackSize > 0);
    ASSERT(stackIdx < stackTraceLocal.stackSize);

    StackTraceData *const data = &stackTraceLocal.stack[stackIdx];

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

FN_EXTERN const char *
stackTraceParam(void)
{
    return stackTraceParamIdx(stackTraceLocal.stackSize - 1);
}

/**********************************************************************************************************************************/
FN_EXTERN char *
stackTraceParamBuffer(const char *const paramName)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    StackTraceData *const data = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];
    const size_t paramNameSize = strlen(paramName);

    // Make sure that adding this parameter will not overflow the buffer
    if ((size_t)(data->param - stackTraceLocal.functionParamBuffer) + data->paramSize + paramNameSize + 4 >
        sizeof(stackTraceLocal.functionParamBuffer) - (STACK_TRACE_PARAM_MAX * 2))
    {
        // Set overflow to true
        data->paramOverflow = true;

        // There's no way to stop the parameter from being formatted so we reserve a space at the end where the format can safely
        // take place and not disturb the rest of the buffer. Hopefully overflows just won't happen but we need to be prepared in
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
FN_EXTERN void
stackTraceParamAdd(const size_t bufferSize)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    StackTraceData *data = &stackTraceLocal.stack[stackTraceLocal.stackSize - 1];

    if (!data->paramOverflow)
        data->paramSize += bufferSize;
}

/**********************************************************************************************************************************/
FN_EXTERN void
stackTraceParamLog(void)
{
    ASSERT(stackTraceLocal.stackSize > 0);

    stackTraceLocal.stack[stackTraceLocal.stackSize - 1].paramLog = true;
}

/**********************************************************************************************************************************/
#ifdef DEBUG

FN_EXTERN void
stackTracePop(const char *const fileName, const char *const functionName, const bool test)
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

FN_EXTERN void
stackTracePop(void)
{
    stackTraceLocal.stackSize--;
}

#endif

/***********************************************************************************************************************************
Stack trace format
***********************************************************************************************************************************/
static FN_PRINTF(4, 5) size_t
stackTraceFmt(char *const buffer, const size_t bufferSize, const size_t bufferUsed, const char *const format, ...)
{
    va_list argumentList;
    va_start(argumentList, format);
    const int result = vsnprintf(
        buffer + bufferUsed, bufferUsed < bufferSize ? bufferSize - bufferUsed : 0, format, argumentList);
    va_end(argumentList);

    return (size_t)result;
}

/***********************************************************************************************************************************
Helper to trim off extra path before the src path
***********************************************************************************************************************************/
static const char *
stackTraceTrimSrc(const char *const fileName)
{
    const char *const src = strstr(fileName, "src/");
    return src == NULL ? fileName : src + 4;
}

/**********************************************************************************************************************************/
#ifdef HAVE_LIBBACKTRACE

typedef struct StackTraceBackData
{
    bool firstCall;
    bool firstLine;
    int stackIdx;
    size_t result;
    char *const buffer;
    const size_t bufferSize;
} StackTraceBackData;

// Callback to add backtrace data when available
static int
stackTraceBackCallback(
    void *const dataVoid, const uintptr_t pc, const char *fileName, const int fileLine, const char *const functionName)
{
    (void)pc;
    StackTraceBackData *const data = dataVoid;

    // Catch any unset parameters which indicates the debug data is not available
    if (fileName == NULL || fileLine == 0 || functionName == NULL)
    {
        // If this is the first call then stop because the top of the backtrace must be one of our functions
        if (data->firstCall)
            return true;

        // Else return but do not stop
        data->firstCall = false;
        return false;
    }

    // Reset first call
    data->firstCall = false;

    // If the function name matches combine backtrace data with stack data
    if (data->stackIdx >= 0 && strcmp(functionName, stackTraceLocal.stack[data->stackIdx].functionName) == 0)
    {
        data->result += stackTraceFmt(
            data->buffer, data->bufferSize, data->result, "%s%s:%s:%d:(%s)", data->firstLine ? "" : "\n",
            stackTraceTrimSrc(stackTraceLocal.stack[data->stackIdx].fileName), functionName, fileLine,
            stackTraceParamIdx(data->stackIdx));

        data->stackIdx--;
    }
    // Else just use stack data. Skip any functions in the error module since they are not useful for the user
    else
    {
        fileName = stackTraceTrimSrc(fileName);

        if (strcmp(fileName, "common/error/error.c") == 0)
            return false;

        data->result += stackTraceFmt(
            data->buffer, data->bufferSize, data->result, "%s%s:%s:%d:(no parameters available)", data->firstLine ? "" : "\n",
            fileName, functionName, fileLine);
    }

    // Reset first line
    data->firstLine = false;

    // Stop when the main function has been processed
    return strcmp(functionName, "main") == 0;
}

// Dummy error callback. If there is an error just generate the default stack trace.
static void
stackTraceBackErrorCallback(void *const data, const char *const msg, const int errnum)
{
    (void)data;
    (void)msg;
    (void)errnum;
}

#endif

FN_EXTERN size_t
stackTraceToZ(
    char *const buffer, const size_t bufferSize, const char *fileName, const char *const functionName,
    const unsigned int fileLine)
{
#ifdef HAVE_LIBBACKTRACE
    // Attempt to use backtrace data
    StackTraceBackData data =
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

    backtrace_full(stackTraceLocal.backTraceState, 2, stackTraceBackCallback, stackTraceBackErrorCallback, &data);

    if (data.result != 0)
        return data.result;
#endif // HAVE_LIBBACKTRACE

    size_t result = 0;
    const char *param = "test build required for parameters";
    int stackIdx = stackTraceLocal.stackSize - 1;

    // If the current function passed in is the same as the top function on the stack then use the parameters for that function
    fileName = stackTraceTrimSrc(fileName);

    if (stackTraceLocal.stackSize > 0 && strcmp(fileName, stackTraceTrimSrc(stackTraceLocal.stack[stackIdx].fileName)) == 0 &&
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
            const StackTraceData *const data = &stackTraceLocal.stack[stackIdx];

            result += stackTraceFmt(buffer, bufferSize, result, "\n%s:%s", stackTraceTrimSrc(data->fileName), data->functionName);

            if (data->fileLine > 0)
                result += stackTraceFmt(buffer, bufferSize, result, ":%u", data->fileLine);

            result += stackTraceFmt(buffer, bufferSize, result, ":(%s)", stackTraceParamIdx(stackIdx));
        }
    }

    return result;
}

/**********************************************************************************************************************************/
FN_EXTERN void
stackTraceClean(const unsigned int tryDepth, const bool fatal)
{
    (void)fatal;                                                    // Cleanup is the same for fatal errors

    while (stackTraceLocal.stackSize > 0 && stackTraceLocal.stack[stackTraceLocal.stackSize - 1].tryDepth >= tryDepth)
        stackTraceLocal.stackSize--;
}
