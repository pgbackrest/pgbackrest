/***********************************************************************************************************************************
Block Incremental Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "command/backup/blockMap.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/xxhash.h"
#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockIncr
{
    unsigned int reference;                                         // Current backup reference
    uint64_t bundleId;                                              // Bundle id

    StringId compressType;                                          // Compress filter type
    const Pack *compressParam;                                      // Compress filter parameters
    const Pack *encryptParam;                                       // Encrypt filter parameters

    unsigned int blockNo;                                           // Block number
    uint64_t superBlockNo;                                          // Block no in super block
    uint64_t blockOffset;                                           // Block offset
    uint64_t superBlockSize;                                        // Super block
    size_t blockSize;                                               // Block size
    size_t checksumSize;                                            // Checksum size
    Buffer *block;                                                  // Block buffer

    Buffer *blockOut;                                               // Block output buffer
    IoWrite *blockOutWrite;                                         // Write to the block block buffer
    List *blockOutList;                                             // List of block map items that need an updated size
    size_t blockOutSize;                                            // Amount written to block output (excluding block no)
    size_t blockOutOffset;                                          // Block output offset (already copied to output buffer)

    const BlockMap *blockMapPrior;                                  // Prior block map
    BlockMap *blockMapOut;                                          // Output block map
    uint64_t blockMapOutSize;                                       // Output block map size (if any)
    bool blockMapWrite;                                             // Write block map (at least one new/changed block)

    size_t inputOffset;                                             // Input offset
    bool inputSame;                                                 // Input the same data
    bool done;                                                      // Is the filter done?
} BlockIncr;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
blockIncrToLog(const BlockIncr *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{superBlockSize, %" PRIu64 ", blockSize: %zu}", this->superBlockSize, this->blockSize);
}

#define FUNCTION_LOG_BLOCK_INCR_TYPE                                                                                               \
    BlockIncr *
#define FUNCTION_LOG_BLOCK_INCR_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(value, blockIncrToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Generate block incremental
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
        // If not done and not flushing out then get more block data
        if (!this->done && this->blockOutOffset == 0)
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

                bufCatSub(this->block, input, this->inputOffset, copySize);
                this->inputOffset += copySize;

                // The same input will be needed again to copy the rest
                this->inputSame = true;
            }
        }

        // If done with a partial block or block is full
        if ((this->done && bufUsed(this->block) > 0) || bufUsed(this->block) == this->blockSize)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Get block checksum
                const Buffer *const checksum = xxHashOne(this->checksumSize, this->block);

                // Does the block exist in the input map?
                const BlockMapItem *const blockMapItemIn =
                    this->blockMapPrior != NULL && this->blockNo < blockMapSize(this->blockMapPrior) ?
                        blockMapGet(this->blockMapPrior, this->blockNo) : NULL;

                // If the block is new or has changed then write it
                if (blockMapItemIn == NULL || memcmp(blockMapItemIn->checksum, bufPtrConst(checksum), this->checksumSize) != 0)
                {
                    // Begin the super block
                    if (this->blockOutWrite == NULL)
                    {
                        MEM_CONTEXT_OBJ_BEGIN(this)
                        {
                            this->blockOutWrite = ioBufferWriteNew(this->blockOut);
                            this->blockOutList = lstNewP(sizeof(unsigned int));
                        }
                        MEM_CONTEXT_OBJ_END();

                        // Add compress filter
                        if (this->compressParam != NULL)
                        {
                            ioFilterGroupAdd(
                                ioWriteFilterGroup(this->blockOutWrite),
                                compressFilterPack(this->compressType, this->compressParam));
                        }

                        // Add encrypt filter
                        if (this->encryptParam != NULL)
                            ioFilterGroupAdd(ioWriteFilterGroup(this->blockOutWrite), cipherBlockNewPack(this->encryptParam));

                        // Add size filter
                        ioFilterGroupAdd(ioWriteFilterGroup(this->blockOutWrite), ioSizeNew());
                        ioWriteOpen(this->blockOutWrite);
                    }

                    // Copy block data through the filters
                    ioCopyP(ioBufferReadNewOpen(this->block), this->blockOutWrite);
                    this->blockOutSize += bufUsed(this->block);
                    bufUsedZero(this->block);

                    // Write to block map
                    BlockMapItem blockMapItem =
                    {
                        .reference = this->reference,
                        .superBlockSize = this->superBlockSize,
                        .bundleId = this->bundleId,
                        .offset = this->blockOffset,
                        .block = this->superBlockNo,
                    };

                    memcpy(blockMapItem.checksum, bufPtrConst(checksum), bufUsed(checksum));

                    const unsigned int blockMapItemIdx = blockMapSize(this->blockMapOut);
                    blockMapAdd(this->blockMapOut, &blockMapItem);
                    lstAdd(this->blockOutList, &blockMapItemIdx);

                    // Increment super block no
                    this->superBlockNo++;
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

        // Write the super block
        if (this->blockOutWrite != NULL && (this->done || this->blockOutSize >= this->superBlockSize))
        {
            // Close write
            ioWriteClose(this->blockOutWrite);

            // Update size and super block size for items already added to the block map
            PackRead *const filter = ioFilterGroupResultP(ioWriteFilterGroup(this->blockOutWrite), SIZE_FILTER_TYPE);
            const uint64_t blockOutSize = pckReadU64P(filter);

            for (unsigned int blockMapIdx = 0; blockMapIdx < lstSize(this->blockOutList); blockMapIdx++)
            {
                BlockMapItem *const blockMapItem = blockMapGet(
                    this->blockMapOut, *(unsigned int *)lstGet(this->blockOutList, blockMapIdx));

                blockMapItem->size = blockOutSize;
                blockMapItem->superBlockSize = this->blockOutSize;
            }

            pckReadFree(filter);
            lstFree(this->blockOutList);

            // Set to NULL so the super block can be flushed
            ioWriteFree(this->blockOutWrite);
            this->blockOutWrite = NULL;

            // Increment block offset
            this->blockOffset += blockOutSize;

            // Reset block out size and super block no
            this->blockOutSize = 0;
            this->superBlockNo = 0;

            // Block map must be written since there are new/changed blocks
            this->blockMapWrite = true;
        }

        // Write the block map if done processing and there are new/changed blocks or block list has been truncated
        if (this->done && this->blockOutOffset == 0 &&
            (this->blockMapWrite ||
             (this->blockMapPrior != NULL && blockMapSize(this->blockMapOut) < blockMapSize(this->blockMapPrior))))
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
                blockMapWrite(this->blockMapOut, write, this->blockSize, this->checksumSize);
                ioWriteClose(write);

                // Get total bytes written for the map
                this->blockMapOutSize = bufUsed(this->blockOut) - blockOutBegin;
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Copy to output buffer if output has been completely written
        if (this->blockOutWrite == NULL)
        {
            const size_t blockOutSize = bufUsed(this->blockOut) - this->blockOutOffset;

            if (blockOutSize > 0)
            {
                // Output the rest of the block if it will fit
                if (bufRemains(output) >= blockOutSize)
                {
                    bufCatSub(output, this->blockOut, this->blockOutOffset, blockOutSize);
                    bufUsedZero(this->blockOut);

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
FN_EXTERN IoFilter *
blockIncrNew(
    const uint64_t superBlockSize, const size_t blockSize, const size_t checksumSize, const unsigned int reference,
    const uint64_t bundleId, const uint64_t bundleOffset, const Buffer *const blockMapPrior, const IoFilter *const compress,
    const IoFilter *const encrypt)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT64, superBlockSize);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(SIZE, checksumSize);
        FUNCTION_LOG_PARAM(UINT, reference);
        FUNCTION_LOG_PARAM(UINT64, bundleId);
        FUNCTION_LOG_PARAM(UINT64, bundleOffset);
        FUNCTION_LOG_PARAM(BUFFER, blockMapPrior);
        FUNCTION_LOG_PARAM(IO_FILTER, compress);
        FUNCTION_LOG_PARAM(IO_FILTER, encrypt);
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(BlockIncr, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (BlockIncr)
        {
            .superBlockSize = (superBlockSize / blockSize + (superBlockSize % blockSize == 0 ? 0 : 1)) * blockSize,
            .blockSize = blockSize,
            .checksumSize = checksumSize,
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
            this->compressType = ioFilterType(compress);
            this->compressParam = pckDup(ioFilterParamList(compress));
        }

        // Duplicate encrypt filter
        if (encrypt != NULL)
            this->encryptParam = pckDup(ioFilterParamList(encrypt));

        // Load prior block map
        if (blockMapPrior)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                IoRead *const read = ioBufferReadNewOpen(blockMapPrior);

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    this->blockMapPrior = blockMapNewRead(read, blockSize, checksumSize);
                }
                MEM_CONTEXT_PRIOR_END();
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU64P(packWrite, this->superBlockSize);
        pckWriteU64P(packWrite, blockSize);
        pckWriteU64P(packWrite, checksumSize);
        pckWriteU32P(packWrite, reference);
        pckWriteU64P(packWrite, bundleId);
        pckWriteU64P(packWrite, bundleOffset);
        pckWriteBinP(packWrite, blockMapPrior);
        pckWritePackP(packWrite, this->compressParam);

        if (this->compressParam != NULL)
            pckWriteStrIdP(packWrite, this->compressType);

        pckWritePackP(packWrite, this->encryptParam);

        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            BLOCK_INCR_FILTER_TYPE, this, paramList, .done = blockIncrDone, .inOut = blockIncrProcess,
            .inputSame = blockIncrInputSame, .result = blockIncrResult));
}

FN_EXTERN IoFilter *
blockIncrNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const uint64_t superBlockSize = pckReadU64P(paramListPack);
        const size_t blockSize = (size_t)pckReadU64P(paramListPack);
        const size_t checksumSize = (size_t)pckReadU64P(paramListPack);
        const unsigned int reference = pckReadU32P(paramListPack);
        const uint64_t bundleId = pckReadU64P(paramListPack);
        const uint64_t bundleOffset = pckReadU64P(paramListPack);
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
            blockIncrNew(
                superBlockSize, blockSize, checksumSize, reference, bundleId, bundleOffset, blockMapPrior, compress, encrypt),
            memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
