/***********************************************************************************************************************************
Block Incremental Map
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "command/backup/blockMap.h"

/**********************************************************************************************************************************/
typedef struct BlockMapRef
{
    unsigned int reference;
    uint64_t bundleId;
    uint64_t offset;
} BlockMapRef;

static int
lstComparatorBlockMapRef(const void *const blockMapRef1, const void *const blockMapRef2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, blockMapRef1);
        FUNCTION_TEST_PARAM_P(VOID, blockMapRef2);
    FUNCTION_TEST_END();

    ASSERT(blockMapRef1 != NULL);
    ASSERT(blockMapRef2 != NULL);

    if (((BlockMapRef *)blockMapRef1)->reference < ((BlockMapRef *)blockMapRef2)->reference)
        FUNCTION_TEST_RETURN(INT, -1);
    else if (((BlockMapRef *)blockMapRef1)->reference > ((BlockMapRef *)blockMapRef2)->reference)
        FUNCTION_TEST_RETURN(INT, 1);

    if (((BlockMapRef *)blockMapRef1)->bundleId < ((BlockMapRef *)blockMapRef2)->bundleId)
        FUNCTION_TEST_RETURN(INT, -1);
    else if (((BlockMapRef *)blockMapRef1)->bundleId > ((BlockMapRef *)blockMapRef2)->bundleId)
        FUNCTION_TEST_RETURN(INT, 1);

    FUNCTION_TEST_RETURN(INT, 0);
}

uint64_t
blockMapSave(const BlockMap *const this, Buffer *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, this);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(blockMapSize(this) > 0);
    ASSERT(buffer != NULL);

    uint64_t usedBegin = bufUsed(buffer);
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);

    // Resize the buffer to add enough space for the maximum size of the map. This will prevent resizes during the loop.
    #define BLOCK_MAP_SIZE_MAX                                                                                                     \
        (blockMapSize(this) * (CVT_VARINT128_BUFFER_SIZE * 3 + HASH_TYPE_SHA1_SIZE))

    if (bufRemains(buffer) < BLOCK_MAP_SIZE_MAX)
        bufResize(buffer, bufUsed(buffer) + BLOCK_MAP_SIZE_MAX);

    for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(this); blockMapIdx++)
    {
        const BlockMapItem *const blockMapItem = blockMapGet(this, blockMapIdx);

        // !!! WRITE REFERENCE, BUNDLE ID, CHECKSUM

        // Check if the reference is already in the list
        const BlockMapRef *blockMapRef = lstFind(
            refList, &(BlockMapRef){.reference = blockMapItem->reference, .bundleId = blockMapItem->bundleId});

        if (blockMapRef == NULL)
        {
            // !!! WRITE FULL OFFSET

            blockMapRef = lstAdd(
                refList,
                &(BlockMapRef){
                    .reference = blockMapItem->reference, .bundleId = blockMapItem->bundleId, .offset = blockMapItem->offset});
        }
        else
        {
            // WRITE DELTA OFFSET
            // !!! MAYBE MAKE THE OFFSET ROLLING RATHER THAN FROM THE FILE OFFSET? THAT WOULD JUST MEAN UPDATING THE REF EACH TIME
        }
    }

    FUNCTION_TEST_RETURN(UINT64, bufUsed(buffer) - usedBegin);
}
