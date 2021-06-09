/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_ERROR_H
#define TEST_COMMON_HARNESS_ERROR_H

#include "common/error.h"
#include "common/type/param.h"

/***********************************************************************************************************************************
Generate a test error

!!!
***********************************************************************************************************************************/
typedef struct HrnErrorThrowParam
{
    VAR_PARAM_HEADER;
    const ErrorType *errorType;                                     // Error type (defaults to AssertError)
    const char *fileName;                                           // Source file where the error occurred (defaults to ERR_FILE)
    const char *functionName;                                       // Function where the error occurred (defaults to ERR_FUNCTION)
    int fileLine;                                                   // Source file line where the error occurred (defaults to 999)
    const char *message;                                            // Description of the error (defaults to ERR_MESSAGE)
    const char *stackTrace;                                         // Stack trace (defaults to ERR_STACK_TRACE)
} HrnErrorThrowParam;

#define hrnErrorThrowP(...)                                                                                                        \
    hrnErrorThrow((HrnErrorThrowParam){VAR_PARAM_INIT, __VA_ARGS__})

void hrnErrorThrow(const HrnErrorThrowParam param);

#endif
