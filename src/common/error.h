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
#ifndef ERROR_H
#define ERROR_H

#include <setjmp.h>

#include "common/type.h"

/***********************************************************************************************************************************
Error type object
***********************************************************************************************************************************/
typedef struct ErrorType ErrorType;

// Macro for declaring new error types
#define ERROR_DECLARE(name)                                                                                                        \
    extern const ErrorType name

// Include error type declarations
#include "common/error.auto.h"

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
const ErrorType *errorType();
int errorCode();
const char *errorFileName();
int errorFileLine();
const char *errorMessage();
const char *errorName();
bool errorInstanceOf(const ErrorType *errorTypeTest);

/***********************************************************************************************************************************
Begin a block where errors can be thrown
***********************************************************************************************************************************/
#define TRY_BEGIN()                                                                                                                \
do                                                                                                                                 \
{                                                                                                                                  \
    if (errorInternalTry(__FILE__, __LINE__) && setjmp(*errorInternalJump()) >= 0)                                                 \
    {                                                                                                                              \
        while (errorInternalProcess(false))                                                                                        \
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
    }                                                                                                                              \
} while (0)

/***********************************************************************************************************************************
Throw an error

Errors can be thrown any time, but if there is no TRY block somewhere in the call stack then the program will exit and print the
error information to stderr.
***********************************************************************************************************************************/
#define THROW(errorType, ...)                                                                                                      \
    errorInternalThrow(&errorType, __FILE__, __LINE__, __VA_ARGS__)

#define THROW_CODE(errorCode, ...)                                                                                                 \
    errorInternalThrow(errorTypeFromCode(errorCode), __FILE__, __LINE__, __VA_ARGS__)

/***********************************************************************************************************************************
Throw an error when a system call fails
***********************************************************************************************************************************/
#define THROW_ON_SYS_ERROR(result, errorType, ...)                                                                                 \
    errorInternalThrowSys(result, &errorType, __FILE__, __LINE__, __VA_ARGS__)

/***********************************************************************************************************************************
Rethrow the current error
***********************************************************************************************************************************/
#define RETHROW()                                                                                                                  \
    errorInternalPropagate()

/***********************************************************************************************************************************
Internal functions

These functions are used by the macros to implement the error handler and should never be called independently.
***********************************************************************************************************************************/
bool errorInternalTry(const char *fileName, int fileLine);
jmp_buf *errorInternalJump();
bool errorInternalStateTry();
bool errorInternalStateCatch(const ErrorType *errorTypeCatch);
bool errorInternalStateFinal();
bool errorInternalProcess(bool catch);
void errorInternalPropagate() __attribute__((__noreturn__));
void errorInternalThrow(
    const ErrorType *errorType, const char *fileName, int fileLine, const char *format, ...) __attribute__((__noreturn__));
void errorInternalThrowSys(int result, const ErrorType *errorType, const char *fileName, int fileLine, const char *format, ...);

#endif
