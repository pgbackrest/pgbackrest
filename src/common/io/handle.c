/***********************************************************************************************************************************
Handle IO
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/error.h"
#include "common/io/handle.h"

/***********************************************************************************************************************************
Write a string to the specified handle
***********************************************************************************************************************************/
void
ioHandleWriteOneStr(int handle, const String *string)
{
    if (write(handle, strPtr(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write to %zu byte(s) to handle", strSize(string));
}
