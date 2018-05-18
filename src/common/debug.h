/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "common/stackTrace.h"
#include "common/type/convert.h"

/***********************************************************************************************************************************
NDEBUG indicates to C library routines that debugging is off -- set a more readable flag to use when debugging is on
***********************************************************************************************************************************/
#ifndef NDEBUG
    #define DEBUG
#endif

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
#define FUNCTION_DEBUG_LEVEL()                                                                                                     \
    FUNCTION_DEBUG_logLevel

#ifdef DEBUG
    #define FUNCTION_DEBUG_BEGIN_BASE(logLevel)                                                                                    \
        LogLevel FUNCTION_DEBUG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                              \
                                                                                                                                   \
        {                                                                                                                          \
            stackTraceParamLog();

    #define FUNCTION_DEBUG_END_BASE()                                                                                              \
            LOG_WILL(FUNCTION_DEBUG_LEVEL(), 0, "(%s)", stackTraceParam());                                                        \
        }

    #define FUNCTION_DEBUG_PARAM_BASE_BEGIN()                                                                                      \
        stackTraceTestStop()                                                                                                       \

    #define FUNCTION_DEBUG_PARAM_BASE_END()                                                                                        \
        stackTraceTestStart()
#else
    #define FUNCTION_DEBUG_BEGIN_BASE(logLevel)                                                                                    \
        LogLevel FUNCTION_DEBUG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                              \
                                                                                                                                   \
        if (logWill(FUNCTION_DEBUG_LEVEL()))                                                                                       \
        {                                                                                                                          \
            stackTraceParamLog()

    #define FUNCTION_DEBUG_END_BASE()                                                                                              \
            LOG(FUNCTION_DEBUG_LEVEL(), 0, "(%s)", stackTraceParam());                                                             \
        }

    #define FUNCTION_DEBUG_PARAM_BASE_BEGIN()
    #define FUNCTION_DEBUG_PARAM_BASE_END()
#endif

/***********************************************************************************************************************************
General purpose function debugging macros

FUNCTION_DEBUG_VOID() is provided as a shortcut for functions that have no parameters.
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_BEGIN(logLevel)                                                                                             \
    FUNCTION_DEBUG_BEGIN_BASE(logLevel)

#define FUNCTION_DEBUG_END()                                                                                                       \
    FUNCTION_DEBUG_END_BASE()

#define FUNCTION_DEBUG_VOID(logLevel)                                                                                              \
    FUNCTION_DEBUG_BEGIN_BASE(logLevel);                                                                                           \
    FUNCTION_DEBUG_END_BASE()

#define FUNCTION_DEBUG_PARAM(typeMacroPrefix, param)                                                                               \
    FUNCTION_DEBUG_PARAM_BASE_BEGIN();                                                                                             \
    stackTraceParamAdd(FUNCTION_DEBUG_##typeMacroPrefix##_FORMAT(param, stackTraceParamBuffer(#param), STACK_TRACE_PARAM_MAX));    \
    FUNCTION_DEBUG_PARAM_BASE_END()

#define FUNCTION_DEBUG_PARAM_PTR(typeName, param)                                                                                  \
    FUNCTION_DEBUG_PARAM_BASE_BEGIN();                                                                                             \
    stackTraceParamAdd(ptrToLog(param, typeName, stackTraceParamBuffer(#param), STACK_TRACE_PARAM_MAX   ));                        \
    FUNCTION_DEBUG_PARAM_BASE_END()

/***********************************************************************************************************************************
Functions and macros to render various data types
***********************************************************************************************************************************/
size_t objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize);
size_t ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize);
size_t strzToLog(const char *string, char *buffer, size_t bufferSize);

#define FUNCTION_DEBUG_BOOL_TYPE                                                                                                   \
    bool
#define FUNCTION_DEBUG_BOOL_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtBoolToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_BOOLP_TYPE                                                                                                  \
    bool *
#define FUNCTION_DEBUG_BOOLP_FORMAT(value, buffer, bufferSize)                                                                     \
    ptrToLog(value, "bool *", buffer, bufferSize)

#define FUNCTION_DEBUG_CHARP_TYPE                                                                                                  \
    char *
#define FUNCTION_DEBUG_CHARP_FORMAT(value, buffer, bufferSize)                                                                     \
    ptrToLog(value, "char *", buffer, bufferSize)

