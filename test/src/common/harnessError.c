/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessError.h"
#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/**********************************************************************************************************************************/
void hrnErrorThrow(const HrnErrorThrowParam param)
{
    errorContext.error.errorType = param.errorType != NULL ? param.errorType : &AssertError;
    errorContext.error.fileName = param.fileName != NULL ? param.fileName : "ERR_FILE";
    errorContext.error.functionName = param.functionName != NULL ? param.functionName : "ERR_FUNCTION";
    errorContext.error.fileLine = param.fileLine != 0 ? param.fileLine : 999;
    errorContext.error.message = param.message != NULL ? param.message : "ERR_MESSAGE";
    errorContext.error.stackTrace = param.stackTrace != NULL ? param.stackTrace : "ERR_STACK_TRACE";
    errorInternalPropagate();
}
