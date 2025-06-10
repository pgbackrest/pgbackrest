/***********************************************************************************************************************************
C Debug Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_DEBUG_H
#define TEST_COMMON_HARNESS_DEBUG_H

#ifdef HRN_FEATURE_DEBUG

#include "common/debug.h"

// Set line number of the current function in the stack trace. This is used to give more detailed info about which test macro caused
// an error.
#ifdef DEBUG
#define FUNCTION_HARNESS_STACK_TRACE_LINE_SET(lineNo)                                                                              \
        stackTraceTestFileLineSet((unsigned int)lineNo)
#else
#define FUNCTION_HARNESS_STACK_TRACE_LINE_SET(lineNo)
#endif

#define FUNCTION_HARNESS_BEGIN()                                                                                                   \
    STACK_TRACE_PUSH(logLevelDebug);                                                                                               \
    stackTraceParamLog()

#define FUNCTION_HARNESS_PARAM(typeMacroPrefix, param)                                                                             \
    FUNCTION_LOG_PARAM(typeMacroPrefix, param)

#define FUNCTION_HARNESS_PARAM_P(typeMacroPrefix, param)                                                                           \
    FUNCTION_LOG_PARAM_P(typeMacroPrefix, param)

#define FUNCTION_HARNESS_PARAM_PP(typeMacroPrefix, param)                                                                          \
    FUNCTION_LOG_PARAM_PP(typeMacroPrefix, param)

#define FUNCTION_HARNESS_END()

#define FUNCTION_HARNESS_VOID()                                                                                                    \
    FUNCTION_HARNESS_BEGIN();                                                                                                      \
    FUNCTION_HARNESS_END()

#define FUNCTION_HARNESS_ASSERT(condition)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "function harness assertion '%s' failed", #condition);                                          \
    }                                                                                                                              \
    while (0)

#define FUNCTION_HARNESS_RETURN(typeMacroPrefix, ...)                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        STACK_TRACE_POP(false);                                                                                                    \
        return __VA_ARGS__;                                                                                                        \
    }                                                                                                                              \
    while (0)

#define FUNCTION_HARNESS_RETURN_STRUCT(...)                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        STACK_TRACE_POP(false);                                                                                                    \
        return __VA_ARGS__;                                                                                                        \
    }                                                                                                                              \
    while (0)

#define FUNCTION_HARNESS_RETURN_VOID()                                                                                             \
        STACK_TRACE_POP(false);

#else

#define FUNCTION_HARNESS_STACK_TRACE_LINE_SET(lineNo)
#define FUNCTION_HARNESS_BEGIN()
#define FUNCTION_HARNESS_PARAM(typeMacroPrefix, param)
#define FUNCTION_HARNESS_PARAM_P(typeMacroPrefix, param)
#define FUNCTION_HARNESS_PARAM_PP(typeMacroPrefix, param)
#define FUNCTION_HARNESS_END()
#define FUNCTION_HARNESS_VOID()
#define FUNCTION_HARNESS_ASSERT(condition)

#define FUNCTION_HARNESS_RETURN(typeMacroPrefix, ...)                                                                              \
    return __VA_ARGS__

#define FUNCTION_HARNESS_RETURN_VOID();

#endif

#endif