#define FUNCTION_DEBUG_CONST_CHARPP_TYPE                                                                                           \
    const char **
#define FUNCTION_DEBUG_CONST_CHARPP_FORMAT(value, buffer, bufferSize)                                                              \
    ptrToLog(value, "char **", buffer, bufferSize)

#define FUNCTION_DEBUG_CHARPY_TYPE                                                                                                 \
    char *[]
#define FUNCTION_DEBUG_CHARPY_FORMAT(value, buffer, bufferSize)                                                                    \
    ptrToLog(value, "char *[]", buffer, bufferSize)

#define FUNCTION_DEBUG_DOUBLE_TYPE                                                                                                 \
    double
#define FUNCTION_DEBUG_DOUBLE_FORMAT(value, buffer, bufferSize)                                                                    \
    cvtDoubleToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_DOUBLEP_TYPE                                                                                                \
    double *
#define FUNCTION_DEBUG_DOUBLEP_FORMAT(value, buffer, bufferSize)                                                                   \
    ptrToLog(value, "double *", buffer, bufferSize)

#define FUNCTION_DEBUG_INT_TYPE                                                                                                    \
    int
#define FUNCTION_DEBUG_INT_FORMAT(value, buffer, bufferSize)                                                                       \
    cvtIntToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_INTP_TYPE                                                                                                   \
    int *
#define FUNCTION_DEBUG_INTP_FORMAT(value, buffer, bufferSize)                                                                      \
    ptrToLog(value, "int *", buffer, bufferSize)

#define FUNCTION_DEBUG_INT64_TYPE                                                                                                  \
    int64_t
#define FUNCTION_DEBUG_INT64_FORMAT(value, buffer, bufferSize)                                                                     \
    cvtInt64ToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_ENUM_TYPE                                                                                                   \
    unsigned int
#define FUNCTION_DEBUG_ENUM_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_DEBUG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_DEBUG_FUNCTIONP_FORMAT(value, buffer, bufferSize)                                                                 \
    ptrToLog(value == NULL ? NULL : (void *)1, "function *", buffer, bufferSize)

#define FUNCTION_DEBUG_MODE_TYPE                                                                                                   \
    mode_t
#define FUNCTION_DEBUG_MODE_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtModeToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_UCHARP_TYPE                                                                                                 \
    unsigned char *
#define FUNCTION_DEBUG_UCHARP_FORMAT(value, buffer, bufferSize)                                                                    \
    ptrToLog(value, "unsigned char *", buffer, bufferSize)

#define FUNCTION_DEBUG_SIZE_TYPE                                                                                                   \
    size_t
#define FUNCTION_DEBUG_SIZE_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtSizeToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_UINT_TYPE                                                                                                   \
    unsigned int
#define FUNCTION_DEBUG_UINT_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtUIntToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_UINT16_TYPE                                                                                                 \
    uint16_t
#define FUNCTION_DEBUG_UINT16_FORMAT(value, buffer, bufferSize)                                                                    \
    FUNCTION_DEBUG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_DEBUG_UINT32_TYPE                                                                                                 \
    uint32_t
#define FUNCTION_DEBUG_UINT32_FORMAT(value, buffer, bufferSize)                                                                    \
    FUNCTION_DEBUG_UINT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_DEBUG_UINT64_TYPE                                                                                                 \
    uint64_t
#define FUNCTION_DEBUG_UINT64_FORMAT(value, buffer, bufferSize)                                                                    \
    cvtUInt64ToZ(value, buffer, bufferSize)

#define FUNCTION_DEBUG_VOIDP_TYPE                                                                                                  \
    void *
#define FUNCTION_DEBUG_VOIDP_FORMAT(value, buffer, bufferSize)                                                                     \
    ptrToLog(value, "void *", buffer, bufferSize)

#define FUNCTION_DEBUG_VOIDPP_TYPE                                                                                                 \
    void **
#define FUNCTION_DEBUG_VOIDPP_FORMAT(value, buffer, bufferSize)                                                                    \
    ptrToLog(value, "void **", buffer, bufferSize)

#define FUNCTION_DEBUG_STRINGZ_TYPE                                                                                                \
    const char *
#define FUNCTION_DEBUG_STRINGZ_FORMAT(value, buffer, bufferSize)                                                                   \
    strzToLog(value, buffer, bufferSize)

