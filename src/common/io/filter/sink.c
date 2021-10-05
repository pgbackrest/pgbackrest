/***********************************************************************************************************************************
IO Sink Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/sink.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSink
{
    bool dummy;                                                     // Struct requires one member
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

    OBJ_NEW_BEGIN(IoSink)
    {
        IoSink *driver = OBJ_NEW_ALLOC();
        this = ioFilterNewP(SINK_FILTER_TYPE, driver, NULL, .inOut = ioSinkProcess);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
