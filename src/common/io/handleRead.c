/***********************************************************************************************************************************
Handle IO Read
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/io/handleRead.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoHandleRead
{
    MemContext *memContext;                                         // Object memory context
    IoRead *io;                                                     // IoRead interface
    const String *name;                                             // Handle name for error messages
    int handle;                                                     // Handle to read data from
    TimeMSec timeout;                                               // Timeout for read operation
    bool eof;                                                       // Has the end of the stream been reached?
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoHandleRead *
ioHandleReadNew(const String *name, int handle, TimeMSec timeout)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, handle);

        FUNCTION_TEST_ASSERT(handle != -1);
    FUNCTION_DEBUG_END();

    IoHandleRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoHandleRead")
    {
        this = memNew(sizeof(IoHandleRead));
        this->memContext = memContextCurrent();
        this->io = ioReadNewP(this, .eof = (IoReadInterfaceEof)ioHandleReadEof, .read = (IoReadInterfaceRead)ioHandleRead);
        this->name = strDup(name);
        this->handle = handle;
        this->timeout = timeout;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_HANDLE_READ, this);
}

/***********************************************************************************************************************************
Read data from the handle
***********************************************************************************************************************************/
size_t
ioHandleRead(IoHandleRead *this, Buffer *buffer, bool block)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_HANDLE_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);
        FUNCTION_DEBUG_PARAM(BOOL, block);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(!bufFull(buffer));
    FUNCTION_DEBUG_END();

    ssize_t actualBytes = 0;

    // If blocking keep reading until buffer is full or eof
    if (!this->eof)
    {
        do
        {
            // Initialize the file descriptor set used for select
            fd_set selectSet;
            FD_ZERO(&selectSet);

            // We know the handle is not negative because it passed error handling, so it is safe to cast to unsigned
            FD_SET((unsigned int)this->handle, &selectSet);

            // Initialize timeout struct used for select.  Recreate this structure each time since Linux (at least) will modify it.
            struct timeval timeoutSelect;
            timeoutSelect.tv_sec = (time_t)(this->timeout / MSEC_PER_SEC);
            timeoutSelect.tv_usec = (time_t)(this->timeout % MSEC_PER_SEC * 1000);

            // Determine if there is data to be read
            int result = select(this->handle + 1, &selectSet, NULL, NULL, &timeoutSelect);
            THROW_ON_SYS_ERROR_FMT(result == -1, FileReadError, "unable to select from %s", strPtr(this->name));

            // If no data read after time allotted then error
            if (!result)
                THROW_FMT(FileReadError, "unable to read data from %s after %" PRIu64 "ms", strPtr(this->name), this->timeout);

            // Read and handle errors
            THROW_ON_SYS_ERROR_FMT(
                (actualBytes = read(this->handle, bufRemainsPtr(buffer), bufRemains(buffer))) == -1, FileReadError,
                "unable to read from %s", strPtr(this->name));

            // Update amount of buffer used
            bufUsedInc(buffer, (size_t)actualBytes);

            // If zero bytes were returned then eof
            if (actualBytes == 0)
                this->eof = true;
        }
        while (bufRemains(buffer) > 0 && !this->eof && block);
    }

    FUNCTION_DEBUG_RESULT(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Move the object to a new context
***********************************************************************************************************************************/
IoHandleRead *
ioHandleReadMove(IoHandleRead *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_READ, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(IO_HANDLE_READ, this);
}

/***********************************************************************************************************************************
Have all bytes been read from the buffer?
***********************************************************************************************************************************/
bool
ioHandleReadEof(const IoHandleRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_HANDLE_READ, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->eof);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoRead *
ioHandleReadIo(const IoHandleRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioHandleReadFree(IoHandleRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_HANDLE_READ, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
