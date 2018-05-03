/***********************************************************************************************************************************
Error Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/error.h"

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

// Include error type definitions
#include "common/error.auto.c"

/***********************************************************************************************************************************
Maximum allowed number of nested try blocks
***********************************************************************************************************************************/
#define ERROR_TRY_MAX                                             32

/***********************************************************************************************************************************
States for each try
***********************************************************************************************************************************/
typedef enum {errorStateBegin, errorStateTry, errorStateCatch, errorStateFinal, errorStateEnd} ErrorState;

/***********************************************************************************************************************************
Track error handling
***********************************************************************************************************************************/
struct
{
    // Array of jump buffers
    jmp_buf jumpList[ERROR_TRY_MAX];

    // State of each try
    int tryTotal;

    struct
    {
        ErrorState state;
        bool uncaught;
    } tryList[ERROR_TRY_MAX + 1];

    // Last error
    struct
    {
        const ErrorType *errorType;                                     // Error type
        const char *fileName;                                           // Source file where the error occurred
        int fileLine;                                                   // Source file line where the error occurred
        const char *message;                                            // Description of the error
    } error;
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

/***********************************************************************************************************************************
Error type code
***********************************************************************************************************************************/
int
errorTypeCode(const ErrorType *errorType)
{
    return errorType->code;
}

/***********************************************************************************************************************************
Get error type using a code
***********************************************************************************************************************************/
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
        THROW_FMT(AssertError, "could not find error type for code '%d'", code);

    return result;
}

/***********************************************************************************************************************************
Error type name
***********************************************************************************************************************************/
const char *
errorTypeName(const ErrorType *errorType)
{
    return errorType->name;
}

/***********************************************************************************************************************************
Error type parent
***********************************************************************************************************************************/
const ErrorType *
errorTypeParent(const ErrorType *errorType)
{
    return errorType->parentType;
}

/***********************************************************************************************************************************
Does the child error type extend the parent error type?
***********************************************************************************************************************************/
bool
errorTypeExtends(const ErrorType *child, const ErrorType *parent)
{
    // Search for the parent
    for (; child && errorTypeParent(child) != child; child = (ErrorType *)errorTypeParent(child))
    {
        if (errorTypeParent(child) == parent)
            return true;
    }

    // Parent was not found
    return false;
}

/***********************************************************************************************************************************
Error type
***********************************************************************************************************************************/
const ErrorType *
errorType()
{
    return errorContext.error.errorType;
}

/***********************************************************************************************************************************
Error code (pulled from error type)
***********************************************************************************************************************************/
int
errorCode()
{
    return errorTypeCode(errorType());
}

/***********************************************************************************************************************************
Error filename
***********************************************************************************************************************************/
const char *
errorFileName()
{
    return errorContext.error.fileName;
}

/***********************************************************************************************************************************
Error file line number
***********************************************************************************************************************************/
int
errorFileLine()
{
    return errorContext.error.fileLine;
}

/***********************************************************************************************************************************
Error message
***********************************************************************************************************************************/
const char *
errorMessage()
{
    return errorContext.error.message;
}

/***********************************************************************************************************************************
Error name (pulled from error type)
***********************************************************************************************************************************/
const char *
errorName()
{
    return errorTypeName(errorType());
}

/***********************************************************************************************************************************
Is this error an instance of the error type?
***********************************************************************************************************************************/
bool
errorInstanceOf(const ErrorType *errorTypeTest)
{
    return errorType() == errorTypeTest || errorTypeExtends(errorType(), errorTypeTest);
}

/***********************************************************************************************************************************
Return current error context state
***********************************************************************************************************************************/
static ErrorState
errorInternalState()
{
    return errorContext.tryList[errorContext.tryTotal].state;
}

/***********************************************************************************************************************************
True when in try state
***********************************************************************************************************************************/
bool
errorInternalStateTry()
{
    return errorInternalState() == errorStateTry;
}

/***********************************************************************************************************************************
True when in catch state and the expected error matches
***********************************************************************************************************************************/
bool
errorInternalStateCatch(const ErrorType *errorTypeCatch)
{
    return errorInternalState() == errorStateCatch && errorInstanceOf(errorTypeCatch) && errorInternalProcess(true);
}

/***********************************************************************************************************************************
True when in final state
***********************************************************************************************************************************/
bool
errorInternalStateFinal()
{
    return errorInternalState() == errorStateFinal;
}

/***********************************************************************************************************************************
Return jump buffer for current try
***********************************************************************************************************************************/
jmp_buf *
errorInternalJump()
{
    return &errorContext.jumpList[errorContext.tryTotal - 1];
}

