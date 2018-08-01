/***********************************************************************************************************************************
IO Read Interface
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoRead
{
    MemContext *memContext;                                         // Mem context of driver
    void *driver;                                                   // Driver object
    IoFilterGroup *filterGroup;                                     // IO filters
    Buffer *input;                                                  // Input buffer

    IoReadOpen open;                                                // Driver open
    IoReadProcess read;                                             // Driver read
    IoReadClose close;                                              // Driver close
    IoReadEof eof;                                                  // Driver eof

    bool eofAll;                                                    // Is the read done (read and filters complete)?

#ifdef DEBUG
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
};

/***********************************************************************************************************************************
New object

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoRead *
ioReadNew(void *driver, IoReadOpen open, IoReadProcess read, IoReadClose close, IoReadEof eof)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, open);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, read);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, close);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, eof);

        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(read != NULL);
    FUNCTION_DEBUG_END();

    IoRead *this = memNew(sizeof(IoRead));
    this->memContext = memContextCurrent();
    this->input = bufNew(ioBufferSize());

    this->driver = driver;
    this->open = open;
    this->read = read;
    this->close = close;
    this->eof = eof;

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
    bool result = this->open != NULL ? this->open(this->driver) : true;

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

    FUNCTION_DEBUG_RESULT(BOOL, this->eof != NULL ? this->eof(this->driver) : false);
}

/***********************************************************************************************************************************
Read data from IO and process filters
***********************************************************************************************************************************/
size_t
ioRead(IoRead *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(this->opened && !this->closed);
    FUNCTION_DEBUG_END();

    // Loop until EOF or the output buffer is full
    size_t outputRemains = bufRemains(buffer);

    while (!this->eofAll && bufRemains(buffer) > 0)
    {
        // Process input buffer again to get more output
        if (ioFilterGroupInputSame(this->filterGroup))
            ioFilterGroupProcess(this->filterGroup, this->input, buffer);

        // Else new input can be accepted
        else
        {
            // Read if not EOF
            if (!ioReadEofDriver(this))
            {
                bufUsedZero(this->input);
                this->read(this->driver, this->input);
            }
            else
                this->input = NULL;

            // Process the input buffer (or flush if NULL)
            ioFilterGroupProcess(this->filterGroup, this->input, buffer);
        }

        // Eof when no more input and the filter group is done
        this->eofAll = ioReadEofDriver(this) && ioFilterGroupDone(this->filterGroup);
    }

    FUNCTION_DEBUG_RESULT(SIZE, outputRemains - bufRemains(buffer));
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
    if (this->close != NULL)
        this->close(this->driver);

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
