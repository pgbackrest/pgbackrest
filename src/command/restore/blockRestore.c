/***********************************************************************************************************************************
Block Restore
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockIncr.h"
#include "command/restore/blockRestore.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/chunkedRead.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockRestoreSuperBlock
{
    uint64_t size;                                                  // Size of super block
    List *blockList;                                                // Block list
} BlockRestoreSuperBlock;

typedef struct BlockRestoreBlock
{
    uint64_t no;                                                    // Block number in the super block
    uint64_t offset;                                                // Offset into original file
    unsigned char checksum[HASH_TYPE_SHA1_SIZE];                    // Checksum of the block
} BlockRestoreBlock;

struct BlockRestore
{
    BlockRestorePub pub;                                            // Publicly accessible variables
    size_t blockSize;                                               // Block size
    CipherType cipherType;                                          // Cipher type
    String *cipherPass;                                             // Cipher passphrase
    CompressType compressType;                                      // Compress type

    const BlockRestoreSuperBlock *superBlockData;                   // Current super block data
    unsigned int superBlockIdx;                                     // Current super block index
    unsigned int blockNo;                                           // Block number in current super block
    IoRead *chunkedRead;                                            // Chunked read for current super block
    const BlockRestoreBlock *blockData;                             // Current block data
    unsigned int blockIdx;                                          // Current block index

    BlockRestoreWrite write;                                        // Block/offset to be returned
};

/**********************************************************************************************************************************/
typedef struct BlockRestoreReference
{
    unsigned int reference;                                         // Reference
    List *blockList;                                                // List of blocks in the block map for the reference
} BlockRestoreReference;

