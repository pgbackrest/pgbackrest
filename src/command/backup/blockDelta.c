/***********************************************************************************************************************************
Block Delta

!!!
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h> // !!!

#include "command/backup/blockDelta.h"
#include "command/backup/blockIncr.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/chunkedRead.h"
#include "common/log.h"

// !!! MOVE TO LIST MODULE
static int
lstComparatorUInt(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(INT, (int)*(unsigned int *)item1 - (int)*(unsigned int *)item2);
}

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct BlockDelta
{
    BlockDeltaPub pub;                                              // Publicly accessible variables
    size_t blockSize;                                               // Block size
    CipherType cipherType;                                          // Cipher type
    String *cipherPass;                                             // Cipher passphrase
    CompressType compressType;                                      // Compress type

    unsigned int superBlockIdx;                                     // Super block index
    unsigned int blockIdx;                                          // Block index
    unsigned int blockNo;                                           // Block number
    IoRead *chunkedRead;                                            // Chunked read
    BlockDeltaWrite write;                                          // !!!
};

/**********************************************************************************************************************************/
typedef struct BlockDeltaReference
{
    unsigned int reference;
    List *blockList;
} BlockDeltaReference;

typedef struct BlockDeltaSuperBlock
{
    uint64_t size;                                                  // Size of super block
    List *blockList;                                                // Block list
} BlockDeltaSuperBlock;

typedef struct BlockDeltaBlock
{
    uint64_t no;                                                    // Block number in the super block
    uint64_t offset;                                                // Offset into original file
    unsigned char checksum[HASH_TYPE_SHA1_SIZE];                    // Checksum of the block
} BlockDeltaBlock;

