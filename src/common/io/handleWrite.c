/***********************************************************************************************************************************
Handle IO Write
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/io/handleWrite.h"
#include "common/log.h"

/***********************************************************************************************************************************
Write a string to the specified handle
***********************************************************************************************************************************/
void
ioHandleWriteOneStr(int handle, const String *string)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, handle);
        FUNCTION_DEBUG_PARAM(STRING, string);

        FUNCTION_DEBUG_ASSERT(string != NULL);
    FUNCTION_DEBUG_END();

    if (write(handle, strPtr(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write %zu byte(s) to handle", strSize(string));

    FUNCTION_DEBUG_RESULT_VOID();
}
