/***********************************************************************************************************************************
IO Size Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/size.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(SIZE_FILTER_TYPE_STR,                                 SIZE_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSize
{
    MemContext *memContext;                                         // Mem context of filter

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
static Variant *
ioSizeResult(THIS_VOID)
{
    THIS(IoSize);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(VARIANT, varNewUInt64(this->size));
}

/**********************************************************************************************************************************/
IoFilter *
ioSizeNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSize")
    {
        IoSize *driver = memNew(sizeof(IoSize));

        *driver = (IoSize)
        {
            .memContext = memContextCurrent(),
        };

        this = ioFilterNewP(SIZE_FILTER_TYPE_STR, driver, NULL, .in = ioSizeProcess, .result = ioSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