FN_EXTERN BlockDelta *
blockDeltaNew(
    const BlockMap *const blockMap, const size_t blockSize, const Buffer *const deltaMap, const CipherType cipherType,
    const String *const cipherPass, const CompressType compressType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_TEST_PARAM(SIZE, blockSize);
        FUNCTION_TEST_PARAM(BUFFER, deltaMap);
        FUNCTION_TEST_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_TEST_PARAM(ENUM, compressType);
    FUNCTION_TEST_END();

    ASSERT(blockMap != NULL);
    ASSERT(blockSize > 0);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    BlockDelta *this = NULL;

    OBJ_NEW_BEGIN(BlockDelta, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (BlockDelta)
        {
            .pub =
            {
                .readList = lstNewP(sizeof(BlockDeltaRead)),
            },
            .blockSize = blockSize,
            .cipherType = cipherType,
            .cipherPass = strDup(cipherPass),
            .compressType = compressType,
        };

        // Construct the delta
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Build reference list (!!! will need to be filtered for restore)
            const unsigned int deltaMapSize =
                deltaMap == NULL ? 0 : (unsigned int)(bufUsed(deltaMap) / HASH_TYPE_SHA1_SIZE); // {uncovered - !!!}
            List *const referenceList = lstNewP(sizeof(BlockDeltaReference), .comparator = lstComparatorUInt);

            for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
            {
                const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                // The block must be updated if it beyond the blocks that exist in the delta map or when the checksum stored in the
                // repository is different from the delta map
                if (blockMapIdx >= deltaMapSize || // {uncovered - !!!}
                    !bufEq( // {uncovered - !!!}
                        BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE), // {uncovered - !!!}
                        BUF(bufPtrConst(deltaMap) + blockMapIdx * HASH_TYPE_SHA1_SIZE, HASH_TYPE_SHA1_SIZE))) // {uncovered - !!!}
                {
                    const unsigned int reference = blockMapItem->reference;
                    BlockDeltaReference *const referenceData = lstFind(referenceList, &reference);

                    if (referenceData == NULL)
                    {
                        BlockDeltaReference *referenceData = lstAdd(
                            referenceList,
                            &(BlockDeltaReference){.reference = reference, .blockList = lstNewP(sizeof(unsigned int))});
                        lstAdd(referenceData->blockList, &blockMapIdx);
                    }
                    else
                        lstAdd(referenceData->blockList, &blockMapIdx);
                }
            }

            lstSort(referenceList, sortOrderDesc);

            // Build delta
            for (unsigned int referenceIdx = 0; referenceIdx < lstSize(referenceList); referenceIdx++)
            {
                const BlockDeltaReference *const referenceData = (const BlockDeltaReference *)lstGet(referenceList, referenceIdx);
                BlockDeltaRead *blockDeltaRead = NULL;
                BlockDeltaSuperBlock *blockDeltaSuperBlock = NULL;
                const BlockMapItem *blockMapItemPrior = NULL;

                for (unsigned int blockIdx = 0; blockIdx < lstSize(referenceData->blockList); blockIdx++)
                {
                    const unsigned int blockMapIdx = *(unsigned int *)lstGet(referenceData->blockList, blockIdx);
                    const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                    // Add read when it has changed
                    if (blockMapItemPrior == NULL ||
                        (blockMapItemPrior->offset != blockMapItem->offset &&
                         blockMapItemPrior->offset + blockMapItemPrior->size != blockMapItem->offset))
                    {
                        MEM_CONTEXT_OBJ_BEGIN(this->pub.readList)
                        {
                            BlockDeltaRead blockDeltaReadNew =
                            {
                                .reference = blockMapItem->reference,
                                .bundleId = blockMapItem->bundleId,
                                .offset = blockMapItem->offset,
                                .superBlockList = lstNewP(sizeof(BlockDeltaSuperBlock)),
                            };

                            blockDeltaRead = lstAdd(this->pub.readList, &blockDeltaReadNew);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Add super block when it has changed
                    if (blockMapItemPrior == NULL || blockMapItemPrior->offset != blockMapItem->offset)
                    {
                        MEM_CONTEXT_OBJ_BEGIN(blockDeltaRead->superBlockList)
                        {
                            BlockDeltaSuperBlock blockDeltaSuperBlockNew =
                            {
                                .size = blockMapItem->size,
                                .blockList = lstNewP(sizeof(BlockDeltaBlock)),
                            };

                            blockDeltaSuperBlock = lstAdd(blockDeltaRead->superBlockList, &blockDeltaSuperBlockNew);
                            blockDeltaRead->size += blockMapItem->size;
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Add block
                    BlockDeltaBlock blockDeltaBlockNew =
                    {
                        .no = blockMapItem->block,
                        .offset = blockMapIdx * blockSize,
                    };

                    memcpy(blockDeltaBlockNew.checksum, blockMapItem->checksum, SIZE_OF_STRUCT_MEMBER(BlockDeltaBlock, checksum));
                    lstAdd(blockDeltaSuperBlock->blockList, &blockDeltaBlockNew);

                    // Set prior item for comparison on the next loop
                    blockMapItemPrior = blockMapItem;
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(BLOCK_DELTA, this);
}

const BlockDeltaWrite *
blockDeltaNext(BlockDelta *const this, const BlockDeltaRead *const readDelta, IoRead *const readIo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_DELTA, this);
        FUNCTION_TEST_PARAM_P(VOID, readDelta);
        FUNCTION_TEST_PARAM(IO_READ, readIo);
    FUNCTION_TEST_END();

    ASSERT(readDelta != NULL);
    ASSERT(readIo != NULL);

    BlockDeltaWrite *result = NULL;

    while (this->superBlockIdx < lstSize(readDelta->superBlockList))
    {
        if (this->write.block == NULL)
        {
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->write.block = bufNew(this->blockSize);
            }
            MEM_CONTEXT_OBJ_END();
        }
        else
            bufUsedSet(this->write.block, 0);

        if (this->chunkedRead == NULL)
        {
            // fprintf(
            //     stdout, "!!!SUPER BLOCK %u BLOCK TOTAL %u\n", this->superBlockIdx,
            //     lstSize(readDelta->superBlockList));fflush(stdout);

            this->blockIdx = 0;
            this->blockNo = 0;

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->chunkedRead = ioChunkedReadNew(readIo);
            }
            MEM_CONTEXT_OBJ_END();

            if (this->cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(this->chunkedRead),
                    cipherBlockNewP(cipherModeDecrypt, this->cipherType, BUFSTR(this->cipherPass), .raw = true));
            }

            if (this->compressType != compressTypeNone)
                ioFilterGroupAdd(ioReadFilterGroup(this->chunkedRead), decompressFilter(this->compressType));

            ioReadOpen(this->chunkedRead);
        }

        const BlockDeltaSuperBlock *const superBlockData = lstGet(readDelta->superBlockList, this->superBlockIdx);
        uint64_t blockEncoded = ioReadVarIntU64(this->chunkedRead);

        if (this->blockNo != 0 && blockEncoded == 0)
        {
            ioReadFree(this->chunkedRead);
            this->chunkedRead = NULL;
            this->superBlockIdx++;
            continue;
        }

        const BlockDeltaBlock *const blockData = lstGet(superBlockData->blockList, this->blockIdx);

        do
        {
            // fprintf(stdout, "!!!  BLOCK NO %u IDX %u\n", this->blockNo, this->blockIdx); fflush(stdout);

            if (blockEncoded & BLOCK_INCR_FLAG_SIZE)
                bufLimitSet(this->write.block, (size_t)ioReadVarIntU64(this->chunkedRead));

            ioRead(this->chunkedRead, this->write.block);

            if (this->blockNo == blockData->no) // {uncovered - !!! NEEDS BLOCK FILTER}
            {
                this->write.offset = blockData->offset;
                result = &this->write;

                this->blockIdx++;
            }

            if (blockEncoded & BLOCK_INCR_FLAG_SIZE)
            {
                ioReadFree(this->chunkedRead);
                this->chunkedRead = NULL;
                this->superBlockIdx++;

                if (result == NULL) // {uncovered - !!! NEEDS BLOCK FILTER}
                    continue; // {uncovered - !!! NEEDS BLOCK FILTER}
            }

            this->blockNo++;

            if (result != NULL) // {uncovered - !!! NEEDS BLOCK FILTER}
                break;
        }
        while (true);

        if (result != NULL) // {uncovered - !!! NEEDS BLOCK FILTER}
            break;
    }

    if (result == NULL)
    {
        // fprintf(stdout, "!!!  READ COMPLETE\n"); fflush(stdout);
        this->superBlockIdx = 0;
    }

    FUNCTION_TEST_RETURN_TYPE_P(BlockDeltaWrite, result);
}
