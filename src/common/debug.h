/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "common/assert.h"
#include "common/stackTrace.h"
#include "common/type/convert.h"
#include "common/type/stringz.h"

/***********************************************************************************************************************************
Extern variables that are needed for unit testing
***********************************************************************************************************************************/
#ifdef DEBUG_UNIT
    #define DEBUG_UNIT_EXTERN
#else
    #define DEBUG_UNIT_EXTERN                                                                                                      \
        static
#endif

/***********************************************************************************************************************************
Base function debugging macros

In debug mode parameters will always be recorded in the stack trace while in production mode they will only be recorded when the log
level is set to debug or trace.
***********************************************************************************************************************************/
#define FUNCTION_LOG_LEVEL()                                                                                                       \
    FUNCTION_LOG_logLevel

#ifdef DEBUG_TEST_TRACE
    #define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                      \
        LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                \
                                                                                                                                   \
        {                                                                                                                          \
            stackTraceParamLog();                                                                                                  \
            stackTraceTestStop();

    #define FUNCTION_LOG_END_BASE()                                                                                                \
            stackTraceTestStart();                                                                                                 \
            LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                           \
        }
#else
    #define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                      \
        LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                \
                                                                                                                                   \
        if (logAny(FUNCTION_LOG_LEVEL()))                                                                                          \
        {                                                                                                                          \
            stackTraceParamLog();

    #define FUNCTION_LOG_END_BASE()                                                                                                \
            LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                           \
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
size_t objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize);

// Convert pointer to a zero-terminated string for logging
size_t ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize);

// Convert zero-terminated string for logging
size_t strzToLog(const char *string, char *buffer, size_t bufferSize);

// Convert a type name to a zero-terminated string for logging
size_t typeToLog(const char *typeName, char *buffer, size_t bufferSize);

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

#define FUNCTION_LOG_SSIZE_TYPE                                                                                                    \
    ssize_t
#define FUNCTION_LOG_SSIZE_FORMAT(value, buffer, bufferSize)                                                                       \
    cvtSSizeToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_TIME_TYPE                                                                                                     \
    time_t
#define FUNCTION_LOG_TIME_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtTimeToZ(value, buffer, bufferSize)

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
#define FUNCTION_LOG_RETURN_BASE(typePre, typeMacroPrefix, typePost, result)                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        typePre FUNCTION_LOG_##typeMacroPrefix##_TYPE typePost FUNCTION_LOG_RETURN_result = result;                                \
                                                                                                                                   \
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

#define FUNCTION_LOG_RETURN(typeMacroPrefix, result)                                                                               \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, , result)

#define FUNCTION_LOG_RETURN_P(typeMacroPrefix, result)                                                                             \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, *, result)

#define FUNCTION_LOG_RETURN_PP(typeMacroPrefix, result)                                                                            \
    FUNCTION_LOG_RETURN_BASE(, typeMacroPrefix, **, result)

#define FUNCTION_LOG_RETURN_CONST(typeMacroPrefix, result)                                                                         \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, , result)

#define FUNCTION_LOG_RETURN_CONST_P(typeMacroPrefix, result)                                                                       \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, *, result)

#define FUNCTION_LOG_RETURN_CONST_PP(typeMacroPrefix, result)                                                                      \
    FUNCTION_LOG_RETURN_BASE(const, typeMacroPrefix, **, result)

#define FUNCTION_LOG_RETURN_STRUCT(result)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        STACK_TRACE_POP(false);                                                                                                    \
                                                                                                                                   \
        IF_LOG_ANY(FUNCTION_LOG_LEVEL())                                                                                           \
            LOG_FMT(FUNCTION_LOG_LEVEL(), 0, "=> struct");                                                                         \
                                                                                                                                   \
        return result;                                                                                                             \
    }                                                                                                                              \
    while (0)

#define FUNCTION_LOG_RETURN_VOID()                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
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

#ifdef DEBUG_TEST_TRACE_MACRO
    #define FUNCTION_TEST_BEGIN()                                                                                                  \
        if (stackTraceTest())                                                                                                      \
        {                                                                                                                          \
            STACK_TRACE_PUSH(logLevelDebug);                                                                                       \
            stackTraceParamLog();                                                                                                  \
            stackTraceTestStop()

    #define FUNCTION_TEST_PARAM(typeMacroPrefix, param)                                                                            \
        FUNCTION_LOG_PARAM(typeMacroPrefix, param)

    #define FUNCTION_TEST_PARAM_P(typeName, param)                                                                                 \
        FUNCTION_LOG_PARAM_P(typeName, param)

    #define FUNCTION_TEST_PARAM_PP(typeName, param)                                                                                \
        FUNCTION_LOG_PARAM_PP(typeName, param)

    #define FUNCTION_TEST_END()                                                                                                    \
            stackTraceTestStart();                                                                                                 \
        }

    #define FUNCTION_TEST_VOID()                                                                                                   \
        FUNCTION_TEST_BEGIN();                                                                                                     \
        FUNCTION_TEST_END();

    #define FUNCTION_TEST_RETURN(result)                                                                                           \
        do                                                                                                                         \
        {                                                                                                                          \
            STACK_TRACE_POP(true);                                                                                                 \
            return result;                                                                                                         \
        }                                                                                                                          \
        while (0)

    #define FUNCTION_TEST_RETURN_VOID()                                                                                            \
        STACK_TRACE_POP(true)
#else
    #define FUNCTION_TEST_BEGIN()
    #define FUNCTION_TEST_PARAM(typeMacroPrefix, param)
    #define FUNCTION_TEST_PARAM_P(typeMacroPrefix, param)
    #define FUNCTION_TEST_PARAM_PP(typeMacroPrefix, param)
    #define FUNCTION_TEST_END()
    #define FUNCTION_TEST_VOID()
    #define FUNCTION_TEST_RETURN(result)                                                                                           \
        return result
    #define FUNCTION_TEST_RETURN_VOID()
#endif // DEBUG_TEST_TRACE_MACRO

#endif
