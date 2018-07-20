/***********************************************************************************************************************************
IO Write
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
IO write object
***********************************************************************************************************************************/
struct IoWrite
{
    MemContext *memContext;                                         // Mem context of driver
    void *driver;                                                   // Driver object
    IoWriteOpen open;                                               // Driver open
    IoWriteProcess process;                                         // Driver write
    IoWriteClose close;                                             // Driver close
    size_t size;                                                    // Total bytes written
};

/***********************************************************************************************************************************
Create a new write IO

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoWrite *
ioWriteNew(void *driver, IoWriteOpen open, IoWriteProcess process, IoWriteClose close)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, open);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, process);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, close);

        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(process != NULL);
    FUNCTION_DEBUG_END();

    IoWrite *this = memNew(sizeof(IoWrite));
    this->memContext = memContextCurrent();
    this->driver = driver;
    this->open = open;
    this->process = process;
    this->close = close;

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
    FUNCTION_DEBUG_END();

    if (this->open != NULL)
        this->open(this->driver);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write data to IO
***********************************************************************************************************************************/
void
ioWrite(IoWrite *this, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Only write if there is data to write
    if (buffer != NULL && bufSize(buffer) > 0)
    {
        this->process(this->driver, buffer);
        this->size += bufUsed(buffer);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Close the IO
***********************************************************************************************************************************/
void
ioWriteClose(IoWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    if (this->close != NULL)
        this->close(this->driver);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Total bytes written
***********************************************************************************************************************************/
size_t
ioWriteSize(const IoWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->size);
}
