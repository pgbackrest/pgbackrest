/***********************************************************************************************************************************
Error Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/error.h"
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Represents an error type
***********************************************************************************************************************************/
struct ErrorType
{
    const int code;
    const char *name;
    const struct ErrorType *parentType;
};

// Macro for defining new error types
#define ERROR_DEFINE(code, name, parentType)                                                                                       \
    const ErrorType name = {code, #name, &parentType}

// Define test error
#ifdef DEBUG
    ERROR_DEFINE(1, TestError, RuntimeError);
#endif

// Include error type definitions
#include "common/error.auto.c"

/***********************************************************************************************************************************
Maximum allowed number of nested try blocks
***********************************************************************************************************************************/
#define ERROR_TRY_MAX                                             64

/***********************************************************************************************************************************
States for each try
***********************************************************************************************************************************/
typedef enum {errorStateBegin, errorStateTry, errorStateCatch, errorStateFinal, errorStateEnd} ErrorState;

/***********************************************************************************************************************************
Track error handling
***********************************************************************************************************************************/
typedef struct Error
{
    const ErrorType *errorType;                                     // Error type
    const char *fileName;                                           // Source file where the error occurred
    const char *functionName;                                       // Function where the error occurred
    int fileLine;                                                   // Source file line where the error occurred
    const char *message;                                            // Description of the error
    const char *stackTrace;                                         // Stack trace
} Error;

static struct
{
    // Array of jump buffers
    jmp_buf jumpList[ERROR_TRY_MAX];

    // Handler list
    const ErrorHandlerFunction *handlerList;
    unsigned int handlerTotal;

    // State of each try
    int tryTotal;

    struct
    {
        ErrorState state;
        bool uncaught;
    } tryList[ERROR_TRY_MAX + 1];

    // Last error
    Error error;
} errorContext;

/***********************************************************************************************************************************
Message buffer and buffer size

The message buffer is statically allocated so there is some space to store error messages.  Not being able to allocate such a small
amount of memory seems pretty unlikely so just keep the code simple and let the loader deal with massively constrained memory
situations.

The temp buffer is required because the error message being passed might be the error already stored in the message buffer.
***********************************************************************************************************************************/
#define ERROR_MESSAGE_BUFFER_SIZE                                   8192

static char messageBuffer[ERROR_MESSAGE_BUFFER_SIZE];
static char messageBufferTemp[ERROR_MESSAGE_BUFFER_SIZE];
static char stackTraceBuffer[ERROR_MESSAGE_BUFFER_SIZE];

/**********************************************************************************************************************************/
void errorHandlerSet(ErrorHandlerFunction *list, unsigned int total)
{
    assert(total == 0 || list != NULL);

    errorContext.handlerList = list;
    errorContext.handlerTotal = total;
}

/**********************************************************************************************************************************/
int
errorTypeCode(const ErrorType *errorType)
{
    return errorType->code;
}

/**********************************************************************************************************************************/
const ErrorType *
errorTypeFromCode(int code)
{
    // Search for error type by code
    int errorTypeIdx = 0;
    const ErrorType *result = errorTypeList[errorTypeIdx];

    while (result != NULL)
    {
        if (result->code == code)
            break;

        result = errorTypeList[++errorTypeIdx];
    }

    // Error if type was not found
    if (result == NULL)
        result = &UnknownError;

    return result;
}

/**********************************************************************************************************************************/
const char *
errorTypeName(const ErrorType *errorType)
{
    return errorType->name;
}

/**********************************************************************************************************************************/
const ErrorType *
errorTypeParent(const ErrorType *errorType)
{
    return errorType->parentType;
}

/**********************************************************************************************************************************/
unsigned int errorTryDepth(void)
{
    return (unsigned int)errorContext.tryTotal;
}

/**********************************************************************************************************************************/
bool
errorTypeExtends(const ErrorType *child, const ErrorType *parent)
{
    const ErrorType *find = child;

    do
    {
        find = errorTypeParent(find);

        // Parent was found
        if (find == parent)
            return true;
    }
    while (find != errorTypeParent(find));

    // Parent was not found
    return false;
}

/**********************************************************************************************************************************/
const ErrorType *
errorType(void)
{
    assert(errorContext.error.errorType != NULL);

    return errorContext.error.errorType;
}

/**********************************************************************************************************************************/
int
errorCode(void)
{
    return errorTypeCode(errorType());
}

/**********************************************************************************************************************************/
const char *
errorFileName(void)
{
    assert(errorContext.error.fileName != NULL);

    return errorContext.error.fileName;
}

/**********************************************************************************************************************************/
const char *
errorFunctionName(void)
{
    assert(errorContext.error.functionName != NULL);

    return errorContext.error.functionName;
}

/**********************************************************************************************************************************/
int
errorFileLine(void)
{
    assert(errorContext.error.fileLine != 0);

    return errorContext.error.fileLine;
}

/**********************************************************************************************************************************/
const char *
errorMessage(void)
{
    assert(errorContext.error.message != NULL);

    return errorContext.error.message;
}

/**********************************************************************************************************************************/
const char *
errorName(void)
{
    return errorTypeName(errorType());
}

/**********************************************************************************************************************************/
const char *
errorStackTrace(void)
{
    assert(errorContext.error.stackTrace != NULL);

    return errorContext.error.stackTrace;
}

/**********************************************************************************************************************************/
bool
errorInstanceOf(const ErrorType *errorTypeTest)
{
    return errorType() == errorTypeTest || errorTypeExtends(errorType(), errorTypeTest);
}

/***********************************************************************************************************************************
Return current error context state
***********************************************************************************************************************************/
static ErrorState
errorInternalState(void)
{
    return errorContext.tryList[errorContext.tryTotal].state;
}

/**********************************************************************************************************************************/
bool
errorInternalStateTry(void)
{
    return errorInternalState() == errorStateTry;
}

/**********************************************************************************************************************************/
bool
errorInternalStateCatch(const ErrorType *errorTypeCatch)
{
    if (errorInternalState() == errorStateCatch && errorInstanceOf(errorTypeCatch))
        return errorInternalProcess(true);

    return false;
}

/**********************************************************************************************************************************/
bool
errorInternalStateFinal(void)
{
    return errorInternalState() == errorStateFinal;
}

/**********************************************************************************************************************************/
jmp_buf *
errorInternalJump(void)
{
    return &errorContext.jumpList[errorContext.tryTotal - 1];
}

/**********************************************************************************************************************************/
bool
errorInternalTry(const char *fileName, const char *functionName, int fileLine)
{
    // If try total has been exceeded then throw an error
    if (errorContext.tryTotal >= ERROR_TRY_MAX)
        errorInternalThrowFmt(&AssertError, fileName, functionName, fileLine, "too many nested try blocks");

    // Increment try total
    errorContext.tryTotal++;

    // Setup try
    errorContext.tryList[errorContext.tryTotal].state = errorStateBegin;
    errorContext.tryList[errorContext.tryTotal].uncaught = false;

    // Try setup was successful
    return true;
}

/**********************************************************************************************************************************/
void
errorInternalPropagate(void)
{
    assert(errorContext.error.errorType != NULL);

    // Mark the error as uncaught
    errorContext.tryList[errorContext.tryTotal].uncaught = true;

    // If there is a parent try then jump to it
    if (errorContext.tryTotal > 0)
        longjmp(errorContext.jumpList[errorContext.tryTotal - 1], 1);

    // If there was no try to catch this error then output to stderr
    fprintf(stderr, "\nUncaught %s: %s\n    thrown at %s:%d\n\n", errorName(), errorMessage(), errorFileName(), errorFileLine());
    fflush(stderr);

    // Exit with failure
    exit(UnhandledError.code);
}

/**********************************************************************************************************************************/
bool
errorInternalProcess(bool catch)
{
    // If a catch statement then return
    if (catch)
    {
        errorContext.tryList[errorContext.tryTotal].uncaught = false;
        return true;
    }
    // Else if just entering error state clean up the stack
    else if (errorContext.tryList[errorContext.tryTotal].state == errorStateTry)
    {
        for (unsigned int handlerIdx = 0; handlerIdx < errorContext.handlerTotal; handlerIdx++)
            errorContext.handlerList[handlerIdx](errorTryDepth());
    }

    // Any catch blocks have been processed and none of them called RETHROW() so clear the error
    if (errorContext.tryList[errorContext.tryTotal].state == errorStateCatch &&
        !errorContext.tryList[errorContext.tryTotal].uncaught)
    {
        errorContext.error = (Error){0};
    }

    // Increment the state
    errorContext.tryList[errorContext.tryTotal].state++;

    // If the error has been caught then increment the state
    if (errorContext.tryList[errorContext.tryTotal].state == errorStateCatch &&
        !errorContext.tryList[errorContext.tryTotal].uncaught)
    {
        errorContext.tryList[errorContext.tryTotal].state++;
    }

    // Return if not done
    if (errorContext.tryList[errorContext.tryTotal].state < errorStateEnd)
        return true;

    // Remove the try
    errorContext.tryTotal--;

    // If not caught in the last try then propagate
    if (errorContext.tryList[errorContext.tryTotal + 1].uncaught)
        errorInternalPropagate();

    // Nothing left to process
    return false;
}

/**********************************************************************************************************************************/
void
errorInternalThrow(const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
{
    // Setup error data
    errorContext.error.errorType = errorType;
    errorContext.error.fileName = fileName;
    errorContext.error.functionName = functionName;
    errorContext.error.fileLine = fileLine;

    // Assign message to the error
    strncpy(messageBuffer, message, sizeof(messageBuffer));
    messageBuffer[sizeof(messageBuffer) - 1] = 0;

    errorContext.error.message = (const char *)messageBuffer;

    // Generate the stack trace for the error
    if (stackTraceToZ(
            stackTraceBuffer, sizeof(stackTraceBuffer), fileName, functionName, (unsigned int)fileLine) >= sizeof(stackTraceBuffer))
    {
        // Indicate that the stack trace was truncated
    }

    errorContext.error.stackTrace = (const char *)stackTraceBuffer;

    // Propagate the error
    errorInternalPropagate();
}

void
errorInternalThrowFmt(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
{
    // Format message
    va_list argument;
    va_start(argument, format);
    vsnprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, format, argument);
    va_end(argument);

    errorInternalThrow(errorType, fileName, functionName, fileLine, messageBufferTemp);
}

/**********************************************************************************************************************************/
void
errorInternalThrowSys(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
{
    // Format message with system message appended
    if (errNo == 0)
    {
        strncpy(messageBufferTemp, message, ERROR_MESSAGE_BUFFER_SIZE - 1);
        messageBufferTemp[sizeof(messageBuffer) - 1] = 0;
    }
    else
        snprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, "%s: [%d] %s", message, errNo, strerror(errNo));

    errorInternalThrow(errorType, fileName, functionName, fileLine, messageBufferTemp);
}

#ifdef DEBUG_COVERAGE
void
errorInternalThrowOnSys(
    bool error, int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine,
    const char *message)
{
    if (error)
        errorInternalThrowSys(errNo, errorType, fileName, functionName, fileLine, message);
}
#endif

void
errorInternalThrowSysFmt(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
{
    // Format message
    va_list argument;
    va_start(argument, format);
    size_t messageSize = (size_t)vsnprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, format, argument);
    va_end(argument);

    // Append the system message
    if (errNo != 0)
        snprintf(messageBufferTemp + messageSize, ERROR_MESSAGE_BUFFER_SIZE - 1 - messageSize, ": [%d] %s", errNo, strerror(errNo));

    errorInternalThrow(errorType, fileName, functionName, fileLine, messageBufferTemp);
}

#ifdef DEBUG_COVERAGE
void
errorInternalThrowOnSysFmt(
    bool error, int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine,
    const char *format, ...)
{
    if (error)
    {
        // Format message
        va_list argument;
        va_start(argument, format);
        size_t messageSize = (size_t)vsnprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, format, argument);
        va_end(argument);

        // Append the system message
        if (errNo != 0)
        {
            snprintf(
                messageBufferTemp + messageSize, ERROR_MESSAGE_BUFFER_SIZE - 1 - messageSize, ": [%d] %s", errNo, strerror(errNo));
        }

        errorInternalThrow(errorType, fileName, functionName, fileLine, messageBufferTemp);
    }
}
#endif
