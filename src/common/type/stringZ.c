/***********************************************************************************************************************************
Zero-Terminated String Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/stringZ.h"

/**********************************************************************************************************************************/
FN_EXTERN char *
zNewFmt(const char *const format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(format != NULL);

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    const size_t size = (size_t)vsnprintf(NULL, 0, format, argumentList) + 1;
    va_end(argumentList);

    // Allocate the string as extra or a separate allocation based on how large it is
    char *result;

    OBJ_NEW_BASE_EXTRA_BEGIN(
        char *, size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? 0 : (uint16_t)size, .allocQty = size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? 1 : 0)
    {
        result = size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? memNew(size) : OBJ_NEW_ALLOC();
    }
    OBJ_NEW_END();

    // Format string
    va_start(argumentList, format);
    vsnprintf(result, size, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RETURN(STRINGZ, result);
}
