/***********************************************************************************************************************************
Block Incremental Map

The block map is stored as a flag and a series of reference, super block, and block info:

- Varint-128 flag that contains the version and info about the map (e.g. are the super blocks and blocks equal size).

- List of references:

  - Varint-128 encoded reference (which is an index into the reference list maintained in the manifest). If this is the first time
    the reference appears it will be followed by a bundle id and an offset if they are not 0. If the reference has appeared before
    it might update the offset or be a continuation of a prior super block. Continuations happen when a super block is split by a
    newer super block. The continuation allows the prior super block values for the reference to be used without encoding them
    again.

  - List of super blocks:

    - Varint-128 encoded super block size. The very first size in the map will be encoded plainly and subsequent sizes will be
      encoded as the delta from the last size.

    - List of blocks:

      - Varint-128 encoded block number if super block size does not equal block size. If they are equal then there is one block per
        super block so no reason to encode the block number.

      - Checksum.

References, super blocks, and blocks are encoded with a bit that indicates when the last one has been reached.
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockMap.h"
#include "common/debug.h"
#include "common/log.h"

/**********************************************************************************************************************************/
#define BLOCK_MAP_FLAG_LAST                                         1
#define BLOCK_MAP_FLAG_OFFSET                                       4
#define BLOCK_MAP_FLAG_BUNDLE_ID                                    2
#define BLOCK_MAP_FLAG_CONTINUE                                     2
#define BLOCK_MAP_FLAG_CONTINUE_LAST                                4
#define BLOCK_MAP_REFERENCE_SHIFT                                   3
#define BLOCK_MAP_SUPER_BLOCK_SHIFT                                 1
#define BLOCK_MAP_BLOCK_SHIFT                                       1

typedef enum
{
    blockMapFlagVersion = 0,
    blockMapFlagEqual = 1,
} BlockMapFlag;

