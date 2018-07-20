/***********************************************************************************************************************************
IO Read
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
IO read object
***********************************************************************************************************************************/
struct IoRead
{
    MemContext *memContext;                                         // Mem context of driver
    void *driver;                                                   // Driver object
    IoReadOpen open;                                                // Driver open
    IoReadProcess process;                                          // Driver read
    IoReadClose close;                                              // Driver close
    IoReadEof eof;                                                  // Driver eof
    size_t size;                                                    // Total bytes read
};

/***********************************************************************************************************************************
Create a new read IO

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoRead *
ioReadNew(void *driver, IoReadOpen open, IoReadProcess process, IoReadClose close, IoReadEof eof)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, open);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, process);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, close);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, eof);

        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(process != NULL);
    FUNCTION_DEBUG_END();

    IoRead *this = memNew(sizeof(IoRead));
    this->memContext = memContextCurrent();
    this->driver = driver;
    this->open = open;
    this->process = process;
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
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->open != NULL ? this->open(this->driver) : true);
}

/***********************************************************************************************************************************
Read data from IO
***********************************************************************************************************************************/
size_t
ioRead(IoRead *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    size_t result = this->process(this->driver, buffer);
    this->size += result;

    FUNCTION_DEBUG_RESULT(SIZE, result);
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
    FUNCTION_DEBUG_END();

    if (this->close != NULL)
        this->close(this->driver);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Is IO at EOF?
***********************************************************************************************************************************/
bool
ioReadEof(const IoRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->eof != NULL ? this->eof(this->driver) : false);
}

/***********************************************************************************************************************************
Total bytes read
***********************************************************************************************************************************/
size_t
ioReadSize(const IoRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->size);
}
