/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "common/assert.h"
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
#define FUNCTION_LOG_LEVEL()                                                                                                       \
    FUNCTION_LOG_logLevel

#ifdef DEBUG
    #define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                      \
        LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                \
                                                                                                                                   \
        {                                                                                                                          \
            stackTraceParamLog();

    #define FUNCTION_LOG_END_BASE()                                                                                                \
            LOG_WILL(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                          \
        }

    #define FUNCTION_LOG_PARAM_BASE_BEGIN()                                                                                        \
        stackTraceTestStop()                                                                                                       \

    #define FUNCTION_LOG_PARAM_BASE_END()                                                                                          \
        stackTraceTestStart()
#else
    #define FUNCTION_LOG_BEGIN_BASE(logLevel)                                                                                      \
        LogLevel FUNCTION_LOG_LEVEL() = STACK_TRACE_PUSH(logLevel);                                                                \
                                                                                                                                   \
        if (logWill(FUNCTION_LOG_LEVEL()))                                                                                         \
        {                                                                                                                          \
            stackTraceParamLog()

    #define FUNCTION_LOG_END_BASE()                                                                                                \
            LOG(FUNCTION_LOG_LEVEL(), 0, "(%s)", stackTraceParam());                                                               \
        }

    #define FUNCTION_LOG_PARAM_BASE_BEGIN()
    #define FUNCTION_LOG_PARAM_BASE_END()
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
    FUNCTION_LOG_PARAM_BASE_BEGIN();                                                                                               \
    stackTraceParamAdd(FUNCTION_LOG_##typeMacroPrefix##_FORMAT(param, stackTraceParamBuffer(#param), STACK_TRACE_PARAM_MAX));      \
    FUNCTION_LOG_PARAM_BASE_END()

#define FUNCTION_LOG_PARAM_PTR(typeName, param)                                                                                    \
    FUNCTION_LOG_PARAM_BASE_BEGIN();                                                                                               \
    stackTraceParamAdd(ptrToLog(param, typeName, stackTraceParamBuffer(#param), STACK_TRACE_PARAM_MAX   ));                        \
    FUNCTION_LOG_PARAM_BASE_END()

/***********************************************************************************************************************************
Functions and macros to render various data types
***********************************************************************************************************************************/
size_t objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize);
size_t ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize);
size_t strzToLog(const char *string, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_BOOL_TYPE                                                                                                     \
    bool
#define FUNCTION_LOG_BOOL_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtBoolToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_BOOLP_TYPE                                                                                                    \
    bool *
#define FUNCTION_LOG_BOOLP_FORMAT(value, buffer, bufferSize)                                                                       \
    ptrToLog(value, "bool *", buffer, bufferSize)

#define FUNCTION_LOG_CHAR_TYPE                                                                                                     \
    char
#define FUNCTION_LOG_CHAR_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtCharToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_CHARP_TYPE                                                                                                    \
    char *
#define FUNCTION_LOG_CHARP_FORMAT(value, buffer, bufferSize)                                                                       \
    ptrToLog(value, "char *", buffer, bufferSize)

#define FUNCTION_LOG_CONST_CHARPP_TYPE                                                                                             \
    const char **
#define FUNCTION_LOG_CONST_CHARPP_FORMAT(value, buffer, bufferSize)                                                                \
    ptrToLog(value, "char **", buffer, bufferSize)

#define FUNCTION_LOG_CHARPY_TYPE                                                                                                   \
    char *[]
#define FUNCTION_LOG_CHARPY_FORMAT(value, buffer, bufferSize)                                                                      \
    ptrToLog(value, "char *[]", buffer, bufferSize)

#define FUNCTION_LOG_DOUBLE_TYPE                                                                                                   \
    double
#define FUNCTION_LOG_DOUBLE_FORMAT(value, buffer, bufferSize)                                                                      \
    cvtDoubleToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_DOUBLEP_TYPE                                                                                                  \
    double *
#define FUNCTION_LOG_DOUBLEP_FORMAT(value, buffer, bufferSize)                                                                     \
    ptrToLog(value, "double *", buffer, bufferSize)

#define FUNCTION_LOG_INT_TYPE                                                                                                      \
    int
#define FUNCTION_LOG_INT_FORMAT(value, buffer, bufferSize)                                                                         \
    cvtIntToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_INTP_TYPE                                                                                                     \
    int *
#define FUNCTION_LOG_INTP_FORMAT(value, buffer, bufferSize)                                                                        \
    ptrToLog(value, "int *", buffer, bufferSize)

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

#define FUNCTION_LOG_UCHARP_TYPE                                                                                                   \
    unsigned char *
#define FUNCTION_LOG_UCHARP_FORMAT(value, buffer, bufferSize)                                                                      \
    ptrToLog(value, "unsigned char *", buffer, bufferSize)

#define FUNCTION_LOG_SIZE_TYPE                                                                                                     \
    size_t
#define FUNCTION_LOG_SIZE_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtSizeToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_UINT_TYPE                                                                                                     \
    unsigned int
#define FUNCTION_LOG_UINT_FORMAT(value, buffer, bufferSize)                                                                        \
    cvtUIntToZ(value, buffer, bufferSize)

#define FUNCTION_LOG_UINTP_TYPE                                                                                                    \
    unsigned int *
#define FUNCTION_LOG_UINTP_FORMAT(value, buffer, bufferSize)                                                                       \
    ptrToLog(value, "unsigned int *", buffer, bufferSize)

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

#define FUNCTION_LOG_VOIDP_TYPE                                                                                                    \
    void *
#define FUNCTION_LOG_VOIDP_FORMAT(value, buffer, bufferSize)                                                                       \
    ptrToLog(value, "void *", buffer, bufferSize)

#define FUNCTION_LOG_CONST_VOIDP_TYPE                                                                                              \
    const void *
#define FUNCTION_LOG_CONST_VOIDP_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_VOIDP_FORMAT(value, buffer, bufferSize)

#define FUNCTION_LOG_VOIDPP_TYPE                                                                                                   \
    void **
#define FUNCTION_LOG_VOIDPP_FORMAT(value, buffer, bufferSize)                                                                      \
    ptrToLog(value, "void **", buffer, bufferSize)

#define FUNCTION_LOG_STRINGZ_TYPE                                                                                                  \
    const char *
#define FUNCTION_LOG_STRINGZ_FORMAT(value, buffer, bufferSize)                                                                     \
    strzToLog(value, buffer, bufferSize)

/***********************************************************************************************************************************
Macros to return function results (or void)
***********************************************************************************************************************************/
#define FUNCTION_LOG_RETURN(typeMacroPrefix, result)                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        FUNCTION_LOG_##typeMacroPrefix##_TYPE FUNCTION_LOG_RETURN_result = result;                                                 \
                                                                                                                                   \
        STACK_TRACE_POP();                                                                                                         \
                                                                                                                                   \
        if (logWill(FUNCTION_LOG_LEVEL()))                                                                                         \
        {                                                                                                                          \
            char buffer[STACK_TRACE_PARAM_MAX];                                                                                    \
                                                                                                                                   \
            FUNCTION_LOG_##typeMacroPrefix##_FORMAT(FUNCTION_LOG_RETURN_result, buffer, sizeof(buffer));                           \
            LOG(FUNCTION_LOG_LEVEL(), 0, "=> %s", buffer);                                                                         \
        }                                                                                                                          \
                                                                                                                                   \
        return FUNCTION_LOG_RETURN_result;                                                                                         \
    }                                                                                                                              \
    while(0)

#define FUNCTION_LOG_RETURN_VOID()                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        STACK_TRACE_POP();                                                                                                         \
                                                                                                                                   \
        LOG_WILL(FUNCTION_LOG_LEVEL(), 0, "=> void");                                                                              \
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
        FUNCTION_LOG_PARAM(typeMacroPrefix, param)

    #define FUNCTION_TEST_PARAM_PTR(typeName, param)                                                                               \
        FUNCTION_LOG_PARAM_PTR(typeName, param)

    #define FUNCTION_TEST_END()                                                                                                    \
        }

    #define FUNCTION_TEST_VOID()                                                                                                   \
        FUNCTION_TEST_BEGIN();                                                                                                     \
        FUNCTION_TEST_END();

    #define FUNCTION_TEST_RETURN(typeMacroPrefix, result)                                                                          \
        do                                                                                                                         \
        {                                                                                                                          \
            FUNCTION_LOG_##typeMacroPrefix##_TYPE FUNCTION_LOG_RETURN_result = result;                                             \
                                                                                                                                   \
            if (stackTraceTest())                                                                                                  \
                STACK_TRACE_POP();                                                                                                 \
                                                                                                                                   \
            return FUNCTION_LOG_RETURN_result;                                                                                     \
        }                                                                                                                          \
        while(0);

    #define FUNCTION_TEST_RETURN_VOID()                                                                                            \
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
    #define FUNCTION_TEST_RETURN(typeMacroPrefix, result)                                                                          \
        return result
    #define FUNCTION_TEST_RETURN_VOID()
#endif

#endif
