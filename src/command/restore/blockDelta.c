/***********************************************************************************************************************************
Block Restore
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "command/restore/blockDelta.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/limitRead.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockDeltaSuperBlock
{
    uint64_t superBlockSize;                                        // Super block size
    uint64_t size;                                                  // Stored size of superblock (with compression, etc.)
    List *blockList;                                                // Block list
} BlockDeltaSuperBlock;

typedef struct BlockDeltaBlock
{
    uint64_t no;                                                    // Block number in the super block
    uint64_t offset;                                                // Offset into original file
    unsigned char checksum[XX_HASH_SIZE_MAX];                       // Checksum of the block
} BlockDeltaBlock;

struct BlockDelta
{
    BlockDeltaPub pub;                                              // Publicly accessible variables
    size_t blockSize;                                               // Block size
    size_t checksumSize;                                            // Checksum size
    CipherType cipherType;                                          // Cipher type
    String *cipherPass;                                             // Cipher passphrase
    CompressType compressType;                                      // Compress type

    const BlockDeltaSuperBlock *superBlockData;                     // Current super block data
    unsigned int superBlockIdx;                                     // Current super block index
    IoRead *limitRead;                                              // Limit read for current super block
    const BlockDeltaBlock *blockData;                               // Current block data
    unsigned int blockIdx;                                          // Current block index
    unsigned int blockTotal;                                        // Block total for super block
    unsigned int blockFindIdx;                                      // Index of the block to find in the super block

    BlockDeltaWrite write;                                          // Block/offset to be returned for write
};

/**********************************************************************************************************************************/
typedef struct BlockDeltaReference
{
    unsigned int reference;                                         // Reference
    List *blockList;                                                // List of blocks in the block map for the reference
} BlockDeltaReference;

