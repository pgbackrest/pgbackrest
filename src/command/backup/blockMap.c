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

    FUNCTION_TEST_RETURN(INT, 0);
}

uint64_t
blockMapSave(const BlockMap *const this, Buffer *const output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, this);
        FUNCTION_TEST_PARAM(BUFFER, output);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(blockMapSize(this) > 0);
    ASSERT(output != NULL);

    const size_t outputBegin = bufUsed(output);
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);

    // Resize the buffer to add enough space for the maximum size of the map. This will prevent resizes during the loop.
    #define BLOCK_MAP_SIZE_MAX                                                                                                     \
        (blockMapSize(this) * (CVT_VARINT128_BUFFER_SIZE * 3 + HASH_TYPE_SHA1_SIZE))

    if (bufRemains(output) < BLOCK_MAP_SIZE_MAX)
        bufResize(output, bufUsed(output) + BLOCK_MAP_SIZE_MAX);

    // !!!
    unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];

    for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(this); blockMapIdx++)
    {
        const BlockMapItem *const blockMapItem = blockMapGet(this, blockMapIdx);

        // Add reference
        size_t bufferPos = 0;
        cvtUInt64ToVarInt128(blockMapItem->reference, buffer, &bufferPos, sizeof(buffer));
        bufCatC(output, buffer, 0, bufferPos);

        // !!! WRITE REFERENCE, BUNDLE ID, CHECKSUM

        // Check if the reference is already in the list
        BlockMapRef *const blockMapRef = lstFind(
            refList, &(BlockMapRef){.reference = blockMapItem->reference, .bundleId = blockMapItem->bundleId});

        if (blockMapRef == NULL)
        {
            // Add bundle id
            bufferPos = 0;
            cvtUInt64ToVarInt128(blockMapItem->bundleId, buffer, &bufferPos, sizeof(buffer));
            bufCatC(output, buffer, 0, bufferPos);

            // Add offset
            bufferPos = 0;
            cvtUInt64ToVarInt128(blockMapItem->offset, buffer, &bufferPos, sizeof(buffer));
            bufCatC(output, buffer, 0, bufferPos);

            // Add reference to list
            lstAdd(
                refList,
                &(BlockMapRef){
                    .reference = blockMapItem->reference, .bundleId = blockMapItem->bundleId, .offset = blockMapItem->offset});
        }
        else
        {
            ASSERT(blockMapItem->reference == blockMapRef->reference);
            ASSERT(blockMapItem->bundleId == blockMapRef->bundleId);
            ASSERT(blockMapItem->offset > blockMapRef->offset);

            // Add rolling offset delta
            bufferPos = 0;
            cvtUInt64ToVarInt128(blockMapItem->offset - blockMapRef->offset, buffer, &bufferPos, sizeof(buffer));
            bufCatC(output, buffer, 0, bufferPos);

            // Update the offset
            blockMapRef->offset = blockMapItem->offset;
        }

        // Add checksum
        bufCatC(output, blockMapItem->checksum, 0, HASH_TYPE_SHA1_SIZE);
    }

    FUNCTION_TEST_RETURN(UINT64, bufUsed(output) - outputBegin);
}