typedef struct BlockMapRef
{
    unsigned int reference;
    uint64_t bundleId;
    uint64_t offset;
    uint64_t size;
    uint64_t block;
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

FN_EXTERN BlockMap *
blockMapNewRead(IoRead *const map)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, map);
    FUNCTION_LOG_END();

    // Read flags. Currently the version flag must always be zero. This may be used in the future to indicate if the map version
    // has changed.
    const uint64_t flags = ioReadVarIntU64(map);
    CHECK(FormatError, (flags & (1 << blockMapFlagVersion)) == 0, "block map version must be zero");

    // Are the super block and block size equal?
    const bool blockEqual = flags & (1 << blockMapFlagEqual);

    // Read all references in packed format
    BlockMap *const this = blockMapNew();
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);
    Buffer *const checksum = bufNew(HASH_TYPE_SHA1_SIZE);
    int64_t sizeLast = 0;
    bool referenceContinue = false;

    do
    {
        // Read reference
        const uint64_t referenceEncoded = ioReadVarIntU64(map);
        BlockMapItem blockMapItem = {.reference = (unsigned int)(referenceEncoded >> BLOCK_MAP_REFERENCE_SHIFT)};
        BlockMapRef *referenceData = lstFind(refList, &(BlockMapRef){.reference = blockMapItem.reference});

        // If this is the first time this reference has been read
        if (referenceData == NULL)
        {
            // Read bundle id
            if (referenceEncoded & BLOCK_MAP_FLAG_BUNDLE_ID)
                blockMapItem.bundleId = ioReadVarIntU64(map);

            // Read offset
            if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                blockMapItem.offset = ioReadVarIntU64(map);

            // Add reference to list
            BlockMapRef referenceDataAdd =
            {
                .reference = blockMapItem.reference,
                .bundleId = blockMapItem.bundleId,
                .offset = blockMapItem.offset,
            };

            referenceData = lstAdd(refList, &referenceDataAdd);
        }
        // Else this reference has been read before
        else
        {
            blockMapItem.bundleId = referenceData->bundleId;

            // If the reference is continued use the prior offset and size values
            if (referenceEncoded & BLOCK_MAP_FLAG_CONTINUE)
            {
                blockMapItem.offset = referenceData->offset;
                blockMapItem.size = referenceData->size;
                referenceContinue = true;
            }
            // Else this is a new reference and super block with a possible offset update
            else
            {
                blockMapItem.offset = referenceData->offset + referenceData->size;

                if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                    blockMapItem.offset += ioReadVarIntU64(map);

                referenceData->offset = blockMapItem.offset;
            }
        }

        // Read all super blocks in the current reference in packed format
        bool superBlockFirst = true;

        do
        {
            uint64_t superBlockEncoded = 0;

            // If the reference was continued check if this is the last super block in the reference
            if (referenceContinue)
            {
                if (referenceEncoded & BLOCK_MAP_FLAG_CONTINUE_LAST)
                    superBlockEncoded = BLOCK_MAP_FLAG_LAST;

                referenceContinue = false;
            }
            // Else read the super block size for the reference
            else
            {
                superBlockEncoded = ioReadVarIntU64(map);

                // If this is the first size read then just read the size
                if (sizeLast == 0)
                {
                    blockMapItem.size = superBlockEncoded >> BLOCK_MAP_SUPER_BLOCK_SHIFT;
                }
                // Else read the difference from the prior size and apply to sizeLast
                else
                    blockMapItem.size = (uint64_t)(cvtInt64FromZigZag(superBlockEncoded >> BLOCK_MAP_SUPER_BLOCK_SHIFT) + sizeLast);

                // Set offset, size, and block for the super block
                if (superBlockFirst)
                    referenceData->offset = blockMapItem.offset;
                else
                    referenceData->offset += (uint64_t)sizeLast;

                referenceData->size = blockMapItem.size;
                referenceData->block = 0;
            }

            // Update sizeLast with the current size and clear superBlockFirst
            sizeLast = (int64_t)blockMapItem.size;
            superBlockFirst = false;

            // Read all blocks in the current super block in packed format
            do
            {
                // Block no only needs to be read when super block size does not equal block size
                uint64_t blockEncoded = BLOCK_MAP_FLAG_LAST;

                if (!blockEqual)
                {
                    // Read block no as the difference from the prior block
                    blockEncoded = ioReadVarIntU64(map);
                    blockMapItem.block = (blockEncoded >> BLOCK_MAP_BLOCK_SHIFT) + referenceData->block;
                    referenceData->block = blockMapItem.block;
                }

                // Read checksum
                bufUsedZero(checksum);
                ioRead(map, checksum);
                memcpy(blockMapItem.checksum, bufPtr(checksum), bufUsed(checksum));

                // Add to block list
                lstAdd((List *)this, &blockMapItem);

                // Break when this is the last block in the super block
                if (blockEncoded & BLOCK_MAP_FLAG_LAST)
                    break;
            }
            while (true);

            // Update offset with the super block size
            blockMapItem.offset += blockMapItem.size;

            // Break when this is the last super block in the reference
            if (superBlockEncoded & BLOCK_MAP_FLAG_LAST)
                break;
        }
        while (true);

        // Break when this is the last reference
        if (referenceEncoded & BLOCK_MAP_FLAG_LAST)
            break;
    }
    while (true);

    lstFree(refList);
    bufFree(checksum);

    FUNCTION_LOG_RETURN(BLOCK_MAP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
blockMapWrite(const BlockMap *const this, IoWrite *const output, bool blockEqual)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_MAP, this);
        FUNCTION_LOG_PARAM(IO_WRITE, output);
        FUNCTION_LOG_PARAM(BOOL, blockEqual);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(blockMapSize(this) > 0);
    ASSERT(output != NULL);

    // Set flag to indicate that super block and block size are equal
    ioWriteVarIntU64(output, blockEqual ? 1 << blockMapFlagEqual : 0);

    // Write all references in packed format
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);
    unsigned int referenceIdx = 0;
    int64_t sizeLast = 0;
    bool referenceContinue = false;

    while (referenceIdx < blockMapSize(this))
    {
        const BlockMapItem *const reference = blockMapGet(this, referenceIdx);
        unsigned int superBlockIdx = referenceIdx;
        unsigned int blockIdx = referenceIdx;

        // Determine if this is the last reference
        uint64_t referenceEncoded = BLOCK_MAP_FLAG_LAST;

        for (referenceIdx++; referenceIdx < blockMapSize(this); referenceIdx++)
        {
            if (reference->reference != blockMapGet(this, referenceIdx)->reference)
            {
                referenceEncoded = 0;
                break;
            }

            ASSERT(reference->offset <= blockMapGet(this, referenceIdx)->offset);
        }

        // If this is the first time this reference has been written
        BlockMapRef *referenceData = lstFind(refList, &(BlockMapRef){.reference = reference->reference});

        if (referenceData == NULL)
        {
            // Add bundle id and offset flags
            if (reference->bundleId > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_BUNDLE_ID;

            if (reference->offset > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_OFFSET;

            // Write the references
            ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);

            // Add bundle id and offset
            if (referenceEncoded & BLOCK_MAP_FLAG_BUNDLE_ID)
                ioWriteVarIntU64(output, reference->bundleId);

            if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                ioWriteVarIntU64(output, reference->offset);

            // Add reference to list
            const BlockMapRef referenceAdd =
            {
                .reference = reference->reference,
                .bundleId = reference->bundleId,
                .offset = reference->offset,
            };

            referenceData = lstAdd(refList, &referenceAdd);
        }
        // Else this reference has been written before
        else
        {
            ASSERT(reference->reference == referenceData->reference);
            ASSERT(reference->bundleId == referenceData->bundleId);
            ASSERT(reference->offset >= referenceData->offset);

            // If the offset is identical then reference is continuing the super block started by a prior reference. The prior
            // super block size and block no should be used. Not that writing the reference is deferred until we know if the
            // continued super block is the last one for the reference.
            if (reference->offset == referenceData->offset)
            {
                referenceEncoded |= BLOCK_MAP_FLAG_CONTINUE;
                referenceContinue = true;
            }
            // Else the reference starts a new super block which means the offset may need to be stored if there is a gap from the
            // prior super block
            else
            {
                if (reference->offset > referenceData->offset + referenceData->size)
                    referenceEncoded |= BLOCK_MAP_FLAG_OFFSET;

                ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);

                if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                    ioWriteVarIntU64(output, reference->offset - (referenceData->offset + referenceData->size));

                referenceData->offset = reference->offset;
                referenceData->size = reference->size;
            }
        }

        // Write all super blocks in the current reference in packed format
        while (superBlockIdx < referenceIdx)
        {
            const BlockMapItem *const superBlock = blockMapGet(this, superBlockIdx);

            // Determine if this is the last super block in the reference
            uint64_t superBlockEncoded = BLOCK_MAP_FLAG_LAST;

            for (superBlockIdx++; superBlockIdx < referenceIdx; superBlockIdx++)
            {
                if (superBlock->offset != blockMapGet(this, superBlockIdx)->offset)
                {
                    superBlockEncoded = 0;
                    break;
                }
            }

            // Write the continued reference now that we know if this will be the last super block in the reference
            if (referenceContinue)
            {
                if (superBlockEncoded & BLOCK_MAP_FLAG_LAST)
                    referenceEncoded |= BLOCK_MAP_FLAG_CONTINUE_LAST;

                ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);
                referenceContinue = false;
            }
            // Else write the super block size for the reference
            else
            {
                // If this is the first size written then just write the size
                if (sizeLast == 0)
                {
                    ioWriteVarIntU64(output, superBlockEncoded | superBlock->size << BLOCK_MAP_SUPER_BLOCK_SHIFT);
                }
                // Else write the difference from the prior size. This depends on the idea that the compressed size of equal-sized
                // blocks will be similar.
                else
                {
                    ioWriteVarIntU64(
                        output,
                        superBlockEncoded | cvtInt64ToZigZag((int64_t)superBlock->size - sizeLast) << BLOCK_MAP_SUPER_BLOCK_SHIFT);
                }

                // Set offset, size, and block for the super block
                referenceData->offset = superBlock->offset;
                referenceData->size = superBlock->size;
                referenceData->block = 0;
            }

            sizeLast = (int64_t)superBlock->size;

            // Write all blocks in the current super block in packed format
            while (blockIdx < superBlockIdx)
            {
                const BlockMapItem *const block = blockMapGet(this, blockIdx);
                ASSERT(block->block >= referenceData->block);

                // Block no only needs to be written when super block size does not equal block size
                if (!blockEqual)
                {
                    // Write block no as the difference from the prior block
                    ioWriteVarIntU64(
                        output,
                        (blockIdx == superBlockIdx - 1 ? BLOCK_MAP_FLAG_LAST : 0) |
                        (block->block - referenceData->block) << BLOCK_MAP_BLOCK_SHIFT);

                    referenceData->block = block->block;
                }

                // Write checksum
                ioWrite(output, BUF(block->checksum, HASH_TYPE_SHA1_SIZE));

                blockIdx++;
            }
        }
    }

    lstFree(refList);

    FUNCTION_LOG_RETURN_VOID();
}
