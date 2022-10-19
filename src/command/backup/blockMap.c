/***********************************************************************************************************************************
Block Incremental Map

The block map is stored as a series of block info that are abbreviated when sequential blocks are from the same reference:

- Each block logically contains all fields in BlockMapItem but not all fields are encoded for each block and some are encoded as
  deltas:

  - Varint-128 encoded reference (which is an index into the reference list maintained in the manifest). If the prior block has the
    same reference then this is omitted.

  - If this is the first time the reference appears it will be followed by a bundle id (0 if no bundle). The bundle id is always the
    same for a reference so it does not need to be encoded more than once.

  - Offset where the block is located. For the first block after the reference, the offset is varint-128 encoded. After that it is a
    delta of the prior offset for the reference. The offset is not encoded if the block is sequential to the prior block in the
    reference. In this case the offset can be calculated by adding the prior size to the prior offset.

  - Block size. For the first block this is the varint-128 encoded size. Afterwards it is a delta of the previous block using the
    following formula: cvtInt64ToZigZag(blockSize - blockSizeLast) + 1. Adding one is required so the size delta is never zero,
    which is used as the stop byte.

  - SHA1 checksum of the block.

  - If the next block is from a different reference then a varint-128 encoded zero stop byte is added.

The block list is terminated by a varint-128 encoded zero stop byte.
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
    uint64_t sizeLast = 0;

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
            // The first size is read directly and then each subsequent size is a delta of the previous size. Subtract one from the
            // delta which is required to distinguish it from the stop byte.
            if (sizeLast == 0)
                blockMapItem.size = blockMapItem.size;
            else
                blockMapItem.size = (uint64_t)(cvtInt64FromZigZag(blockMapItem.size - 1) + (int64_t)sizeLast);

            sizeLast = blockMapItem.size;

            // Add size to offset
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

    // Write all block items into a packed format
    unsigned int referenceLast = UINT_MAX;
    uint64_t sizeLast = 0;

    for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(this); blockMapIdx++)
    {
        const BlockMapItem *const blockMapItem = blockMapGet(this, blockMapIdx);

        if (referenceLast != blockMapItem->reference)
        {
            if (referenceLast != UINT_MAX)
                ioWriteVarIntU64(output, 0);

            // Add reference
            ioWriteVarIntU64(output, blockMapItem->reference + 1);

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
                ASSERT(blockMapItem->offset >= blockMapRef->offset);

                // Add offset delta
                ioWriteVarIntU64(output, blockMapItem->offset - blockMapRef->offset);

                // Update the offset
                blockMapRef->offset = blockMapItem->offset;
            }

            referenceLast = blockMapItem->reference;
        }

        // The first size is stored directly and then each subsequent size is a delta of the previous size. Add one to the delta
        // so it can be distinguished from the stop byte.
        if (sizeLast == 0)
            ioWriteVarIntU64(output, blockMapItem->size);
        else
            ioWriteVarIntU64(output, cvtInt64ToZigZag((int64_t)blockMapItem->size - (int64_t)sizeLast) + 1);

        sizeLast = blockMapItem->size;

        // Add size to offset
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