/***********************************************************************************************************************************
Begin the try block
***********************************************************************************************************************************/
bool
errorInternalTry(const char *fileName, int fileLine)
{
    // If try total has been exceeded then throw an error
    if (errorContext.tryTotal >= ERROR_TRY_MAX)
        errorInternalThrowFmt(&AssertError, fileName, fileLine, "too many nested try blocks");

    // Increment try total
    errorContext.tryTotal++;

    // Setup try
    errorContext.tryList[errorContext.tryTotal].state = errorStateBegin;
    errorContext.tryList[errorContext.tryTotal].uncaught = false;

    // Try setup was successful
    return true;
}

/***********************************************************************************************************************************
Propogate the error up so it can be caught
***********************************************************************************************************************************/
void
errorInternalPropagate()
{
    // Mark the error as uncaught
    errorContext.tryList[errorContext.tryTotal].uncaught = true;

    // If there is a parent try then jump to it
    if (errorContext.tryTotal > 0)
        longjmp(errorContext.jumpList[errorContext.tryTotal - 1], 1);

    // If there was no try to catch this error then output to stderr
    if (fprintf(                                                    // {uncovered - output to stderr is a problem for test harness}
            stderr, "\nUncaught %s: %s\n    thrown at %s:%d\n\n",
            errorName(), errorMessage(), errorFileName(), errorFileLine()) > 0)
    {
        fflush(stderr);                                                                                             // {+uncovered}
    }

    // Exit with failure
    exit(EXIT_FAILURE);                                             // {uncovered - exit failure is a problem for test harness}
}

/***********************************************************************************************************************************
Process the error through each try and state
***********************************************************************************************************************************/
bool
errorInternalProcess(bool catch)
{
    // If a catch statement then return
    if (catch)
    {
        errorContext.tryList[errorContext.tryTotal].uncaught = false;
        return true;
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

    // If not caught in the last try then propogate
    if (errorContext.tryList[errorContext.tryTotal + 1].uncaught)
        errorInternalPropagate();

    // Nothing left to process
    return false;
}

/***********************************************************************************************************************************
Throw an error
***********************************************************************************************************************************/
void
errorInternalThrow(const ErrorType *errorType, const char *fileName, int fileLine, const char *message)
{
    // Setup error data
    errorContext.error.errorType = errorType;
    errorContext.error.fileName = fileName;
    errorContext.error.fileLine = fileLine;

    // Assign message to the error
    strcpy(messageBuffer, message);
    errorContext.error.message = (const char *)messageBuffer;

    // Propogate the error
    errorInternalPropagate();
}

void
errorInternalThrowFmt(const ErrorType *errorType, const char *fileName, int fileLine, const char *format, ...)
{
    // Format message
    va_list argument;
    va_start(argument, format);
    vsnprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, format, argument);
    va_end(argument);

    errorInternalThrow(errorType, fileName, fileLine, messageBufferTemp);
}

/***********************************************************************************************************************************
Throw a system error
***********************************************************************************************************************************/
void
errorInternalThrowSys(int errNo, const ErrorType *errorType, const char *fileName, int fileLine, const char *message)
{
    // Setup error data
    errorContext.error.errorType = errorType;
    errorContext.error.fileName = fileName;
    errorContext.error.fileLine = fileLine;

    // Append the system message
    snprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, "%s: [%d] %s", message, errNo, strerror(errNo));

    // Assign message to the error
    strcpy(messageBuffer, messageBufferTemp);
    errorContext.error.message = (const char *)messageBuffer;

    // Propogate the error
    errorInternalPropagate();
}

void
errorInternalThrowSysFmt(int errNo, const ErrorType *errorType, const char *fileName, int fileLine, const char *format, ...)
{
    // Setup error data
    errorContext.error.errorType = errorType;
    errorContext.error.fileName = fileName;
    errorContext.error.fileLine = fileLine;

    // Format message
    va_list argument;
    va_start(argument, format);
    size_t messageSize = (size_t)vsnprintf(messageBufferTemp, ERROR_MESSAGE_BUFFER_SIZE - 1, format, argument);
    va_end(argument);

    // Append the system message
    snprintf(messageBufferTemp + messageSize, ERROR_MESSAGE_BUFFER_SIZE - 1 - messageSize, ": [%d] %s", errNo, strerror(errNo));

    // Assign message to the error
    strcpy(messageBuffer, messageBufferTemp);
    errorContext.error.message = (const char *)messageBuffer;

    // Propogate the error
    errorInternalPropagate();
}
