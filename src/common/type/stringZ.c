/***********************************************************************************************************************************
Zero-Terminated String Handler
***********************************************************************************************************************************/
#include <build.h>

#include <stdarg.h>
#include <stdio.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/stringId.h"
#include "common/type/stringZ.h"

/**********************************************************************************************************************************/
static char *
zNewInternal(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate the string as extra or a separate allocation based on how large it is
    char *result;

    OBJ_NEW_BASE_EXTRA_BEGIN(
        char *, size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? 0 : (uint16_t)size, .allocQty = size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? 1 : 0)
    {
        result = size > MEM_CONTEXT_ALLOC_EXTRA_MAX ? memNew(size) : OBJ_NEW_ALLOC();
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

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

    // Format string
    char *const result = zNewInternal(size);

    va_start(argumentList, format);
    vsnprintf(result, size, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

/**********************************************************************************************************************************/
FN_EXTERN char *
zNewStrId(const StringId strId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, strId);
    FUNCTION_TEST_END();

    ASSERT(strId != 0);
    ASSERT(MEM_CONTEXT_ALLOC_EXTRA_MAX >= STRID_MAX + 1);

    char *const result = zNewInternal(STRID_MAX + 1);
    result[strIdToZN(strId, result)] = '\0';

    FUNCTION_TEST_RETURN(STRINGZ, result);
}