FN_EXTERN BlockRestore *
blockRestoreNew(
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

    BlockRestore *this = NULL;

    OBJ_NEW_BEGIN(BlockRestore, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (BlockRestore)
        {
            .pub =
            {
                .readList = lstNewP(sizeof(BlockRestoreRead)),
            },
            .blockSize = blockSize,
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
            const unsigned int deltaMapSize =
                deltaMap == NULL ? 0 : (unsigned int)(bufUsed(deltaMap) / HASH_TYPE_SHA1_SIZE);
            List *const referenceList = lstNewP(sizeof(BlockRestoreReference), .comparator = lstComparatorUInt);

            for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
            {
                const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                // The block must be updated if it is beyond the blocks that exist in the delta map or when the checksum stored in
                // the repository is different from the delta map
                if (blockMapIdx >= deltaMapSize ||
                    !bufEq(
                        BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE),
                        BUF(bufPtrConst(deltaMap) + blockMapIdx * HASH_TYPE_SHA1_SIZE, HASH_TYPE_SHA1_SIZE)))
                {
                    const unsigned int reference = blockMapItem->reference;
                    BlockRestoreReference *const referenceData = lstFind(referenceList, &reference);

                    // If the reference has not been added
                    if (referenceData == NULL)
                    {
                        BlockRestoreReference *referenceData = lstAdd(
                            referenceList,
                            &(BlockRestoreReference){.reference = reference, .blockList = lstNewP(sizeof(unsigned int))});
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
                const BlockRestoreReference *const referenceData =
                    (const BlockRestoreReference *)lstGet(referenceList, referenceIdx);
                BlockRestoreRead *blockRestoreRead = NULL;
                BlockRestoreSuperBlock *blockRestoreSuperBlock = NULL;
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
                            BlockRestoreRead blockRestoreReadNew =
                            {
                                .reference = blockMapItem->reference,
                                .bundleId = blockMapItem->bundleId,
                                .offset = blockMapItem->offset,
                                .superBlockList = lstNewP(sizeof(BlockRestoreSuperBlock)),
                            };

                            blockRestoreRead = lstAdd(this->pub.readList, &blockRestoreReadNew);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Add super block when it has changed
                    if (blockMapItemPrior == NULL || blockMapItemPrior->offset != blockMapItem->offset)
                    {
                        MEM_CONTEXT_OBJ_BEGIN(blockRestoreRead->superBlockList)
                        {
                            BlockRestoreSuperBlock blockRestoreSuperBlockNew =
                            {
                                .size = blockMapItem->size,
                                .blockList = lstNewP(sizeof(BlockRestoreBlock)),
                            };

                            blockRestoreSuperBlock = lstAdd(blockRestoreRead->superBlockList, &blockRestoreSuperBlockNew);
                            blockRestoreRead->size += blockMapItem->size;
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Add block
                    BlockRestoreBlock blockRestoreBlockNew =
                    {
                        .no = blockMapItem->block,
                        .offset = blockMapIdx * blockSize,
                    };

                    memcpy(
                        blockRestoreBlockNew.checksum, blockMapItem->checksum, SIZE_OF_STRUCT_MEMBER(BlockRestoreBlock, checksum));
                    lstAdd(blockRestoreSuperBlock->blockList, &blockRestoreBlockNew);

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
FN_EXTERN const BlockRestoreWrite *
blockRestoreNext(BlockRestore *const this, const BlockRestoreRead *const readDelta, IoRead *const readIo)
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

    BlockRestoreWrite *result = NULL;

    // Iterate super blocks
    while (this->superBlockIdx < lstSize(readDelta->superBlockList))
    {
        // If the super block read has not begun yet
        if (this->superBlockData == NULL)
        {
            // Free prior chunked read and create chunked read for current super block
            ioReadFree(this->chunkedRead);
            this->superBlockData = lstGet(readDelta->superBlockList, this->superBlockIdx);

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

            // Set block info
            this->blockIdx = 0;
            this->blockNo = 0;
            this->blockData = lstGet(this->superBlockData->blockList, this->blockIdx);
        }

        // Read encoded info about the block
        uint64_t blockEncoded = ioReadVarIntU64(this->chunkedRead);

        // Loop through blocks in the super block until a match with the map is found
        do
        {
            // Break out if encoded block info is zero and this is not the first block. This means the super block has ended when
            // all blocks are equal size.
            if (this->blockNo != 0 && blockEncoded == 0)
            {
                this->superBlockData = NULL;
                this->superBlockIdx++;
                break;
            }

            // Apply block size limit if required and read the block
            bufUsedSet(this->write.block, 0);

            if (blockEncoded & BLOCK_INCR_FLAG_SIZE)
            {
                size_t size = (size_t)ioReadVarIntU64(this->chunkedRead);
                bufLimitSet(this->write.block, size);
            }
            else
                bufLimitClear(this->write.block);

            ioRead(this->chunkedRead, this->write.block);

            // If the block matches the block we are expecting
            if (this->blockNo == this->blockData->no)
            {
                this->write.offset = this->blockData->offset;
                result = &this->write;
                this->blockIdx++;

                // Get the next block if there are any more to read
                if (this->blockIdx < lstSize(this->superBlockData->blockList))
                {
                    this->blockData = lstGet(this->superBlockData->blockList, this->blockIdx);
                }
                // Else stop processing if this is the last super block. This is only works for the last super block because
                // otherwise the blocks must be read sequentially.
                else if (this->superBlockIdx >= lstSize(readDelta->superBlockList) - 1)
                {
                    this->superBlockData = NULL;
                    this->superBlockIdx++;
                }
            }

            // If the size was set (meaning this is the last block in the super block) it must also be the last block we are looking
            // for in the read. Partial blocks cannot happen in the middle of a read and there is code above to cut out early on the
            // last super block of a read when possible.
            ASSERT(!(blockEncoded & BLOCK_INCR_FLAG_SIZE) || this->superBlockIdx >= lstSize(readDelta->superBlockList) - 1);

            // Increment the block to read in the super block
            this->blockNo++;

            // Break if there is a result
            if (result != NULL)
                break;

            // Read encoded info about the block
            blockEncoded = ioReadVarIntU64(this->chunkedRead);
        }
        while (true);

        // Break if there is a result
        if (result != NULL)
            break;
    }

    // If no result then the super blocks have been read. Reset for the next read.
    if (result == NULL)
    {
        this->superBlockData = NULL;
        this->superBlockIdx = 0;
    }

    FUNCTION_TEST_RETURN_TYPE_P(BlockRestoreWrite, result);
}
