/***********************************************************************************************************************************
IO Sink Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/sink.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(SINK_FILTER_TYPE_STR,                                 SINK_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSink
{
    MemContext *memContext;                                         // Mem context of filter
} IoSink;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_SINK_TYPE                                                                                                  \
    IoSink *
#define FUNCTION_LOG_IO_SINK_FORMAT(value, buffer, bufferSize)                                                                     \
    objToLog(value, "IoSink", buffer, bufferSize)

/***********************************************************************************************************************************
Discard all input
***********************************************************************************************************************************/
static void
ioSinkProcess(THIS_VOID, const Buffer *input, Buffer *output)
{
    THIS(IoSink);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SINK, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoFilter *
ioSinkNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSink")
    {
        IoSink *driver = memNew(sizeof(IoSink));

        *driver = (IoSink)
        {
            .memContext = memContextCurrent(),
        };

        this = ioFilterNewP(SINK_FILTER_TYPE_STR, driver, NULL, .inOut = ioSinkProcess);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
