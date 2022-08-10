/***********************************************************************************************************************************
Error Handler

Implement a try ... catch ... finally error handler.

TRY_BEGIN()
{
    <Do something that might throw an error>
}
CATCH(Error1)
{
    <Handle Error1>
}
CATCH(Error2)
{
    <Handle Error2>
}
CATCH_ANY()
{
    <Handle errors that are not Error1 or Error2>
}
FINALLY()
{
    <Cleanup code that runs in all cases>
}
TRY_END();

The CATCH() and FINALLY() blocks are optional but at least one must be specified.  There is no need for a TRY block by itself
because errors will automatically be propagated to the nearest try block in the call stack.

IMPORTANT: If a local variable of the function containing a TRY block is modified in the TRY_BEGIN() block and used later in the
function after an error is thrown, that variable must be declared "volatile" if the preserving the value is important.  Beware that
gcc's -Wclobbered warnings are almost entirely useless for catching such issues.

IMPORTANT: Never call return from within any of the error-handling blocks.
***********************************************************************************************************************************/
#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>

/***********************************************************************************************************************************
Error type object
***********************************************************************************************************************************/
typedef struct ErrorType ErrorType;

// Macro for declaring new error types
#define ERROR_DECLARE(name)                                                                                                        \
    extern const ErrorType name

// Include error type declarations
#include "common/error.auto.h"

// Declare test error
#ifdef DEBUG
    ERROR_DECLARE(TestError);
#else
    // Must always be defined since it might be needed to compile (though not used) during profiling
    #define TestError AssertError
#endif

/***********************************************************************************************************************************
Functions to get information about a generic error type
***********************************************************************************************************************************/
// Error type code
int errorTypeCode(const ErrorType *errorType);

// Is the error type fatal
bool errorTypeFatal(const ErrorType *errorType);

// Get error type using a code
const ErrorType *errorTypeFromCode(int code);

// Error type name
const char *errorTypeName(const ErrorType *errorType);

// Error type parent
const ErrorType *errorTypeParent(const ErrorType *errorType);

// Does the child error type extend the parent error type?
bool errorTypeExtends(const ErrorType *child, const ErrorType *parent);

/***********************************************************************************************************************************
Functions to get information about the current error within a CATCH() block. Invalid outside a CATCH() block.
***********************************************************************************************************************************/
// Error type
const ErrorType *errorType(void);

// Error code (pulled from error type)
int errorCode(void);

// Is the error fatal?
bool errorFatal(void);

// Error filename
const char *errorFileName(void);

// Error function name
const char *errorFunctionName(void);

// Error file line number
int errorFileLine(void);

// Is this error an instance of the error type?
bool errorInstanceOf(const ErrorType *errorTypeTest);

// Error message
const char *errorMessage(void);

// Error name (pulled from error type)
const char *errorName(void);

// Error stack trace
const char *errorStackTrace(void);

/***********************************************************************************************************************************
Try stack getters/setters
***********************************************************************************************************************************/
// Get the depth of the current try statement (0 if none)
unsigned int errorTryDepth(void);

// Add a handler to be called when an error occurs
typedef struct ErrorHandler
{
    void (*const function)(unsigned int);
} ErrorHandler;

void errorHandlerSet(const ErrorHandler *list, unsigned int total);

