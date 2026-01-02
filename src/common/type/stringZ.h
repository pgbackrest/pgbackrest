/***********************************************************************************************************************************
Zero-Terminated String Handler

These functions are shortcuts for creating/modifying zero-terminated strings without needing to wrap a strNew*() call in strZ().
The benefit is simpler code with less indentation, but be aware that memory is still being allocated.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGZ_H
#define COMMON_TYPE_STRINGZ_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Format a zero-terminated string
FN_EXTERN FN_PRINTF(1, 2) char *zNewFmt(const char *format, ...);

#endif
