/***********************************************************************************************************************************
Error Handler

Implement a try ... catch ... finally error handler.

TRY()
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

The CATCH() and FINALLY() blocks are optional but at least one must be specified.  There is no need for an TRY block by itself
because errors will automatically be propagated to the nearest try block in the call stack.

Never call return from within any of the error-handling blocks.
***********************************************************************************************************************************/
#ifndef ERROR_H
#define ERROR_H

#include <setjmp.h>

#include "common/errorType.h"
#include "common/type.h"

/***********************************************************************************************************************************
Error accessor functions
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
#define TRY()                                                                                                                      \
    if (errorInternalTry(__FILE__, __LINE__) && setjmp(*errorInternalJump()) >= 0)                                                 \
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
Throw an error

Errors can be thrown any time, but if there is no TRY block somewhere in the call stack then the program will exit and print the
error information to stderr.
***********************************************************************************************************************************/
#define THROW(errorType, ...)                                                                                                      \
    errorInternalThrow(&errorType, __FILE__, __LINE__, __VA_ARGS__)

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
void errorInternalPropagate();
void errorInternalThrow(const ErrorType *errorType, const char *fileName, int fileLine, const char *format, ...);

#endif
