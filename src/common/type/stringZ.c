/***********************************************************************************************************************************
Zero-Terminated String Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/stringZ.h"

/**********************************************************************************************************************************/
FV_EXTERN char *
zNewFmt(const char *const format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(format != NULL);

    // Determine how long the allocated string needs to be and create object
    va_list argumentList;
    va_start(argumentList, format);
    const size_t size = (size_t)vsnprintf(NULL, 0, format, argumentList) + 1;
    char *const result = memNew(size);
    va_end(argumentList);

    // Format string
    va_start(argumentList, format);
    vsnprintf(result, size, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RETURN(STRINGZ, result);
}
