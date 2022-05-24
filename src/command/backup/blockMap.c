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

BlockMap *
blockMapNewRead(IoRead *const map)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, map);
    FUNCTION_TEST_END();

    BlockMap *const this = blockMapNew();
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);
    Buffer *const checksum = bufNew(HASH_TYPE_SHA1_SIZE);
    BlockMapRef *blockMapRef = NULL;

    do
    {
        if (blockMapRef == NULL)
        {
            unsigned int reference = (unsigned int)ioReadVarIntU64(map);

            if (reference == 0)
                break;

            reference--;

            blockMapRef = lstFind(refList, &(BlockMapRef){.reference = reference});

            if (blockMapRef == NULL)
            {
                BlockMapRef blockMapRefAdd = {.reference = reference, .bundleId = ioReadVarIntU64(map)};
                blockMapRefAdd.offset = ioReadVarIntU64(map);

                // Add reference to list
                blockMapRef = lstAdd(refList, &blockMapRefAdd);
            }
            else
                blockMapRef->offset += ioReadVarIntU64(map);
        }

        BlockMapItem blockMapItem =
        {
            .reference = blockMapRef->reference,
            .bundleId = blockMapRef->bundleId,
            .offset = blockMapRef->offset,
            .size = ioReadVarIntU64(map),
        };

        if (blockMapItem.size == 0)
        {
            blockMapRef = NULL;
        }
        else
        {
            blockMapRef->offset += blockMapItem.size;

            bufUsedZero(checksum);
            ioRead(map, checksum);
            memcpy(blockMapItem.checksum, bufPtr(checksum), bufUsed(checksum));

            // Add to list
            lstAdd((List *)this, &blockMapItem);
        }
    }
    while (1);

    FUNCTION_TEST_RETURN(BLOCK_MAP, this);
}

/**********************************************************************************************************************************/
void
blockMapWrite(const BlockMap *const this, IoWrite *const output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, this);
        FUNCTION_TEST_PARAM(IO_WRITE, output);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(blockMapSize(this) > 0);
    ASSERT(output != NULL);

    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);
    BlockMapRef *blockMapRef = NULL;

    // !!!
    unsigned int referenceLast = UINT_MAX;

    for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(this); blockMapIdx++)
    {
        const BlockMapItem *const blockMapItem = blockMapGet(this, blockMapIdx);

        if (referenceLast != blockMapItem->reference)
        {
            if (referenceLast != UINT_MAX)
                ioWriteVarIntU64(output, 0);

            // Add reference
            ioWriteVarIntU64(output, blockMapItem->reference + 1);

            // !!! WRITE REFERENCE, BUNDLE ID, CHECKSUM

            // Check if the reference is already in the list
            blockMapRef = lstFind(refList, &(BlockMapRef){.reference = blockMapItem->reference});

            if (blockMapRef == NULL)
            {
                // Add bundle id and offset
                ioWriteVarIntU64(output, blockMapItem->bundleId);
                ioWriteVarIntU64(output, blockMapItem->offset);

                // Add reference to list
                blockMapRef = lstAdd(
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
                ioWriteVarIntU64(output, blockMapItem->offset - blockMapRef->offset);

                // Update the offset
                blockMapRef->offset = blockMapItem->offset;
            }

            referenceLast = blockMapItem->reference;
        }

        // Add size !!! SHOULD THE SIZE BE DELTA AS WELL TO SAVE SOME MORE SPACE?
        ioWriteVarIntU64(output, blockMapItem->size);
        blockMapRef->offset += blockMapItem->size;

        // Add checksum
        ioWrite(output, BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE));
    }

    // Write reference end
    ioWriteVarIntU64(output, 0);

    // Write map end
    ioWriteVarIntU64(output, 0);

    FUNCTION_TEST_RETURN_VOID();
}
