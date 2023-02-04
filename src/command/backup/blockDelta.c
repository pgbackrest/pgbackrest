/***********************************************************************************************************************************
Block Delta

!!!
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockDelta.h"
#include "common/debug.h"
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

/**********************************************************************************************************************************/
typedef struct BlockDeltaReference
{
    unsigned int reference;
    List *blockList;
} BlockDeltaReference;

FN_EXTERN BlockDelta *
blockDeltaNew(const BlockMap *const blockMap, const uint64_t blockSize, const Buffer *const deltaMap)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_TEST_PARAM(UINT64, blockSize);
        FUNCTION_TEST_PARAM(BUFFER, deltaMap);
    FUNCTION_TEST_END();

    BlockDelta *const this = (BlockDelta *)OBJ_NAME(lstNewP(sizeof(BlockDeltaRead)), BlockDelta::List);

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
                        referenceList, &(BlockDeltaReference){.reference = reference, .blockList = lstNewP(sizeof(unsigned int))});
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
                    (blockMapItemPrior->offset != blockMapItem->offset && // {uncovered - !!!}
                     blockMapItemPrior->offset + blockMapItemPrior->size != blockMapItem->offset)) // {uncovered - !!!}
                {
                    MEM_CONTEXT_OBJ_BEGIN((List *)this)
                    {
                        BlockDeltaRead blockDeltaReadNew =
                        {
                            .reference = blockMapItem->reference,
                            .bundleId = blockMapItem->bundleId,
                            .offset = blockMapItem->offset,
                            .superBlockList = lstNewP(sizeof(BlockDeltaSuperBlock)),
                        };

                        blockDeltaRead = lstAdd((List *)this, &blockDeltaReadNew);
                    }
                    MEM_CONTEXT_OBJ_END();
                }

                // Add super block when it has changed
                if (blockMapItemPrior == NULL || blockMapItemPrior->offset != blockMapItem->offset) // {uncovered - !!!}
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

    FUNCTION_TEST_RETURN(BLOCK_DELTA, this);
}
