/***********************************************************************************************************************************
Variable Parameter Helper Macros
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_PARAM_H
#define COMMON_TYPE_PARAM_H

/***********************************************************************************************************************************
Macros to help with implementing functions with variable parameters using structs
***********************************************************************************************************************************/
// This macro goes at the top of the parameter struct
#define VAR_PARAM_HEADER                                                                                                           \
    bool dummy

// This macro goes in the struct parameter list right before __VA_ARGS__
#define VAR_PARAM_INIT                                                                                                             \
    .dummy = false

#endif
