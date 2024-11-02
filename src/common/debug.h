/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "common/assert.h"
#include "common/stackTrace.h"
#include "common/type/convert.h"
#include "common/type/stringStatic.h"
#include "common/type/stringZ.h"

/***********************************************************************************************************************************
These functions allow auditing of child mem contexts and allocations that are left in the calling context when a function exits.
This helps find leaks, i.e. child mem contexts or allocations created in the calling context (and not freed) accidentally.

The FUNCTION_AUDIT_*() macros can be used to annotate functions that do that follow the default behavior, i.e. that a single value
is returned and that is the only value created in the calling context.
***********************************************************************************************************************************/
#if defined(DEBUG_MEM) && defined(DEBUG_TEST_TRACE)

#include "common/macro.h"
#include "common/memContext.h"

// Begin the audit
#define FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN()                                                                                    \
    MemContextAuditState MEM_CONTEXT_AUDIT_param = {.memContext = memContextCurrent()};                                            \
    memContextAuditBegin(&MEM_CONTEXT_AUDIT_param)

// End the audit
#define FUNCTION_TEST_MEM_CONTEXT_AUDIT_END(returnType)                                                                            \
    memContextAuditEnd(&MEM_CONTEXT_AUDIT_param, returnType)

// Allow any new mem contexts or allocations in the calling context. These should be fixed and this macro eventually removed.
#define FUNCTION_AUDIT_IF(condition)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            MEM_CONTEXT_AUDIT_param.returnTypeAny = true;                                                                          \
    }                                                                                                                              \
    while (0)

// Callbacks are difficult to audit so ignore them. Eventually they should all be removed.
#define FUNCTION_AUDIT_CALLBACK()                                   MEM_CONTEXT_AUDIT_param.returnTypeAny = true

// Helper function that creates new mem contexts or allocations in the calling context. These functions should be static (except
// for interface helpers) but it is not clear that anything else needs to be done.
#define FUNCTION_AUDIT_HELPER()                                     MEM_CONTEXT_AUDIT_param.returnTypeAny = true

// Function returns a struct that has new mem contexts or allocations in the calling context. Find a way to fix these.
#define FUNCTION_AUDIT_STRUCT()                                     MEM_CONTEXT_AUDIT_param.returnTypeAny = true

#else

#define FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN()
#define FUNCTION_TEST_MEM_CONTEXT_AUDIT_END(returnType)

#define FUNCTION_AUDIT_IF(condition)
#define FUNCTION_AUDIT_CALLBACK()
#define FUNCTION_AUDIT_HELPER()
#define FUNCTION_AUDIT_STRUCT()

#endif

/***********************************************************************************************************************************
Base function debugging macros

In debug mode parameters will always be recorded in the stack trace while in production mode they will only be recorded when the log
level is set to debug or trace.
***********************************************************************************************************************************/
#define FUNCTION_LOG_LEVEL()                                                                                                       \
    FUNCTION_LOG_logLevel

#ifdef DEBUG_TEST_TRACE

#define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                          \
    LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                    \
    FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN();                                                                                       \
                                                                                                                                   \
    {                                                                                                                              \
        stackTraceParamLog();                                                                                                      \
        stackTraceTestStop()

#define FUNCTION_LOG_END_BASE()                                                                                                    \
        stackTraceTestStart();                                                                                                     \
        LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                               \
    }

#else

#define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                          \
    LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                    \
    FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN();                                                                                       \
                                                                                                                                   \
    if (logAny(FUNCTION_LOG_LEVEL()))                                                                                              \
    {                                                                                                                              \
        stackTraceParamLog()

#define FUNCTION_LOG_END_BASE()                                                                                                    \
        LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                               \
    }

#endif

/***********************************************************************************************************************************
General purpose function debugging macros

FUNCTION_LOG_VOID() is provided as a shortcut for functions that have no parameters.
***********************************************************************************************************************************/
#define FUNCTION_LOG_BEGIN(logLevel)                                                                                               \
    FUNCTION_LOG_BEGIN_BASE(logLevel)

#define FUNCTION_LOG_END()                                                                                                         \
    FUNCTION_LOG_END_BASE()

#define FUNCTION_LOG_VOID(logLevel)                                                                                                \
    FUNCTION_LOG_BEGIN_BASE(logLevel);                                                                                             \
    FUNCTION_LOG_END_BASE()