FN_EXTERN BlockDelta *
blockDeltaNew(
    const BlockMap *const blockMap, const size_t blockSize, const size_t checksumSize, const Buffer *const blockChecksum,
    const CipherType cipherType, const String *const cipherPass, const CompressType compressType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_TEST_PARAM(SIZE, blockSize);
        FUNCTION_TEST_PARAM(SIZE, checksumSize);
        FUNCTION_TEST_PARAM(BUFFER, blockChecksum);
        FUNCTION_TEST_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_TEST_PARAM(ENUM, compressType);
    FUNCTION_TEST_END();

    ASSERT(blockMap != NULL);
    ASSERT(blockSize > 0);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    OBJ_NEW_BEGIN(BlockDelta, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (BlockDelta)
        {
            .pub =
            {
                .readList = lstNewP(sizeof(BlockDeltaRead)),
            },
            .blockSize = blockSize,
            .checksumSize = checksumSize,
            .cipherType = cipherType,
            .cipherPass = strDup(cipherPass),
            .compressType = compressType,
            .write =
            {
                .block = bufNew(blockSize),
            }
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Build list of references and for each reference the list of blocks for that reference
            const unsigned int blockChecksumSize =
                blockChecksum == NULL ? 0 : (unsigned int)(bufUsed(blockChecksum) / this->checksumSize);
            List *const referenceList = lstNewP(sizeof(BlockDeltaReference), .comparator = lstComparatorUInt);

            for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
            {
                const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                // The block must be updated if it is beyond the blocks that exist in the block checksum list or when the checksum
                // stored in the repository is different from the block checksum list
                if (blockMapIdx >= blockChecksumSize ||
                    !bufEq(
                        BUF(blockMapItem->checksum, this->checksumSize),
                        BUF(bufPtrConst(blockChecksum) + blockMapIdx * this->checksumSize, this->checksumSize)))
                {
                    const unsigned int reference = blockMapItem->reference;
                    BlockDeltaReference *const referenceData = lstFind(referenceList, &reference);

                    // If the reference has not been added
                    if (referenceData == NULL)
                    {
                        const BlockDeltaReference *const referenceData = lstAdd(
                            referenceList,
                            &(BlockDeltaReference){.reference = reference, .blockList = lstNewP(sizeof(unsigned int))});
                        lstAdd(referenceData->blockList, &blockMapIdx);
                    }
                    // Else add the new block
                    else
                        lstAdd(referenceData->blockList, &blockMapIdx);
                }
            }

            // Sort the reference list descending. This is an arbitrary choice as the order does not matter.
            lstSort(referenceList, sortOrderDesc);

            // Build delta
            for (unsigned int referenceIdx = 0; referenceIdx < lstSize(referenceList); referenceIdx++)
            {
                const BlockDeltaReference *const referenceData = (const BlockDeltaReference *)lstGet(referenceList, referenceIdx);
                BlockDeltaRead *blockDeltaRead = NULL;
                const BlockDeltaSuperBlock *blockDeltaSuperBlock = NULL;
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
                            const BlockDeltaRead blockDeltaReadNew =
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
                            const BlockDeltaSuperBlock blockDeltaSuperBlockNew =
                            {
                                .superBlockSize = blockMapItem->superBlockSize,
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

/**********************************************************************************************************************************/
FN_EXTERN const BlockDeltaWrite *
blockDeltaNext(BlockDelta *const this, const BlockDeltaRead *const readDelta, IoRead *const readIo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_DELTA, this);
        FUNCTION_TEST_PARAM_P(VOID, readDelta);
        FUNCTION_TEST_PARAM(IO_READ, readIo);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);
    ASSERT(readDelta != NULL);
    ASSERT(readIo != NULL);

    BlockDeltaWrite *result = NULL;

    // Iterate super blocks
    while (this->superBlockIdx < lstSize(readDelta->superBlockList))
    {
        // If the super block read has not begun yet
        if (this->superBlockData == NULL)
        {
            // Free prior limit read and create limit read for current super block
            ioReadFree(this->limitRead);
            this->superBlockData = lstGet(readDelta->superBlockList, this->superBlockIdx);

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->limitRead = ioLimitReadNew(readIo, this->superBlockData->size);
            }
            MEM_CONTEXT_OBJ_END();

            if (this->cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(this->limitRead),
                    cipherBlockNewP(cipherModeDecrypt, this->cipherType, BUFSTR(this->cipherPass), .raw = true));
            }

            if (this->compressType != compressTypeNone)
                ioFilterGroupAdd(ioReadFilterGroup(this->limitRead), decompressFilterP(this->compressType, .raw = true));

            ioReadOpen(this->limitRead);

            // Set block info
            this->blockIdx = 0;
            this->blockFindIdx = 0;
            this->blockTotal =
                (unsigned int)(this->superBlockData->superBlockSize / this->blockSize) +
                (this->superBlockData->superBlockSize % this->blockSize == 0 ? 0 : 1);
            this->blockData = lstGet(this->superBlockData->blockList, this->blockFindIdx);
        }

        // Find required blocks in the super block
        while (this->blockIdx < this->blockTotal)
        {
            // Clear buffer and read block
            bufUsedZero(this->write.block);
            bufLimitClear(this->write.block);

            ioRead(this->limitRead, this->write.block);

            // If the block matches the block we are expecting
            if (this->blockIdx == this->blockData->no)
            {
                ASSERT(result == NULL);

                this->write.offset = this->blockData->offset;
                result = &this->write;
                this->blockFindIdx++;

                // Get the next block if there are any more to read
                if (this->blockFindIdx < lstSize(this->superBlockData->blockList))
                    this->blockData = lstGet(this->superBlockData->blockList, this->blockFindIdx);
            }

            // Increment the block to read in the super block
            this->blockIdx++;

            // Break if there is a result
            if (result != NULL)
                break;
        }

        // Break if there is a result
        if (result != NULL)
            break;

        // Check that no bytes remain to be written. It is possible that some bytes remain in the super block, however, since we may
        // have gotten all the bytes we needed but just missed reading something important, e.g. an end of file marker. If we do not
        // read the remaining bytes then the next read will start too early.
        ioReadFlushP(this->limitRead, .errorOnBytes = true);

        this->superBlockData = NULL;
        this->superBlockIdx++;
    }

    // If no result then the super blocks have been read. Reset for the next read.
    if (result == NULL)
    {
        ASSERT(this->superBlockIdx == lstSize(readDelta->superBlockList));
        ASSERT(this->blockIdx == this->blockTotal);

        this->superBlockData = NULL;
        this->superBlockIdx = 0;
    }

    FUNCTION_TEST_RETURN_TYPE_P(BlockDeltaWrite, result);
}
