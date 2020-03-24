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
#ifndef NDEBUG
    ERROR_DECLARE(TestError);
#endif

/***********************************************************************************************************************************
Functions to get information about a generic error type
***********************************************************************************************************************************/
int errorTypeCode(const ErrorType *errorType);
const ErrorType *errorTypeFromCode(int code);
const char *errorTypeName(const ErrorType *errorType);
const ErrorType *errorTypeParent(const ErrorType *errorType);
bool errorTypeExtends(const ErrorType *child, const ErrorType *parent);

/***********************************************************************************************************************************
Functions to get information about the current error
***********************************************************************************************************************************/
const ErrorType *errorType(void);
int errorCode(void);
const char *errorFileName(void);
const char *errorFunctionName(void);
int errorFileLine(void);
bool errorInstanceOf(const ErrorType *errorTypeTest);
const char *errorMessage(void);
const char *errorName(void);
const char *errorStackTrace(void);

/***********************************************************************************************************************************
Functions to get information about the try stack
***********************************************************************************************************************************/
unsigned int errorTryDepth(void);

/***********************************************************************************************************************************
Begin a block where errors can be thrown
***********************************************************************************************************************************/
#define TRY_BEGIN()                                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        if (errorInternalTry(__FILE__, __func__, __LINE__) && setjmp(*errorInternalJump()) >= 0)                                   \
        {                                                                                                                          \
            while (errorInternalProcess(false))                                                                                    \
            {                                                                                                                      \
                if (errorInternalStateTry())

/***********************************************************************************************************************************
Catch a specific error thrown in the try block
***********************************************************************************************************************************/
#define CATCH(errorTypeCatch)                                                                                                      \
                else if (errorInternalStateCatch(&errorTypeCatch))

/***********************************************************************************************************************************
Catch any error thrown in the try block
***********************************************************************************************************************************/
#define CATCH_ANY()                                                                                                                \
                else if (errorInternalStateCatch(&RuntimeError))

/***********************************************************************************************************************************
Code to run whether the try block was successful or not
***********************************************************************************************************************************/
#define FINALLY()                                                                                                                  \
                else if (errorInternalStateFinal())

/***********************************************************************************************************************************
End the try block
***********************************************************************************************************************************/
#define TRY_END()                                                                                                                  \
            }                                                                                                                      \
        }                                                                                                                          \
    } while (0)

/***********************************************************************************************************************************
Throw an error

Errors can be thrown any time, but if there is no TRY block somewhere in the call stack then the program will exit and print the
error information to stderr.

The seldom used "THROWP" variants allow an error to be thrown with a pointer to the error type.
***********************************************************************************************************************************/
#define THROW(errorType, message)                                                                                                  \
    errorInternalThrow(&errorType, __FILE__, __func__, __LINE__, message)
