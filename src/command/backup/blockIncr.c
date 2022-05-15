/***********************************************************************************************************************************
Block Incremental Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockIncr
{
    MemContext *memContext;                                         // Mem context of filter

    size_t blockSize;                                               // Block size
} BlockIncr;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *
blockIncrToLog(const BlockIncr *this)
{
    return strNewFmt("{blockSize: %zu}", this->blockSize);
}

#define FUNCTION_LOG_BLOCK_INCR_TYPE                                                                                               \
    BlockIncr *
#define FUNCTION_LOG_BLOCK_INCR_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, blockIncrToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
static void
blockIncrProcess(THIS_VOID, const Buffer *const input, Buffer *const output)
{
    THIS(BlockIncr);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_INCR, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    // !!!

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
static Pack *
blockIncrResult(THIS_VOID)
{
    THIS(BlockIncr);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_INCR, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    // !!!

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
IoFilter *
blockIncrNew(const size_t blockSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(BlockIncr)
    {
        BlockIncr *driver = OBJ_NEW_ALLOC();

        *driver = (BlockIncr)
        {
            .memContext = memContextCurrent(),
            .blockSize = blockSize,
        };

        // Create param list
        Pack *paramList = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewP();

            pckWriteU64P(packWrite, blockSize);
            pckWriteEndP(packWrite);

            paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();

        this = ioFilterNewP(BLOCK_INCR_FILTER_TYPE, driver, paramList, .inOut = blockIncrProcess, .result = blockIncrResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
blockIncrNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const size_t blockSize = (size_t)pckReadU64P(paramListPack);

        result = ioFilterMove(blockIncrNew(blockSize), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