#define FUNCTION_LOG_PARAM(typeMacroPrefix, param)                                                                                 \
    stackTraceParamAdd(FUNCTION_LOG_##typeMacroPrefix##_FORMAT(param, stackTraceParamBuffer(#param), STACK_TRACE_PARAM_MAX))

#define FUNCTION_LOG_PARAM_P(typeMacroPrefix, param)                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        char *buffer = stackTraceParamBuffer(#param);                                                                              \
                                                                                                                                   \
        if (param == NULL)                                                                                                         \
            stackTraceParamAdd(typeToLog(NULL_Z, buffer, STACK_TRACE_PARAM_MAX));                                                  \
        else                                                                                                                       \
        {                                                                                                                          \
            buffer[0] = '*';                                                                                                       \
            stackTraceParamAdd(FUNCTION_LOG_##typeMacroPrefix##_FORMAT(*param, buffer + 1, STACK_TRACE_PARAM_MAX - 1) + 1);        \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

#define FUNCTION_LOG_PARAM_PP(typeMacroPrefix, param)                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        char *buffer = stackTraceParamBuffer(#param);                                                                              \
                                                                                                                                   \
        if (param == NULL)                                                                                                         \
            stackTraceParamAdd(typeToLog(NULL_Z, buffer, STACK_TRACE_PARAM_MAX));                                                  \
        else if (*param == NULL)                                                                                                   \
            stackTraceParamAdd(typeToLog("*null", buffer, STACK_TRACE_PARAM_MAX));                                                 \
        else                                                                                                                       \
        {                                                                                                                          \
            buffer[0] = '*';                                                                                                       \
            buffer[1] = '*';                                                                                                       \
            stackTraceParamAdd(FUNCTION_LOG_##typeMacroPrefix##_FORMAT(**param, buffer + 2, STACK_TRACE_PARAM_MAX - 2) + 2);       \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Functions and macros to render various data types
***********************************************************************************************************************************/
// Convert object to a zero-terminated string for logging
typedef void (*ObjToLogFormat)(const void *object, StringStatic *debugLog);

FN_EXTERN size_t objToLog(const void *object, ObjToLogFormat formatFunc, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_OBJECT_FORMAT(object, formatFunc, buffer, bufferSize)                                                         \
    objToLog(object, (ObjToLogFormat)formatFunc, buffer, bufferSize)

// Convert object name to a zero-terminated string for logging
FN_EXTERN size_t objNameToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize);

// Convert pointer to a zero-terminated string for logging
FN_EXTERN size_t ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize);

// Convert zero-terminated string for logging
FN_INLINE_ALWAYS size_t
strzToLog(const char *const string, char *const buffer, const size_t bufferSize)
{
    return (size_t)snprintf(buffer, bufferSize, string == NULL ? "%s" : "\"%s\"", string == NULL ? NULL_Z : string);
}

// Convert a type name to a zero-terminated string for logging
FN_EXTERN size_t typeToLog(const char *typeName, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_BOOL_TYPE                                                                                                     \
    bool
#define FUNCTION_LOG_BOOL_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtBoolToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_CHAR_TYPE                                                                                                     \
    char
#define FUNCTION_LOG_CHAR_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtCharToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_CHARDATA_TYPE                                                                                                 \
    char
#define FUNCTION_LOG_CHARDATA_FORMAT(value, buffer, bufferSize)                                                                    \
    typeToLog("(char)", buffer, bufferSize)

#define FUNCTION_LOG_CHARPY_TYPE                                                                                                   \
    char *[]
#define FUNCTION_LOG_CHARPY_FORMAT(value, buffer, bufferSize)                                                                      \
    ptrToLog(value, "char *[]", buffer, bufferSize)

#define FUNCTION_LOG_DOUBLE_TYPE                                                                                                   \
    double
#define FUNCTION_LOG_DOUBLE_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtDoubleToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_INT_TYPE                                                                                                      \
    int
#define FUNCTION_LOG_INT_FORMAT(value, buffer, bufferSize)                                                                         \
    cvtIntToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_INT64_TYPE                                                                                                    \
    int64_t
#define FUNCTION_LOG_INT64_FORMAT(value, buffer, bufferSize)                                                                       \
    cvtInt64ToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_ENUM_TYPE                                                                                                     \
    unsigned int
#define FUNCTION_LOG_ENUM_FORMAT(value, buffer, bufferSize)                                                                        \
    FUNCTION_LOG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_LOG_FUNCTIONP_FORMAT(value, buffer, bufferSize)                                                                   \
    ptrToLog(value == NULL ? NULL : (void *)1, "function *", buffer, bufferSize)

#define FUNCTION_LOG_MODE_TYPE                                                                                                     \
    mode_t
#define FUNCTION_LOG_MODE_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtModeToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_TIMEMSEC_TYPE                                                                                                 \
    TimeMSec
#define FUNCTION_LOG_TIMEMSEC_FORMAT(value, buffer, bufferSize)                                                                    \
    cvtUInt64ToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_UCHARDATA_TYPE                                                                                                \
    unsigned char
#define FUNCTION_LOG_UCHARDATA_FORMAT(value, buffer, bufferSize)                                                                   \
    typeToLog("(unsigned char)", buffer, bufferSize)

#define FUNCTION_LOG_SIZE_TYPE                                                                                                     \
    size_t
#define FUNCTION_LOG_SIZE_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtSizeToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_TIME_TYPE                                                                                                     \
    time_t
#define FUNCTION_LOG_TIME_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtTimeToZP("%s", value, buffer, bufferSize)

#define FUNCTION_LOG_UINT_TYPE                                                                                                     \
    unsigned int
#define FUNCTION_LOG_UINT_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtUIntToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_UINT16_TYPE                                                                                                   \
    uint16_t
#define FUNCTION_LOG_UINT16_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_LOG_UINT32_TYPE                                                                                                   \
    uint32_t
#define FUNCTION_LOG_UINT32_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_LOG_UINT64_TYPE                                                                                                   \
    uint64_t
#define FUNCTION_LOG_UINT64_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtUInt64ToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_VOID_TYPE                                                                                                     \
    void
#define FUNCTION_LOG_VOID_FORMAT(value, buffer, bufferSize)                                                                        \
    typeToLog("void", buffer, bufferSize)

#define FUNCTION_LOG_STRINGZ_TYPE                                                                                                  \
    char *
#define FUNCTION_LOG_STRINGZ_FORMAT(value, buffer, bufferSize)                                                                     \
    strzToLog(value, buffer, bufferSize)

/***********************************************************************************************************************************
Macros to return function results (or void)
***********************************************************************************************************************************/
#define FUNCTION_LOG_RETURN_BASE(typePre, typeMacroPrefix, typePost, ...)                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        typePre FUNCTION_LOG_##typeMacroPrefix##_TYPE typePost FUNCTION_LOG_RETURN_result = __VA_ARGS__;                           \
                                                                                                                                   \
        FUNCTION_TEST_MEM_CONTEXT_AUDIT_END(STRINGIFY(FUNCTION_LOG_##typeMacroPrefix##_TYPE));                                     \
        STACK_TRACE_POP(false);                                                                                                    \
                                                                                                                                   \
        IF_LOG_ANY(FUNCTION_LOG_LEVEL())                                                                                           \
        {                                                                                                                          \
            char buffer[STACK_TRACE_PARAM_MAX];                                                                                    \
                                                                                                                                   \
            FUNCTION_LOG_##typeMacroPrefix##_FORMAT(FUNCTION_LOG_RETURN_result, buffer, sizeof(buffer));                           \
            LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "=> %s", buffer);                                                                     \
        }                                                                                                                          \
                                                                                                                                   \
        return FUNCTION_LOG_RETURN_result;                                                                                         \
    }                                                                                                                              \
    while (0)

#define FUNCTION_LOG_RETURN(typeMacroPrefix, ...)                                                                                  \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, , __VA_ARGS__)

#define FUNCTION_LOG_RETURN_P(typeMacroPrefix, ...)                                                                                \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, *, __VA_ARGS__)

#define FUNCTION_LOG_RETURN_PP(typeMacroPrefix, ...)                                                                               \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, **, __VA_ARGS__)

#define FUNCTION_LOG_RETURN_CONST(typeMacroPrefix, ...)                                                                            \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, , __VA_ARGS__)

#define FUNCTION_LOG_RETURN_CONST_P(typeMacroPrefix, ...)                                                                          \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, *, __VA_ARGS__)

#define FUNCTION_LOG_RETURN_CONST_PP(typeMacroPrefix, ...)                                                                         \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, **, __VA_ARGS__)

#define FUNCTION_LOG_RETURN_STRUCT(...)                                                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        FUNCTION_TEST_MEM_CONTEXT_AUDIT_END("struct");                                                                             \
        STACK_TRACE_POP(false);                                                                                                    \
                                                                                                                                   \
        IF_LOG_ANY(FUNCTION_LOG_LEVEL())                                                                                           \
            LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "=> struct");                                                                         \
                                                                                                                                   \
        return __VA_ARGS__;                                                                                                        \
    }                                                                                                                              \
    while (0)

#define FUNCTION_LOG_RETURN_VOID()                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        FUNCTION_TEST_MEM_CONTEXT_AUDIT_END("void");                                                                               \
        STACK_TRACE_POP(false);                                                                                                    \
                                                                                                                                   \
        LOG(FUNCTION_LOG_LEVEL(), 0, "=> void");                                                                                   \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Function Test Macros

In debug builds these macros will update the stack trace with function names and parameters but will not log. In production builds
all test macros are compiled out (except for return statements).

Ignore DEBUG_TEST_TRACE_MACRO if DEBUG is not defined because the underlying functions that support the macros will not be present.
***********************************************************************************************************************************/
#ifdef DEBUG
#ifdef DEBUG_TEST_TRACE
#define DEBUG_TEST_TRACE_MACRO
#endif // DEBUG_TEST_TRACE
#endif // DEBUG

// Annotate functions that do not return so it is clear why there is no FUNCTION_TEST_RETURN*() macro
#define FUNCTION_TEST_NO_RETURN()

#ifdef DEBUG_TEST_TRACE_MACRO

#define FUNCTION_TEST_BEGIN()                                                                                                      \
    FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN();                                                                                       \
                                                                                                                                   \
    /* Ensure that FUNCTION_LOG_BEGIN() and FUNCTION_TEST_BEGIN() are not both used in a single function by declaring the */       \
    /* same variable that FUNCTION_LOG_BEGIN() uses to track logging */                                                            \
    LogLevel FUNCTION_LOG_LEVEL();                                                                                                 \
    (void)FUNCTION_LOG_LEVEL();                                                                                                    \
                                                                                                                                   \
    /* Ensure that FUNCTION_TEST_RETURN*() is not used with FUNCTION_LOG_BEGIN*() by declaring a variable that will be */          \
    /* referenced in FUNCTION_TEST_RETURN*() */                                                                                    \
    bool FUNCTION_TEST_BEGIN_exists;                                                                                               \
                                                                                                                                   \
    if (stackTraceTest())                                                                                                          \
    {                                                                                                                              \
        STACK_TRACE_PUSH(logLevelDebug);                                                                                           \
        stackTraceParamLog();                                                                                                      \
        stackTraceTestStop()

#define FUNCTION_TEST_PARAM(typeMacroPrefix, param)                                                                                \
    FUNCTION_LOG_PARAM(typeMacroPrefix, param)

#define FUNCTION_TEST_PARAM_P(typeName, param)                                                                                     \
    FUNCTION_LOG_PARAM_P(typeName, param)

#define FUNCTION_TEST_PARAM_PP(typeName, param)                                                                                    \
    FUNCTION_LOG_PARAM_PP(typeName, param)

#define FUNCTION_TEST_END()                                                                                                        \
        (void)FUNCTION_TEST_BEGIN_exists; /* CHECK for presence of FUNCTION_TEST_BEGIN*() */                                       \
                                                                                                                                   \
        stackTraceTestStart();                                                                                                     \
    }

#define FUNCTION_TEST_VOID()                                                                                                       \
    FUNCTION_TEST_BEGIN();                                                                                                         \
    FUNCTION_TEST_END();

#define FUNCTION_TEST_RETURN_TYPE_BASE(typePre, type, typePost, ...)                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        (void)FUNCTION_TEST_BEGIN_exists; /* CHECK for presence of FUNCTION_TEST_BEGIN*() */                                       \
                                                                                                                                   \
        typePre type typePost FUNCTION_TEST_result = __VA_ARGS__;                                                                  \
                                                                                                                                   \
        STACK_TRACE_POP(true);                                                                                                     \
        FUNCTION_TEST_MEM_CONTEXT_AUDIT_END(STRINGIFY(type));                                                                      \
                                                                                                                                   \
        return FUNCTION_TEST_result;                                                                                               \
    }                                                                                                                              \
    while (0)

#define FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(typePre, typeMacroPrefix, typePost, ...)                                              \
    FUNCTION_TEST_RETURN_TYPE_BASE(typePre, FUNCTION_LOG_##typeMacroPrefix##_TYPE, typePost, __VA_ARGS__)

#define FUNCTION_TEST_RETURN(typeMacroPrefix, ...)                                                                                 \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(, typeMacroPrefix, , __VA_ARGS__)
#define FUNCTION_TEST_RETURN_P(typeMacroPrefix, ...)                                                                               \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(, typeMacroPrefix, *, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_PP(typeMacroPrefix, ...)                                                                              \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(, typeMacroPrefix, **, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_CONST(typeMacroPrefix, ...)                                                                           \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(const, typeMacroPrefix, , __VA_ARGS__)
#define FUNCTION_TEST_RETURN_CONST_P(typeMacroPrefix, ...)                                                                         \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(const, typeMacroPrefix, *, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_CONST_PP(typeMacroPrefix, ...)                                                                        \
    FUNCTION_TEST_RETURN_TYPE_MACRO_BASE(const, typeMacroPrefix, **, __VA_ARGS__)

#define FUNCTION_TEST_RETURN_TYPE(type, ...)                                                                                       \
    FUNCTION_TEST_RETURN_TYPE_BASE(, type, , __VA_ARGS__)
#define FUNCTION_TEST_RETURN_TYPE_P(type, ...)                                                                                     \
    FUNCTION_TEST_RETURN_TYPE_BASE(, type, *, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_TYPE_PP(type, ...)                                                                                    \
    FUNCTION_TEST_RETURN_TYPE_BASE(, type, **, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_TYPE_CONST(type, ...)                                                                                 \
    FUNCTION_TEST_RETURN_TYPE_BASE(const, type, , __VA_ARGS__)
#define FUNCTION_TEST_RETURN_TYPE_CONST_P(type, ...)                                                                               \
    FUNCTION_TEST_RETURN_TYPE_BASE(const, type, *, __VA_ARGS__)
#define FUNCTION_TEST_RETURN_TYPE_CONST_PP(type, ...)                                                                              \
    FUNCTION_TEST_RETURN_TYPE_BASE(const, type, **, __VA_ARGS__)

#define FUNCTION_TEST_RETURN_VOID()                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        (void)FUNCTION_TEST_BEGIN_exists; /* CHECK for presence of FUNCTION_TEST_BEGIN*() */                                       \
                                                                                                                                   \
        FUNCTION_TEST_MEM_CONTEXT_AUDIT_END("void");                                                                               \
        STACK_TRACE_POP(true);                                                                                                     \
        return;                                                                                                                    \
    }                                                                                                                              \
    while (0)

#else

#define FUNCTION_TEST_BEGIN()
#define FUNCTION_TEST_PARAM(typeMacroPrefix, param)
#define FUNCTION_TEST_PARAM_P(typeMacroPrefix, param)
#define FUNCTION_TEST_PARAM_PP(typeMacroPrefix, param)
#define FUNCTION_TEST_END()
#define FUNCTION_TEST_VOID()

#define FUNCTION_TEST_RETURN(typeMacroPrefix, ...)                                                                                 \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_P(typeMacroPrefix, ...)                                                                               \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_PP(typeMacroPrefix, ...)                                                                              \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_CONST(typeMacroPrefix, ...)                                                                           \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_CONST_P(typeMacroPrefix, ...)                                                                         \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_CONST_PP(typeMacroPrefix, ...)                                                                        \
    return __VA_ARGS__

#define FUNCTION_TEST_RETURN_TYPE(type, ...)                                                                                       \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_TYPE_P(type, ...)                                                                                     \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_TYPE_PP(type, ...)                                                                                    \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_TYPE_CONST(type, ...)                                                                                 \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_TYPE_CONST_P(type, ...)                                                                               \
    return __VA_ARGS__
#define FUNCTION_TEST_RETURN_TYPE_CONST_PP(type, ...)                                                                              \
    return __VA_ARGS__

#define FUNCTION_TEST_RETURN_VOID()                                                                                                \
    return

#endif // DEBUG_TEST_TRACE_MACRO

#endif
