/***********************************************************************************************************************************
Zero-Terminated String Handler

These functions are shortcuts for creating/modifying zero-terminated strings without needing to wrap a strNew*() call in strZ().
The benefit is simpler code with less indentation, but be aware that memory is still being allocated.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGZ_H
#define COMMON_TYPE_STRINGZ_H

#include "common/type/object.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Format a zero-terminated string
FN_EXTERN FN_PRINTF(1, 2) char *zNewFmt(const char *format, ...);

// Convert StringId to zero-terminated string
FN_EXTERN char *zNewStrId(StringId strId);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
zFree(char *const this)
{
    objFree(this);
}

#endif