/***********************************************************************************************************************************
Begin a block where errors can be thrown
***********************************************************************************************************************************/
#define TRY_BEGIN()                                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        volatile bool TRY_finally = false;                                                                                         \
        (void)TRY_finally;                                          /* Will be unused if there is no finally block */              \
                                                                                                                                   \
        errorInternalTryBegin(__FILE__, __func__, __LINE__);                                                                       \
                                                                                                                                   \
        if (setjmp(*errorInternalJump()) == 0)                                                                                     \
        {

/***********************************************************************************************************************************
Catch a specific error thrown in the try block
***********************************************************************************************************************************/
#define CATCH(errorTypeCatch)                                                                                                      \
        }                                                                                                                          \
        else if (errorInternalCatch(&errorTypeCatch, true))                                                                        \
        {

/***********************************************************************************************************************************
Catch any non-fatal error thrown in the try block
***********************************************************************************************************************************/
#define CATCH_ANY()                                                                                                                \
        }                                                                                                                          \
        else if (errorInternalCatch(&RuntimeError, false))                                                                         \
        {

/***********************************************************************************************************************************
Catch any error thrown in the try block
***********************************************************************************************************************************/
#define CATCH_FATAL()                                                                                                              \
        }                                                                                                                          \
        else if (errorInternalCatch(&RuntimeError, true))                                                                          \
        {

/***********************************************************************************************************************************
Code to run whether the try block was successful or not
***********************************************************************************************************************************/
#define FINALLY()                                                                                                                  \
        }                                                                                                                          \
                                                                                                                                   \
        if (!TRY_finally)                                                                                                          \
        {                                                                                                                          \
            TRY_finally = true;

/***********************************************************************************************************************************
End the try block
***********************************************************************************************************************************/
#define TRY_END()                                                                                                                  \
        }                                                                                                                          \
                                                                                                                                   \
        errorInternalTryEnd();                                                                                                     \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Throw an error

Errors can be thrown any time, but if there is no TRY block somewhere in the call stack then the program will exit and print the
error information to stderr.

The seldom used "THROWP" variants allow an error to be thrown with a pointer to the error type.
***********************************************************************************************************************************/
#define THROW(errorType, message)                                                                                                  \
    errorInternalThrow(&errorType, __FILE__, __func__, __LINE__, message, NULL)
