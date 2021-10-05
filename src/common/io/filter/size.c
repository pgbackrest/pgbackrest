/***********************************************************************************************************************************
IO Size Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/size.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSize
{
    uint64_t size;                                                  // Total size of al input
} IoSize;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *
ioSizeToLog(const IoSize *this)
{
    return strNewFmt("{size: %" PRIu64 "}", this->size);
}

#define FUNCTION_LOG_IO_SIZE_TYPE                                                                                                  \
    IoSize *
#define FUNCTION_LOG_IO_SIZE_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioSizeToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
static void
ioSizeProcess(THIS_VOID, const Buffer *input)
{
    THIS(IoSize);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    this->size += bufUsed(input);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
static Pack *
ioSizeResult(THIS_VOID)
{
    THIS(IoSize);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU64P(packWrite, this->size);
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
IoFilter *
ioSizeNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(IoSize)
    {
        IoSize *driver = OBJ_NEW_ALLOC();
        *driver = (IoSize){0};

        this = ioFilterNewP(SIZE_FILTER_TYPE, driver, NULL, .in = ioSizeProcess, .result = ioSizeResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
