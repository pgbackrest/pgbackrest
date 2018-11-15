/***********************************************************************************************************************************
IO Read Interface
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoRead
{
    MemContext *memContext;                                         // Mem context of driver
    void *driver;                                                   // Driver object
    IoReadInterface interface;                                      // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
    Buffer *input;                                                  // Input buffer
    Buffer *output;                                                 // Output buffer (holds extra data from line read)

    bool eofAll;                                                    // Is the read done (read and filters complete)?

#ifdef DEBUG
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoRead *
ioReadNew(void *driver, IoReadInterface interface)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(IO_READ_INTERFACE, interface);

        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(interface.read != NULL);
    FUNCTION_DEBUG_END();

    IoRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoRead")
    {
        this = memNew(sizeof(IoRead));
        this->memContext = memContextCurrent();
        this->driver = driver;
        this->interface = interface;
        this->input = bufNew(ioBufferSize());
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_READ, this);
}

/***********************************************************************************************************************************
Open the IO
***********************************************************************************************************************************/
bool
ioReadOpen(IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(!this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    // Open if the driver has an open function
    bool result = this->interface.open != NULL ? this->interface.open(this->driver) : true;

    // Only open the filter group if the read was opened
    if (result)
    {
        // If no filter group exists create one to do buffering
        if (this->filterGroup == NULL)
            this->filterGroup = ioFilterGroupNew();

        ioFilterGroupOpen(this->filterGroup);
    }

#ifdef DEBUG
    this->opened = result;
#endif

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Is the driver at EOF?

This is different from the overall eof because filters may still be holding buffered data.
***********************************************************************************************************************************/
static bool
ioReadEofDriver(const IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->interface.eof != NULL ? this->interface.eof(this->driver) : false);
}

/***********************************************************************************************************************************
Read data from IO and process filters
***********************************************************************************************************************************/
static void
ioReadInternal(IoRead *this, Buffer *buffer, bool relaxed)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);
        FUNCTION_DEBUG_PARAM(BOOL, relaxed);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    // Loop until EOF or the output buffer is full
    size_t bufferUsedBegin = bufUsed(buffer);

    while (!this->eofAll && bufRemains(buffer) > 0)
    {
        // Process input buffer again to get more output
        if (ioFilterGroupInputSame(this->filterGroup))
        {
            ioFilterGroupProcess(this->filterGroup, this->input, buffer);
        }
        // Else new input can be accepted
        else
        {
            // Read if not EOF
            if (!ioReadEofDriver(this))
            {
                bufUsedZero(this->input);
                this->interface.read(this->driver, this->input);
            }
            // Set input to NULL and flush (no need to actually free the buffer here as it will be freed with the mem context)
            else
                this->input = NULL;

            // Process the input buffer (or flush if NULL)
            ioFilterGroupProcess(this->filterGroup, this->input, buffer);

            // Stop if relaxed read -- we don't need to fill the buffer as long as we got some data
            if (relaxed && bufUsed(buffer) > bufferUsedBegin)
                break;
        }

        // Eof when no more input and the filter group is done
        this->eofAll = ioReadEofDriver(this) && ioFilterGroupDone(this->filterGroup);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Read data and use buffered line read output when present
***********************************************************************************************************************************/
size_t
ioRead(IoRead *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);
        FUNCTION_DEBUG_PARAM(BUFFER, this->output);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    // Store size of remaining portion of buffer to calculate total read at the end
    size_t outputRemains = bufRemains(buffer);

    // Use any data in the output buffer left over from a line read
    if (this->output != NULL && bufUsed(this->output) > 0 && bufRemains(buffer) > 0)
    {
        // Determine how much data should be copied
        size_t size = bufUsed(this->output) > bufRemains(buffer) ? bufRemains(buffer) : bufUsed(this->output);

        // Copy data to the user buffer
        bufCatSub(buffer, this->output, 0, size);

        // Remove copied data from the output buffer
        memmove(bufPtr(this->output), bufPtr(this->output) + size, bufUsed(this->output) - size);
        bufUsedSet(this->output, bufUsed(this->output) - size);
    }

    // Read data
    ioReadInternal(this, buffer, false);

    FUNCTION_DEBUG_RESULT(SIZE, outputRemains - bufRemains(buffer));
}

/***********************************************************************************************************************************
Read linefeed-terminated string

The entire string to search for must fit within a single buffer.
***********************************************************************************************************************************/
String *
ioReadLine(IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, this->output);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    // Allocate the output buffer if it has not already been allocated.  This buffer is not allocated at object creation because it
    // is not always used.
    if (this->output == NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->output = bufNew(ioBufferSize());
        }
        MEM_CONTEXT_END();
    }

    // Search for a linefeed
    String *result = NULL;

    do
    {
        if (bufUsed(this->output) > 0)
        {
            // Search for a linefeed in the buffer
            char *linefeed = memchr(bufPtr(this->output), '\n', bufUsed(this->output));

            // A linefeed was found so get the string
            if (linefeed != NULL)
            {
                // Get the string size
                size_t size = (size_t)(linefeed - (char *)bufPtr(this->output) + 1);

                // Create the string
                result = strNewN((char *)bufPtr(this->output), size - 1);

                // Remove string from the output buffer
                memmove(bufPtr(this->output), bufPtr(this->output) + size, bufUsed(this->output) - size);
                bufUsedSet(this->output, bufUsed(this->output) - size);
            }
        }

        // Read data if no linefeed was found in the existing buffer
        if (result == NULL)
        {
            if (bufFull(this->output))
                THROW_FMT(FileReadError, "unable to find line in %zu byte buffer", bufSize(this->output));

            if (ioReadEof(this))
                THROW(FileReadError, "unexpected eof while reading line");

            ioReadInternal(this, this->output, 1);
        }
    }
    while (result == NULL);

    FUNCTION_DEBUG_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Close the IO
***********************************************************************************************************************************/
void
ioReadClose(IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

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
Is IO at EOF?

All driver reads are complete and all data has been flushed from the filters (if any).
***********************************************************************************************************************************/
bool
ioReadEof(const IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->eofAll);
}

/***********************************************************************************************************************************
Get/set filters

Filters must be set before open and cannot be reset.
***********************************************************************************************************************************/
const IoFilterGroup *
ioReadFilterGroup(const IoRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_FILTER_GROUP, this->filterGroup);
}

void
ioReadFilterGroupSet(IoRead *this, IoFilterGroup *filterGroup)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);
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
ioReadFree(IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
