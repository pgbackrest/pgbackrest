/***********************************************************************************************************************************
Block Incremental Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "common/compress/helper.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/bufferWrite.h"
#include "common/log.h"
#include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockIncr
{
    MemContext *memContext;                                         // Mem context of filter

    unsigned int blockNo;                                           // Block number
    size_t inputOffset;                                             // Input offset
    size_t blockSize;                                               // Block size
    Buffer *block;                                                  // Block buffer
    Buffer *blockOut;                                               // Block output buffer
    Buffer *compressed;                                             // Intermediate compressed data

    bool inputSame;                                                 // Input the same data
    bool done;                                                      // Is the filter done?
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
    ASSERT(output != NULL);

    // Is the input done?
    this->done = input == NULL;

    // If still accumulating data in the buffer
    if (!this->done && bufUsed(this->block) < this->blockSize)
    {
        // If all input can be copied
        if (bufUsed(input) - this->inputOffset <= bufRemains(this->block))
        {
            bufCatSub(this->block, input, this->inputOffset, bufUsed(input) - this->inputOffset);
            this->inputOffset = 0;

            // Same input no longer required
            this->inputSame = false;
        }
        // Else only part of the input can be copied
        else
        {
            const size_t copySize = bufRemains(this->block);

            bufCatSub(this->block, input, this->inputOffset, bufRemains(this->block));
            this->inputOffset += copySize;

            // The same input will be needed again to copy the rest
            this->inputSame = true;
        }
    }

    // If the block is full
    if (bufUsed(this->block) == this->blockSize)
    {
        if (bufUsed(this->blockOut) == 0)
        {
            unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
            size_t bufferPos = 0;

            // Write the block number
            cvtUInt64ToVarInt128(this->blockNo, buffer, &bufferPos, sizeof(buffer));
            bufCatC(this->blockOut, buffer, 0, bufferPos);

            // Write the hash !!!
            unsigned char hash[HASH_TYPE_SHA1_SIZE] = {0xFF};
            bufCatC(this->blockOut, hash, 0, HASH_TYPE_SHA1_SIZE);

            // Write compressed size and data
            MEM_CONTEXT_TEMP_BEGIN()
            {
                Buffer *const compressed = bufNew(0);
                IoWrite *const write = ioBufferWriteNew(compressed);

                ioFilterGroupAdd(ioWriteFilterGroup(write), compressFilter(compressTypeGz, 1));
                ioWriteOpen(write);
                ioWrite(write, this->block);
                ioWriteClose(write);

                bufferPos = 0;
                cvtUInt64ToVarInt128(bufUsed(compressed), buffer, &bufferPos, sizeof(buffer));
                bufCatC(this->blockOut, buffer, 0, bufferPos);
                bufCat(this->blockOut, compressed);
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

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

/***********************************************************************************************************************************
Is filter done?
***********************************************************************************************************************************/
static bool
blockIncrDone(const THIS_VOID)
{
    THIS(const BlockIncr);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_INCR, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done && !this->inputSame);
}

/***********************************************************************************************************************************
Should the same input be provided again?
***********************************************************************************************************************************/
static bool
blockIncrInputSame(const THIS_VOID)
{
    THIS(const BlockIncr);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_INCR, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
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
            .block = bufNew(blockSize),
            .blockOut = bufNew(0),
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

        this = ioFilterNewP(
            BLOCK_INCR_FILTER_TYPE, driver, paramList, .done = blockIncrDone, .inOut = blockIncrProcess,
            .inputSame = blockIncrInputSame, .result = blockIncrResult);
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
