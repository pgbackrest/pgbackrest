/***********************************************************************************************************************************
General Macros
***********************************************************************************************************************************/
#ifndef COMMON_MACRO_H
#define COMMON_MACRO_H

/***********************************************************************************************************************************
Convert the parameter to a zero-terminated string

Useful for converting non-string types (e.g. int) to strings for inclusion in messages.
***********************************************************************************************************************************/
#define STRINGIFY_HELPER(param)                                     #param
#define STRINGIFY(param)                                            STRINGIFY_HELPER(param)

/***********************************************************************************************************************************
Glue together a string/macro and another string//macro

Useful for creating function names when one or both of the macro parameter needs to be converted to a macro before concatenating.
common/type/object.h has numerous examples of this.
***********************************************************************************************************************************/
#define GLUE_HELPER(param1, param2)                                 param1##param2
#define GLUE(param1, param2)                                        GLUE_HELPER(param1, param2)

/***********************************************************************************************************************************
If param2 > param1 then assign it to param1

Useful for ensuring coverage in cases where compared values may be always ascending or descending.
***********************************************************************************************************************************/
#define MAX_ASSIGN(param1, param2)                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        if (param2 > param1)                                                                                                       \
            param1 = param2;                                                                                                       \
    }                                                                                                                              \
    while (0)

#endif
