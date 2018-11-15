/***********************************************************************************************************************************
IO Write Interface
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoWrite
{
    MemContext *memContext;                                         // Mem context of driver
    void *driver;                                                   // Driver object
    IoWriteInterface interface;                                     // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
    Buffer *output;                                                 // Output buffer

#ifdef DEBUG
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoWrite *
ioWriteNew(void *driver, IoWriteInterface interface)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(IO_WRITE_INTERFACE, interface);

        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(interface.write != NULL);
    FUNCTION_DEBUG_END();

    IoWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoWrite")
    {
        this = memNew(sizeof(IoWrite));
        this->memContext = memContextCurrent();
        this->driver = driver;
        this->interface = interface;
        this->output = bufNew(ioBufferSize());
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_WRITE, this);
}

/***********************************************************************************************************************************
Open the IO
***********************************************************************************************************************************/
void
ioWriteOpen(IoWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(!this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    if (this->interface.open != NULL)
        this->interface.open(this->driver);

    // If no filter group exists create one to do buffering
    if (this->filterGroup == NULL)
        this->filterGroup = ioFilterGroupNew();

    ioFilterGroupOpen(this->filterGroup);

#ifdef DEBUG
    this->opened = true;
#endif

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write data to IO and process filters
***********************************************************************************************************************************/
void
ioWrite(IoWrite *this, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write linefeed-terminated string
***********************************************************************************************************************************/
void
ioWriteLine(IoWrite *this, const String *string)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);
        FUNCTION_DEBUG_PARAM(STRING, string);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(string != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load a buffer with the linefeed-terminated string
        Buffer *buffer = bufNew(strSize(string) + 1);
        memcpy(bufPtr(buffer), strPtr(string), strSize(string));
        bufPtr(buffer)[strSize(string)] = '\n';
        bufUsedSet(buffer, bufSize(buffer));

        // Write the string
        ioWrite(this, buffer);
    }
    MEM_CONTEXT_TEMP_END()

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Flush any data in the output buffer

This does not end writing and if there are filters that are not done it might not have the intended effect.
***********************************************************************************************************************************/
void
ioWriteFlush(IoWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    if (bufUsed(this->output) > 0)
    {
        this->interface.write(this->driver, this->output);
        bufUsedZero(this->output);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Close the IO and write any additional data that has not been written yet
***********************************************************************************************************************************/
void
ioWriteClose(IoWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get/set filters

Filters must be set before open and cannot be reset.
***********************************************************************************************************************************/
const IoFilterGroup *
ioWriteFilterGroup(const IoWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && this->closed);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_FILTER_GROUP, this->filterGroup);
}

void
ioWriteFilterGroupSet(IoWrite *this, IoFilterGroup *filterGroup)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, filterGroup);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(filterGroup != NULL);
        FUNCTION_TEST_ASSERT(this->filterGroup == NULL);
        FUNCTION_TEST_ASSERT(!this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    this->filterGroup = filterGroup;

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioWriteFree(IoWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