/***********************************************************************************************************************************
Assert function to be used with function debugging

The assert statement is compiled into production code but only runs when the log level >= debug
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_ASSERT(condition)                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "function debug assertion '%s' failed", #condition);                                            \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Macros to return function results (or void)
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_RESULT(typeMacroPrefix, result)                                                                             \
    do                                                                                                                             \
    {                                                                                                                              \
        FUNCTION_DEBUG_##typeMacroPrefix##_TYPE FUNCTION_DEBUG_RESULT_result = result;                                             \
                                                                                                                                   \
        STACK_TRACE_POP();                                                                                                         \
                                                                                                                                   \
        if (logWill(FUNCTION_DEBUG_LEVEL()))                                                                                       \
        {                                                                                                                          \
            char buffer[STACK_TRACE_PARAM_MAX];                                                                                    \
                                                                                                                                   \
            FUNCTION_DEBUG_##typeMacroPrefix##_FORMAT(FUNCTION_DEBUG_RESULT_result, buffer, sizeof(buffer));                       \
            LOG(FUNCTION_DEBUG_LEVEL(), 0, "=> %s", buffer);                                                                       \
        }                                                                                                                          \
                                                                                                                                   \
        return FUNCTION_DEBUG_RESULT_result;                                                                                       \
    }                                                                                                                              \
    while(0)

#define FUNCTION_DEBUG_RESULT_VOID()                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        STACK_TRACE_POP();                                                                                                         \
                                                                                                                                   \
        LOG_WILL(FUNCTION_DEBUG_LEVEL(), 0, "=> void");                                                                            \
    }                                                                                                                              \
    while(0)

/***********************************************************************************************************************************
Function Test Macros

In debug builds these macros will update the stack trace with function names and parameters but not log.  In production builds all
test macros are compiled out.
***********************************************************************************************************************************/
#ifdef DEBUG
    #define FUNCTION_TEST_BEGIN()                                                                                                  \
        if (stackTraceTest())                                                                                                      \
        {                                                                                                                          \
            STACK_TRACE_PUSH(logLevelDebug);                                                                                       \
            stackTraceParamLog()

    #define FUNCTION_TEST_PARAM(typeMacroPrefix, param)                                                                            \
        FUNCTION_DEBUG_PARAM(typeMacroPrefix, param)

    #define FUNCTION_TEST_PARAM_PTR(typeName, param)                                                                               \
        FUNCTION_DEBUG_PARAM_PTR(typeName, param)

    #define FUNCTION_TEST_END()                                                                                                    \
        }

    #define FUNCTION_TEST_VOID()                                                                                                   \
        FUNCTION_TEST_BEGIN();                                                                                                     \
        FUNCTION_TEST_END();

    #define FUNCTION_TEST_ASSERT(condition)                                                                                        \
        do                                                                                                                         \
        {                                                                                                                          \
            if (!(condition))                                                                                                      \
                THROW_FMT(AssertError, "function test assertion '%s' failed", #condition);                                         \
        }                                                                                                                          \
        while (0)

    #define FUNCTION_TEST_RESULT(typeMacroPrefix, result)                                                                          \
        do                                                                                                                         \
        {                                                                                                                          \
            FUNCTION_DEBUG_##typeMacroPrefix##_TYPE FUNCTION_DEBUG_RESULT_result = result;                                         \
                                                                                                                                   \
            if (stackTraceTest())                                                                                                  \
                STACK_TRACE_POP();                                                                                                 \
                                                                                                                                   \
            return FUNCTION_DEBUG_RESULT_result;                                                                                   \
        }                                                                                                                          \
        while(0);

    #define FUNCTION_TEST_RESULT_VOID()                                                                                            \
        do                                                                                                                         \
        {                                                                                                                          \
            if (stackTraceTest())                                                                                                  \
                STACK_TRACE_POP();                                                                                                 \
        }                                                                                                                          \
        while(0);
#else
    #define FUNCTION_TEST_BEGIN()
    #define FUNCTION_TEST_PARAM(typeMacroPrefix, param)
    #define FUNCTION_TEST_PARAM_PTR(typeName, param)
    #define FUNCTION_TEST_END()
    #define FUNCTION_TEST_VOID()
    #define FUNCTION_TEST_ASSERT(condition)
    #define FUNCTION_TEST_RESULT(typeMacroPrefix, result)                                                                          \
        return result
    #define FUNCTION_TEST_RESULT_VOID()
#endif

#endif
