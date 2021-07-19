/***********************************************************************************************************************************
Build Common
***********************************************************************************************************************************/
#ifndef BUILD_COMMON_COMMON_H
#define BUILD_COMMON_COMMON_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Block comments
***********************************************************************************************************************************/
#define COMMENT_BLOCK_BEGIN                                                                                                        \
    "/***************************************************************************************************************************" \
    "********"

#define COMMENT_BLOCK_END                                                                                                          \
    "****************************************************************************************************************************" \
    "*******/"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Format a #define with the value aligned at column 69
__attribute__((always_inline)) static inline String *
bldDefineRender(const String *const define, const String *const value)
{
    return strNewFmt("#define %s%*s%s", strZ(define), (int)(60 - strSize(define)), "", strZ(value));
}

// Format file header
__attribute__((always_inline)) static inline String *
bldHeader(const char *const module, const char *const description)
{
    return strNewFmt(
        COMMENT_BLOCK_BEGIN "\n"
        "%s\n"
        "\n"
        "Automatically generated by 'make build-%s' -- do not modify directly.\n"
        COMMENT_BLOCK_END "\n",
        description, module);
}

#endif
