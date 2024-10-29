/***********************************************************************************************************************************
Read Chunked I/O
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/limitRead.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoLimitRead
{
    IoRead *read;                                                   // IoRead to read data from
    uint64_t limit;                                                 // Limit of data to read
    uint64_t current;                                               // Current out of data read
} IoLimitRead;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_LIMIT_READ_TYPE                                                                                            \
    IoLimitRead *
#define FUNCTION_LOG_IO_LIMIT_READ_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(value, "IoLimitRead", buffer, bufferSize)

/***********************************************************************************************************************************
Read next chunk or partial chunk
***********************************************************************************************************************************/
static size_t
ioLimitRead(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(IoLimitRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_LIMIT_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    // Continue reading until limit
    const size_t result =
        this->limit - this->current >= bufRemains(buffer) ? bufRemains(buffer) : (size_t)(this->limit - this->current);

    bufLimitSet(buffer, bufUsed(buffer) + result);
    ioRead(this->read, buffer);

    // Update current read
    this->current += result;

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Have all chunks been read?
***********************************************************************************************************************************/
static bool
ioLimitReadEof(THIS_VOID)
{
    THIS(IoLimitRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_LIMIT_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->limit == this->current);
}

/**********************************************************************************************************************************/
FN_EXTERN IoRead *
ioLimitReadNew(IoRead *const read, const uint64_t limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(UINT64, limit);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    OBJ_NEW_BEGIN(IoLimitRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoLimitRead)
        {
            .read = read,
            .limit = limit,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_READ, ioReadNewP(this, .eof = ioLimitReadEof, .read = ioLimitRead));
}
