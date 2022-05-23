/***********************************************************************************************************************************
Block Incremental Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "command/backup/blockMap.h"
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

    unsigned int reference;                                         // Current backup reference
    uint64_t bundleId;                                              // Bundle id

    size_t inputOffset;                                             // Input offset

    unsigned int blockNo;                                           // Block number
    size_t blockOffset;                                             // Block offset
    size_t blockSize;                                               // Block size
    Buffer *block;                                                  // Block buffer

    Buffer *blockOut;                                               // Block output buffer
    size_t blockOutOffset;                                          // Block output offset (already copied to output buffer)

    const BlockMap *blockMapIn;                                     // Input block map
    BlockMap *blockMapOut;                                          // Output block map
    uint64_t blockMapOutSize;                                       // Output block map size (if any)

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

/*
Blocks are compressed and stored separately so they can be accessed randomly during restore.
A map follows the stored blocks so future backups can work directly from the map.

!!! DATA NEEDED

For the stored blocks:
    - block no (varint-128)
    - sha1 (20 bytes)
    - block data size (varint-128)
    - block data (compressed if requested)

For map:
    - zero-based number that references a list of backup labels (varint-128)
    - bundle id (if exists and if different than current) (varint-128)
    - offset to block data (might be simpler if we could store the offset to the file somewhere as a base)
        - maybe just store the file offset the first time a reference/bundle-id is found?
    - sha1 (20 bytes)

If a file that could shrink/grow is > block size but shrinks before getting copied, probably best to store it normally.

For manifest:
    - Global flag to indicate block incr
    - Size of map to separate it from the block data (but map size will be included in repo-size)
        - This will indicate that a file was stored with block level incr.
*/

    // If still accumulating data in the buffer
    if (!this->done && bufUsed(this->block) < this->blockSize)
    {
        // If all input can be copied
        if (bufUsed(input) - this->inputOffset <= bufRemains(this->block))
        {
            // fprintf(stdout, "!!!WRITE ALL %zu", bufUsed(input) - this->inputOffset);
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
            // If the file is smaller than a single block there is no need to store as a block or create a map
            const bool map = this->blockNo > 0 || bufUsed(this->block) == this->blockSize;

            if (bufUsed(this->block) > 0)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    IoWrite *const write = ioBufferWriteNew(this->blockOut);
                    // !!! ENCRYPT FILTER WOULD GO HERE
                    ioWriteOpen(write);

                    // Write block number and hash
                    BlockMapItem blockMapItem =
                    {
                        .reference = this->reference,
                        .bundleId = this->bundleId,
                        .offset = this->blockOffset,
                    };

                    if (map)
                    {
                        // Write block number
                        unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
                        size_t bufferPos = 0;

                        cvtUInt64ToVarInt128(this->blockNo, buffer, &bufferPos, sizeof(buffer));
                        ioWrite(write, BUF(buffer, bufferPos));

                        // Write checksum
                        const Buffer *const checksum = cryptoHashOne(HASH_TYPE_SHA1_STR, this->block);

                        ioWrite(write, checksum);
                        memcpy(blockMapItem.checksum, bufPtrConst(checksum), bufUsed(checksum));
                    }

                    // Write compressed size (if there is a map) and data
                    Buffer *const compressed = bufNew(0);
                    IoWrite *const compressedWrite = ioBufferWriteNew(compressed);

                    // ioFilterGroupAdd(ioWriteFilterGroup(write), compressFilter(/* !!! */compressTypeGz, 1));
                    ioWriteOpen(compressedWrite);
                    ioWrite(compressedWrite, this->block);
                    ioWriteClose(compressedWrite);

                    if (map)
                    {
                        unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
                        size_t bufferPos = 0;

                        cvtUInt64ToVarInt128(bufUsed(compressed), buffer, &bufferPos, sizeof(buffer));
                        ioWrite(write, BUF(buffer, bufferPos));

                        blockMapItem.size = bufUsed(compressed);
                        blockMapAdd(this->blockMapOut, &blockMapItem);

                        this->blockNo++;
                        this->blockOffset += bufUsed(compressed);
                    }

                    ioWrite(write, compressed);
                    ioWriteClose(write);
                }
                MEM_CONTEXT_TEMP_END();
            }

            if (this->done && map)
            {
                // Size of block output before starting to write the map
                const size_t blockOutBegin = bufUsed(this->blockOut);

                IoWrite *const write = ioBufferWriteNew(this->blockOut);
                // !!! ENCRYPT FILTER WOULD GO HERE
                ioWriteOpen(write);

                blockMapWrite(this->blockMapOut, write);

                // Close the write and get total bytes written for the map
                ioWriteClose(write);
                this->blockMapOutSize = bufUsed(this->blockOut) - blockOutBegin;
            }
        }

        // Copy to output buffer
        const size_t blockOutSize = bufUsed(this->blockOut) - this->blockOutOffset;

        if (blockOutSize > 0)
        {
            if (bufRemains(output) >= blockOutSize)
            {
                bufCatSub(output, this->blockOut, this->blockOutOffset, blockOutSize);
                bufUsedZero(this->blockOut);
                bufUsedZero(this->block);

                this->blockOutOffset = 0;
                this->inputSame = this->inputOffset != 0;
            }
            else
            {
                const size_t blockOutSize = bufRemains(output);
                bufCatSub(output, this->blockOut, this->blockOutOffset, blockOutSize);

                this->blockOutOffset += blockOutSize;
                this->inputSame = true;
            }
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
blockIncrNew(const size_t blockSize, const unsigned int reference, const uint64_t bundleId, const uint64_t bundleOffset)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(UINT, reference);
        FUNCTION_LOG_PARAM(UINT64, bundleId);
        FUNCTION_LOG_PARAM(UINT64, bundleOffset);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(BlockIncr, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        BlockIncr *driver = OBJ_NEW_ALLOC();

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

        // Create param list
        Pack *paramList = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewP();

            pckWriteU64P(packWrite, blockSize);
            pckWriteU32P(packWrite, reference);
            pckWriteU64P(packWrite, bundleId);
            pckWriteU64P(packWrite, bundleOffset);
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

        result = ioFilterMove(blockIncrNew(blockSize, reference, bundleId, bundleOffset), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
