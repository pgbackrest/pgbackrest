/***********************************************************************************************************************************
Block Incremental Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "command/backup/blockMap.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/chunk.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockIncr
{
    MemContext *memContext;                                         // Mem context of filter

    unsigned int reference;                                         // Current backup reference
    uint64_t bundleId;                                              // Bundle id

    StringId compressType;                                          // Compress filter type
    const Pack *compressParam;                                      // Compress filter parameters
    const Pack *encryptParam;                                       // Encrypt filter parameters

    unsigned int blockNo;                                           // Block number
    unsigned int blockNoLast;                                       // Last block no
    uint64_t blockOffset;                                           // Block offset
    size_t blockSize;                                               // Block size
    Buffer *block;                                                  // Block buffer

    Buffer *blockOut;                                               // Block output buffer
    size_t blockOutOffset;                                          // Block output offset (already copied to output buffer)

    const BlockMap *blockMapPrior;                                  // Prior block map
    BlockMap *blockMapOut;                                          // Output block map
    uint64_t blockMapOutSize;                                       // Output block map size (if any)

    size_t inputOffset;                                             // Input offset
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

    // Loop until the input is consumed or there is output
    do
    {
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

        // If done or block is full
        if (this->done || bufUsed(this->block) == this->blockSize)
        {
            if (bufUsed(this->blockOut) == 0)
            {
                if (bufUsed(this->block) > 0)
                {
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        // Get block checksum
                        const Buffer *const checksum = cryptoHashOne(hashTypeSha1, this->block);

                        // Does the block exist in the input map?
                        const BlockMapItem *const blockMapItemIn =
                            this->blockMapPrior != NULL && this->blockNo < blockMapSize(this->blockMapPrior) ?
                                blockMapGet(this->blockMapPrior, this->blockNo) : NULL;

                        // If the block is new or has changed then write it
                        if (blockMapItemIn == NULL ||
                            memcmp(blockMapItemIn->checksum, bufPtrConst(checksum), bufUsed(checksum)) != 0)
                        {
                            IoWrite *const write = ioBufferWriteNew(this->blockOut);
                            bool bufferRequired = true;

                            // Add compress filter
                            if (this->compressParam != NULL)
                            {
                                ioFilterGroupAdd(
                                    ioWriteFilterGroup(write), compressFilterPack(this->compressType, this->compressParam));
                                bufferRequired = false;
                            }

                            // Add encrypt filter
                            if (this->encryptParam != NULL)
                            {
                                ioFilterGroupAdd(ioWriteFilterGroup(write), cipherBlockNewPack(this->encryptParam));
                                bufferRequired = false;
                            }

                            // If no compress/encrypt then add a buffer so chunk sizes are as large as possible
                            if (bufferRequired)
                                ioFilterGroupAdd(ioWriteFilterGroup(write), ioBufferNew());

                            // Add chunk and size filters
                            ioFilterGroupAdd(ioWriteFilterGroup(write), ioChunkNew());
                            ioFilterGroupAdd(ioWriteFilterGroup(write), ioSizeNew());
                            ioWriteOpen(write);

                            // Write the block no as a delta of the prior block no
                            ioWriteVarIntU64(write, this->blockNo - this->blockNoLast);

                            // Copy block data through the filters and close
                            ioCopyP(ioBufferReadNewOpen(this->block), write);
                            ioWriteClose(write);

                            // Write to block map
                            BlockMapItem blockMapItem =
                            {
                                .reference = this->reference,
                                .bundleId = this->bundleId,
                                .offset = this->blockOffset,
                                .size = pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), SIZE_FILTER_TYPE)),
                            };

                            memcpy(blockMapItem.checksum, bufPtrConst(checksum), bufUsed(checksum));
                            blockMapAdd(this->blockMapOut, &blockMapItem);

                            // Increment block offset and last block no
                            this->blockOffset += blockMapItem.size;
                            this->blockNoLast = this->blockNo;
                        }
                        // Else write a reference to the block in the prior backup
                        else
                        {
                            blockMapAdd(this->blockMapOut, blockMapItemIn);
                            bufUsedZero(this->block);
                        }

                        this->blockNo++;
                    }
                    MEM_CONTEXT_TEMP_END();
                }

                // Write the block map if done processing and at least one block was written
                if (this->done && this->blockNo > 0)
                {
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        // Size of block output before starting to write the map
                        const size_t blockOutBegin = bufUsed(this->blockOut);

                        // Write the map
                        IoWrite *const write = ioBufferWriteNew(this->blockOut);

                        if (this->encryptParam != NULL)
                            ioFilterGroupAdd(ioWriteFilterGroup(write), cipherBlockNewPack(this->encryptParam));

                        // Write the map
                        ioWriteOpen(write);
                        blockMapWrite(this->blockMapOut, write);
                        ioWriteClose(write);

                        // Get total bytes written for the map
                        this->blockMapOutSize = bufUsed(this->blockOut) - blockOutBegin;
                    }
                    MEM_CONTEXT_TEMP_END();
                }
            }

            // Copy to output buffer
            const size_t blockOutSize = bufUsed(this->blockOut) - this->blockOutOffset;

            if (blockOutSize > 0)
            {
                // Output the rest of the block if it will fit
                if (bufRemains(output) >= blockOutSize)
                {
                    bufCatSub(output, this->blockOut, this->blockOutOffset, blockOutSize);
                    bufUsedZero(this->blockOut);
                    bufUsedZero(this->block);

                    this->blockOutOffset = 0;
                    this->inputSame = this->inputOffset != 0;
                }
                // Else output as much of the block as possible
                else
                {
                    const size_t blockOutSize = bufRemains(output);
                    bufCatSub(output, this->blockOut, this->blockOutOffset, blockOutSize);

                    this->blockOutOffset += blockOutSize;
                    this->inputSame = true;
                }
            }
        }
    }
    while (this->inputSame && bufEmpty(output));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
The result is the size of the block map
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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU64P(packWrite, this->blockMapOutSize);
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

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
blockIncrNew(
    const size_t blockSize, const unsigned int reference, const uint64_t bundleId, const uint64_t bundleOffset,
    const Buffer *const blockMapPrior, const IoFilter *const compress, const IoFilter *const encrypt)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(UINT, reference);
        FUNCTION_LOG_PARAM(UINT64, bundleId);
        FUNCTION_LOG_PARAM(UINT64, bundleOffset);
        FUNCTION_LOG_PARAM(BUFFER, blockMapPrior);
        FUNCTION_LOG_PARAM(IO_FILTER, compress);
        FUNCTION_LOG_PARAM(IO_FILTER, encrypt);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(BlockIncr, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        BlockIncr *const driver = OBJ_NEW_ALLOC();

        *driver = (BlockIncr)
        {
            .memContext = memContextCurrent(),
            .blockSize = blockSize,
            .reference = reference,
            .bundleId = bundleId,
            .blockOffset = bundleOffset,
            .block = bufNew(blockSize),
            .blockOut = bufNew(0),
            .blockMapOut = blockMapNew(),
        };

        // Duplicate compress filter
        if (compress != NULL)
        {
            driver->compressType = ioFilterType(compress);
            driver->compressParam = pckDup(ioFilterParamList(compress));
        }

        // Duplicate encrypt filter
        if (encrypt != NULL)
            driver->encryptParam = pckDup(ioFilterParamList(encrypt));

        // Load prior block map
        if (blockMapPrior)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                IoRead *const read = ioBufferReadNewOpen(blockMapPrior);

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    driver->blockMapPrior =  blockMapNewRead(read);
                }
                MEM_CONTEXT_PRIOR_END();
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Create param list
        Pack *paramList = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewP();

            pckWriteU64P(packWrite, blockSize);
            pckWriteU32P(packWrite, reference);
            pckWriteU64P(packWrite, bundleId);
            pckWriteU64P(packWrite, bundleOffset);
            pckWriteBinP(packWrite, blockMapPrior);
            pckWritePackP(packWrite, driver->compressParam);

            if (driver->compressParam != NULL)
                pckWriteStrIdP(packWrite, driver->compressType);

            pckWritePackP(packWrite, driver->encryptParam);

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
        const unsigned int reference = pckReadU32P(paramListPack);
        const uint64_t bundleId = (size_t)pckReadU64P(paramListPack);
        const uint64_t bundleOffset = (size_t)pckReadU64P(paramListPack);
        const Buffer *blockMapPrior = pckReadBinP(paramListPack);

        // Create compress filter
        const Pack *const compressParam = pckReadPackP(paramListPack);
        const IoFilter *compress = NULL;

        if (compressParam != NULL)
            compress = compressFilterPack(pckReadStrIdP(paramListPack), compressParam);

        // Create encrypt filter
        const Pack *const encryptParam = pckReadPackP(paramListPack);
        const IoFilter *encrypt = NULL;

        if (encryptParam != NULL)
            encrypt = cipherBlockNewPack(encryptParam);

        result = ioFilterMove(
            blockIncrNew(blockSize, reference, bundleId, bundleOffset, blockMapPrior, compress, encrypt), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
