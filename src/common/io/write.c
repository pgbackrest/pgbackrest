/***********************************************************************************************************************************
IO Write Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoWrite
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    IoWriteInterface interface;                                     // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
    Buffer *output;                                                 // Output buffer

#ifdef DEBUG
    bool filterGroupSet;                                            // Were filters set?
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

OBJECT_DEFINE_FREE(IO_WRITE);

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoWrite *
ioWriteNew(void *driver, IoWriteInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface.write != NULL);

    IoWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoWrite")
    {
        this = memNew(sizeof(IoWrite));

        *this = (IoWrite)
        {
            .memContext = memContextCurrent(),
            .driver = driver,
            .interface = interface,
            .filterGroup = ioFilterGroupNew(),
            .output = bufNew(ioBufferSize()),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, this);
}

/***********************************************************************************************************************************
Open the IO
***********************************************************************************************************************************/
void
ioWriteOpen(IoWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->opened && !this->closed);

    if (this->interface.open != NULL)
        this->interface.open(this->driver);

    // Track whether filters were added to prevent flush() from being called later since flush() won't work with most filters
#ifdef DEBUG
    this->filterGroupSet = ioFilterGroupSize(this->filterGroup) > 0;
#endif

    // Open the filter group
    ioFilterGroupOpen(this->filterGroup);

#ifdef DEBUG
    this->opened = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write data to IO and process filters
***********************************************************************************************************************************/
void
ioWrite(IoWrite *this, const Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    // Only write if there is data to write
    if (buffer != NULL && bufUsed(buffer) > 0)
    {
        do
        {
            ioFilterGroupProcess(this->filterGroup, buffer, this->output);

            // Write data if the buffer is full
            if (bufRemains(this->output) == 0)
            {
                this->interface.write(this->driver, this->output);
                bufUsedZero(this->output);
            }
        }
        while (ioFilterGroupInputSame(this->filterGroup));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write linefeed-terminated string
***********************************************************************************************************************************/
void
ioWriteLine(IoWrite *this, const Buffer *buffer)
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

/***********************************************************************************************************************************
Write string
***********************************************************************************************************************************/
void
ioWriteStr(IoWrite *this, const String *string)
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

/***********************************************************************************************************************************
Write linefeed-terminated string
***********************************************************************************************************************************/
void
ioWriteStrLine(IoWrite *this, const String *string)
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

/***********************************************************************************************************************************
Flush any data in the output buffer

This does not end writing and if there are filters that are not done it might not have the intended effect.
***********************************************************************************************************************************/
void
ioWriteFlush(IoWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);
    ASSERT(!this->filterGroupSet);

    if (bufUsed(this->output) > 0)
    {
        this->interface.write(this->driver, this->output);
        bufUsedZero(this->output);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the IO and write any additional data that has not been written yet
***********************************************************************************************************************************/
void
ioWriteClose(IoWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    // Flush remaining data
    do
    {
        ioFilterGroupProcess(this->filterGroup, NULL, this->output);

        // Write data if the buffer is full or if this is the last buffer to be written
        if (bufRemains(this->output) == 0 || (ioFilterGroupDone(this->filterGroup) && bufUsed(this->output) > 0))
        {
            this->interface.write(this->driver, this->output);
            bufUsedZero(this->output);
        }
    }
    while (!ioFilterGroupDone(this->filterGroup));

    // Close the filter group and gather results
    ioFilterGroupClose(this->filterGroup);

    // Close the driver if there is a close function
    if (this->interface.close != NULL)
        this->interface.close(this->driver);

#ifdef DEBUG
    this->closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get/set filters

Filters must be set before open and cannot be reset.
***********************************************************************************************************************************/
IoFilterGroup *
ioWriteFilterGroup(const IoWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filterGroup);
}

/***********************************************************************************************************************************
Handle (file descriptor) for the write object

No all write objects have a handle and -1 will be returned in that case.
***********************************************************************************************************************************/
int
ioWriteHandle(const IoWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INT, this->interface.handle == NULL ? -1 : this->interface.handle(this->driver));
}