#define THROW_FMT(errorType, ...)                                                                                                  \
    errorInternalThrowFmt(&errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define THROWP(errorType, message)                                                                                                 \
    errorInternalThrow(errorType, __FILE__, __func__, __LINE__, message)
#define THROWP_FMT(errorType, ...)                                                                                                 \
    errorInternalThrowFmt(errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define THROW_CODE(errorCode, message)                                                                                             \
    errorInternalThrow(errorTypeFromCode(errorCode), __FILE__, __func__, __LINE__, message)
#define THROW_CODE_FMT(errorCode, ...)                                                                                             \
    errorInternalThrowFmt(errorTypeFromCode(errorCode), __FILE__, __func__, __LINE__, __VA_ARGS__)

/***********************************************************************************************************************************
Throw an error when a system call fails
***********************************************************************************************************************************/
// When coverage testing define special versions of the macros that don't contain branches.  These macros are less efficient because
// they need to call errorInternalThrowSys*() before determining if there is an error or not, but they allow coverage testing for
// THROW*_ON*() calls that contain conditionals.
#ifdef DEBUG_COVERAGE
    #define THROW_SYS_ERROR(errorType, message)                                                                                    \
        errorInternalThrowSys(true, &errorType, __FILE__, __func__, __LINE__, message)
    #define THROW_SYS_ERROR_FMT(errorType, ...)                                                                                    \
        errorInternalThrowSysFmt(true, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
    #define THROWP_SYS_ERROR(errorType, message)                                                                                   \
        errorInternalThrowSys(true, errorType, __FILE__, __func__, __LINE__, message)
    #define THROWP_SYS_ERROR_FMT(errorType, ...)                                                                                   \
        errorInternalThrowSysFmt(true, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

    #define THROW_ON_SYS_ERROR(error, errorType, message)                                                                          \
        errorInternalThrowSys(error, &errorType, __FILE__, __func__, __LINE__, message)

    #define THROW_ON_SYS_ERROR_FMT(error, errorType, ...)                                                                          \
        errorInternalThrowSysFmt(error, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

    #define THROWP_ON_SYS_ERROR(error, errorType, message)                                                                         \
        errorInternalThrowSys(error, errorType, __FILE__, __func__, __LINE__, message)

    #define THROWP_ON_SYS_ERROR_FMT(error, errorType, ...)                                                                         \
        errorInternalThrowSysFmt(error, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

// Else define the normal macros which check for an error first
#else
    #define THROW_SYS_ERROR(errorType, message)                                                                                    \
        errorInternalThrowSys(errno, &errorType, __FILE__, __func__, __LINE__, message)
    #define THROW_SYS_ERROR_FMT(errorType, ...)                                                                                    \
        errorInternalThrowSysFmt(errno, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
    #define THROWP_SYS_ERROR(errorType, message)                                                                                   \
        errorInternalThrowSys(errno, errorType, __FILE__, __func__, __LINE__, message)
    #define THROWP_SYS_ERROR_FMT(errorType, ...)                                                                                   \
        errorInternalThrowSysFmt(errno, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

    #define THROW_ON_SYS_ERROR(error, errorType, message)                                                                          \
        do                                                                                                                         \
        {                                                                                                                          \
            if (error)                                                                                                             \
                errorInternalThrowSys(errno, &errorType, __FILE__, __func__, __LINE__, message);                                   \
        } while(0)

    #define THROW_ON_SYS_ERROR_FMT(error, errorType, ...)                                                                          \
        do                                                                                                                         \
        {                                                                                                                          \
            if (error)                                                                                                             \
                errorInternalThrowSysFmt(errno, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                            \
        } while(0)

    #define THROWP_ON_SYS_ERROR(error, errorType, message)                                                                         \
        do                                                                                                                         \
        {                                                                                                                          \
            if (error)                                                                                                             \
                errorInternalThrowSys(errno, errorType, __FILE__, __func__, __LINE__, message);                                    \
        } while(0)

    #define THROWP_ON_SYS_ERROR_FMT(error, errorType, ...)                                                                         \
        do                                                                                                                         \
        {                                                                                                                          \
            if (error)                                                                                                             \
                errorInternalThrowSysFmt(errno, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__);                             \
        } while(0)
#endif

#define THROW_SYS_ERROR_CODE(errNo, errorType, message)                                                                            \
    errorInternalThrowSys(errNo, &errorType, __FILE__, __func__, __LINE__, message)
#define THROW_SYS_ERROR_CODE_FMT(errNo, errorType, ...)                                                                            \
    errorInternalThrowSysFmt(errNo, &errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define THROWP_SYS_ERROR_CODE(errNo, errorType, message)                                                                           \
    errorInternalThrowSys(errNo, errorType, __FILE__, __func__, __LINE__, message)
#define THROWP_SYS_ERROR_CODE_FMT(errNo, errorType, ...)                                                                           \
    errorInternalThrowSysFmt(errNo, errorType, __FILE__, __func__, __LINE__, __VA_ARGS__)

/***********************************************************************************************************************************
Rethrow the current error
***********************************************************************************************************************************/
#define RETHROW()                                                                                                                  \
    errorInternalPropagate()

/***********************************************************************************************************************************
Internal functions

These functions are used by the macros to implement the error handler and should never be called independently.
***********************************************************************************************************************************/
bool errorInternalTry(const char *fileName, const char *functionName, int fileLine);
jmp_buf *errorInternalJump(void);
bool errorInternalStateTry(void);
bool errorInternalStateCatch(const ErrorType *errorTypeCatch);
bool errorInternalStateFinal(void);
bool errorInternalProcess(bool catch);
void errorInternalPropagate(void) __attribute__((__noreturn__));
void errorInternalThrow(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
    __attribute__((__noreturn__));
void errorInternalThrowFmt(
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 5, 6))) __attribute__((__noreturn__));

void errorInternalThrowSys(
#ifdef DEBUG_COVERAGE
    bool error,
#else
    int errNo,
#endif
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *message)
#ifdef DEBUG_COVERAGE
    ;
#else
    __attribute__((__noreturn__));
#endif

void errorInternalThrowSysFmt(
#ifdef DEBUG_COVERAGE
    bool error,
#else
    int errNo,
#endif
    const ErrorType *errorType, const char *fileName, const char *functionName, int fileLine, const char *format, ...)
    __attribute__((format(printf, 6, 7)))
#ifdef DEBUG_COVERAGE
    ;
#else
    __attribute__((__noreturn__));
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_ERROR_TYPE_TYPE                                                                                               \
    ErrorType *
#define FUNCTION_LOG_ERROR_TYPE_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "ErrorType", buffer, bufferSize)

#endif