#define THROW_FMT(errorType, ...)                                                                                                  \
    errorInternalThrowFmt(&errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define THROWP(errorType, message)                                                                                                 \
    errorInternalThrow(errorType, __FILE__, __func__, __LINE__, message, NULL)
#define THROWP_FMT(errorType, ...)                                                                                                 \
    errorInternalThrowFmt(errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define THROW_CODE(errorCode, message)                                                                                             \
    errorInternalThrow(errorTypeFromCode(errorCode), __FILE__, __func__, __LINE__, message, NULL)
#define THROW_CODE_FMT(errorCode, ...)                                                                                             \
    errorInternalThrowFmt(errorTypeFromCode(errorCode), __FILE__, __func__, __LINE__, __VA_ARGS__)

/***********************************************************************************************************************************
Throw an error when a system call fails
***********************************************************************************************************************************/
#define THROW_SYS_ERROR(errorType, message)                                                                                        \
    errorInternalThrowSys(errno, &errorType, __FILE__, __func__, __LINE__, message)
#define THROW_SYS_ERROR_FMT(errorType, ...)                                                                                        \
    errorInternalThrowSysFmt(errno, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define THROWP_SYS_ERROR(errorType, message)                                                                                       \
    errorInternalThrowSys(errno, errorType, __FILE__, __func__, __LINE__, message)
#define THROWP_SYS_ERROR_FMT(errorType, ...)                                                                                       \
    errorInternalThrowSysFmt(errno, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define THROW_SYS_ERROR_CODE(errNo, errorType, message)                                                                            \
    errorInternalThrowSys(errNo, &errorType, __FILE__, __func__, __LINE__, message)
#define THROW_SYS_ERROR_CODE_FMT(errNo, errorType, ...)                                                                            \
    errorInternalThrowSysFmt(errNo, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define THROWP_SYS_ERROR_CODE(errNo, errorType, message)                                                                           \
    errorInternalThrowSys(errNo, errorType, __FILE__, __func__, __LINE__, message)
#define THROWP_SYS_ERROR_CODE_FMT(errNo, errorType, ...)                                                                           \
    errorInternalThrowSysFmt(errNo, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

// When coverage testing, define special versions of the macros that don't contain branches. These macros are less efficient because
// they need to call errorInternalThrowOnSys*() before determining if there is an error or not, but they allow coverage testing for
// THROW*_ON*() calls that contain conditionals.
#ifdef DEBUG_COVERAGE

    // The expression can't be passed directly to errorInternalThrowSys*() because we need to be sure it is evaluated before passing
    // errno. Depending on optimization that might not happen.
    #define THROW_ON_SYS_ERROR(expression, errorType, message)                                                                     \
        do                                                                                                                         \
        {                                                                                                                          \
            bool error = expression;                                                                                               \
            errorInternalThrowOnSys(error, errno, &errorType, __FILE__, __func__, __LINE__, message);                              \
        } while (0)

    #define THROW_ON_SYS_ERROR_FMT(expression, errorType, ...)                                                                     \
        do                                                                                                                         \
        {                                                                                                                          \
            bool error = expression;                                                                                               \
            errorInternalThrowOnSysFmt(error, errno, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                       \
        } while (0)

    #define THROWP_ON_SYS_ERROR(expression, errorType, message)                                                                    \
        do                                                                                                                         \
        {                                                                                                                          \
            bool error = expression;                                                                                               \
            errorInternalThrowOnSys(error, errno, errorType, __FILE__, __func__, __LINE__, message);                               \
        } while (0)

    #define THROWP_ON_SYS_ERROR_FMT(expression, errorType, ...)                                                                    \
        do                                                                                                                         \
        {                                                                                                                          \
            bool error = expression;                                                                                               \
            errorInternalThrowOnSysFmt(error, errno, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                        \
        } while (0)

// Else define the normal macros which check for an error first
#else
    #define THROW_ON_SYS_ERROR(expression, errorType, message)                                                                     \
        do                                                                                                                         \
        {                                                                                                                          \
            if (expression)                                                                                                        \
                errorInternalThrowSys(errno, &errorType, __FILE__, __func__, __LINE__, message);                                   \
        } while (0)

    #define THROW_ON_SYS_ERROR_FMT(expression, errorType, ...)                                                                     \
        do                                                                                                                         \
        {                                                                                                                          \
            if (expression)                                                                                                        \
                errorInternalThrowSysFmt(errno, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                            \
        } while (0)

    #define THROWP_ON_SYS_ERROR(expression, errorType, message)                                                                    \
        do                                                                                                                         \
        {                                                                                                                          \
            if (expression)                                                                                                        \
                errorInternalThrowSys(errno, errorType, __FILE__, __func__, __LINE__, message);                                    \
        } while (0)

    #define THROWP_ON_SYS_ERROR_FMT(expression, errorType, ...)                                                                    \
        do                                                                                                                         \
        {                                                                                                                          \
            if (expression)                                                                                                        \
                errorInternalThrowSysFmt(errno, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                             \
        } while (0)
#endif

/***********************************************************************************************************************************
Rethrow the current error
***********************************************************************************************************************************/
#define RETHROW()                                                                                                                  \
    errorInternalPropagate()

/***********************************************************************************************************************************
Internal functions

These functions are used by the macros to implement the error handler and should never be called independently.
***********************************************************************************************************************************/
// Begin the try block
void errorInternalTryBegin(const char *fileName, const char *functionName, int fileLine);

// Return jump buffer for current try
jmp_buf *errorInternalJump(void);

// True when in catch state and the expected error matches
bool errorInternalCatch(const ErrorType *errorTypeCatch, bool fatalCatch);

// Propagate the error up so it can be caught
FN_NO_RETURN void errorInternalPropagate(void);

// End the try block
void errorInternalTryEnd(void);

// Throw an error
FN_NO_RETURN void errorInternalThrow(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message,
    const char *stackTrace);
FN_NO_RETURN void errorInternalThrowFmt(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 5, 6)));

// Throw a system error
FN_NO_RETURN void errorInternalThrowSys(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message);

// Throw a formatted system error
FN_NO_RETURN void errorInternalThrowSysFmt(
    int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 6, 7)));

// Versions of the above for coverage testing which checks the error condition inside the function
#ifdef DEBUG_COVERAGE
    void errorInternalThrowOnSys(
        bool error, int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine,
        const char *message);

    void errorInternalThrowOnSysFmt(
        bool error, int errNo, const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine,
        const char *format, ...) __attribute__((format(printf, 7, 8)));
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_ERROR_TYPE_TYPE                                                                                               \
    ErrorType *
#define FUNCTION_LOG_ERROR_TYPE_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "ErrorType", buffer, bufferSize)

#endif
