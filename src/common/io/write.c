/***********************************************************************************************************************************
IO Write Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/write.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoWrite
{
    IoWritePub pub;                                                 // Publicly accessible variables
    Buffer *output;                                                 // Output buffer

#ifdef DEBUG
    bool filterGroupSet;                                            // Were filters set?
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

/**********************************************************************************************************************************/
FN_EXTERN IoWrite *
ioWriteNew(void *const driver, const IoWriteInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface.write != NULL);

    OBJ_NEW_BEGIN(IoWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoWrite)
        {
            .pub =
            {
                .driver = objMoveToInterface(driver, this, memContextPrior()),
                .interface = interface,
                .filterGroup = ioFilterGroupNew(),
            },
            .output = bufNew(ioBufferSize()),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteOpen(IoWrite *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->opened && !this->closed);

    if (ioWriteInterface(this)->open != NULL)
        ioWriteInterface(this)->open(ioWriteDriver(this));

    // Track whether filters were added to prevent flush() from being called later since flush() won't work with most filters
#ifdef DEBUG
    this->filterGroupSet = ioFilterGroupSize(this->pub.filterGroup) > 0;
#endif

    // Open the filter group
    ioFilterGroupOpen(this->pub.filterGroup);

#ifdef DEBUG
    this->opened = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWrite(IoWrite *const this, const Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    // Only write if there is data to write
    if (buffer != NULL && !bufEmpty(buffer))
    {
        do
        {
            ioFilterGroupProcess(this->pub.filterGroup, buffer, this->output);

            // Write data if the buffer is full
            if (bufRemains(this->output) == 0)
            {
                ioWriteInterface(this)->write(ioWriteDriver(this), this->output);
                bufUsedZero(this->output);
            }
        }
        while (ioFilterGroupInputSame(this->pub.filterGroup));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteLine(IoWrite *const this, const Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    ioWrite(this, buffer);
    ioWrite(this, LF_BUF);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN bool
ioWriteReady(IoWrite *const this, const IoWriteReadyParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(BOOL, param.error);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = true;

    if (ioWriteInterface(this)->ready != NULL)
        result = ioWriteInterface(this)->ready(ioWriteDriver(this), param.error);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteStr(IoWrite *const this, const String *const string)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(STRING, string);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    ioWrite(this, BUFSTR(string));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteStrLine(IoWrite *const this, const String *const string)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(STRING, string);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    ioWrite(this, BUFSTR(string));
    ioWrite(this, LF_BUF);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteVarIntU64(IoWrite *const this, const uint64_t value)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(UINT64, value);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
    size_t bufferPos = 0;

    cvtUInt64ToVarInt128(value, buffer, &bufferPos, sizeof(buffer));
    ioWrite(this, BUF(buffer, bufferPos));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteFlush(IoWrite *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);
    ASSERT(!this->filterGroupSet);

    if (!bufEmpty(this->output))
    {
        ioWriteInterface(this)->write(ioWriteDriver(this), this->output);
        bufUsedZero(this->output);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioWriteClose(IoWrite *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    // Flush remaining data
    do
    {
        ioFilterGroupProcess(this->pub.filterGroup, NULL, this->output);

        // Write data if the buffer is full or if this is the last buffer to be written
        if (bufRemains(this->output) == 0 || (ioFilterGroupDone(this->pub.filterGroup) && !bufEmpty(this->output)))
        {
            ioWriteInterface(this)->write(ioWriteDriver(this), this->output);
            bufUsedZero(this->output);
        }
    }
    while (!ioFilterGroupDone(this->pub.filterGroup));

    // Close the filter group and gather results
    ioFilterGroupClose(this->pub.filterGroup);

    // Close the driver if there is a close function
    if (ioWriteInterface(this)->close != NULL)
        ioWriteInterface(this)->close(ioWriteDriver(this));

#ifdef DEBUG
    this->closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN int
ioWriteFd(const IoWrite *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INT, ioWriteInterface(this)->fd == NULL ? -1 : ioWriteInterface(this)->fd(ioWriteDriver(this)));
}
