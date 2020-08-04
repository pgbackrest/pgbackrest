/***********************************************************************************************************************************
IO Read Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoSession
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    IoSessionInterface interface;                                      // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
    Buffer *input;                                                  // Input buffer
    Buffer *output;                                                 // Output buffer (holds extra data from line read)

    bool eofAll;                                                    // Is the read done (read and filters complete)?

#ifdef DEBUG
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

OBJECT_DEFINE_FREE(IO_SESSION);

/**********************************************************************************************************************************/
IoSession *
ioSessionNew(void *driver, IoSessionInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_SESSION_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface.read != NULL);

    IoSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSession")
    {
        this = memNew(sizeof(IoSession));

        *this = (IoSession)
        {
            .memContext = memContextCurrent(),
            .driver = driver,
            .interface = interface,
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}

/***********************************************************************************************************************************
Is the driver at EOF?

This is different from the overall eof because filters may still be holding buffered data.
***********************************************************************************************************************************/
static bool
ioSessionEofDriver(const IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_LOG_RETURN(BOOL, this->interface.eof != NULL ? this->interface.eof(this->driver) : false);
}

/**********************************************************************************************************************************/
static void
ioSessionInternal(IoSession *this, Buffer *buffer, bool block)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->opened && !this->closed);

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
            if (this->input != NULL)
            {
                if (!ioSessionEofDriver(this))
                {
                    bufUsedZero(this->input);

                    // If blocking then limit the amount of data requested
                    if (ioSessionBlock(this) && bufRemains(this->input) > bufRemains(buffer))
                        bufLimitSet(this->input, bufRemains(buffer));

                    this->interface.read(this->driver, this->input, block);
                    bufLimitClear(this->input);
                }
                // Set input to NULL and flush (no need to actually free the buffer here as it will be freed with the mem context)
                else
                    this->input = NULL;
            }

            // Process the input buffer (or flush if NULL)
            if (this->input == NULL || bufUsed(this->input) > 0)
                ioFilterGroupProcess(this->filterGroup, this->input, buffer);

            // Stop if not blocking -- we don't need to fill the buffer as long as we got some data
            if (!block && bufUsed(buffer) > bufferUsedBegin)
                break;
        }

        // Eof when no more input and the filter group is done
        this->eofAll = ioSessionEofDriver(this) && ioFilterGroupDone(this->filterGroup);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Read data and use buffered line read output when present
***********************************************************************************************************************************/
size_t
ioSession(IoSession *this, Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BUFFER, this->output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->opened && !this->closed);

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
    ioSessionInternal(this, buffer, true);

    FUNCTION_LOG_RETURN(SIZE, outputRemains - bufRemains(buffer));
}

/***********************************************************************************************************************************
The entire string to search for must fit within a single buffer.
***********************************************************************************************************************************/
String *
ioSessionLineParam(IoSession *this, bool allowEof)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
        FUNCTION_LOG_PARAM(BOOL, allowEof);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

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

            if (ioSessionEof(this))
            {
                if (allowEof)
                    result = strNewN((char *)bufPtr(this->output), bufUsed(this->output));
                else
                    THROW(FileReadError, "unexpected eof while reading line");
            }
            else
                ioSessionInternal(this, this->output, false);
        }
    }
    while (result == NULL);

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
ioSessionLine(IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(STRING, ioSessionLineParam(this, false));
}

/**********************************************************************************************************************************/
void
ioSessionClose(IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

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

/**********************************************************************************************************************************/
bool
ioSessionBlock(const IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface.block);
}

/**********************************************************************************************************************************/
void *
ioSessionDriver(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->driver);
}

/**********************************************************************************************************************************/
bool
ioSessionEof(const IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_LOG_RETURN(BOOL, this->eofAll);
}

/**********************************************************************************************************************************/
IoFilterGroup *
ioSessionFilterGroup(const IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filterGroup);
}

/**********************************************************************************************************************************/
int
ioSessionHandle(const IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INT, this->interface.handle == NULL ? -1 : this->interface.handle(this->driver));
}

/**********************************************************************************************************************************/
const IoSessionInterface *
ioSessionInterface(const IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(&this->interface);
}
