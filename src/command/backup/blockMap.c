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

    - Varint-128 encoded super block size. The very first size in the map will be encoded directly and subsequent sizes will be
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
#define BLOCK_MAP_FLAG_LAST                                         1   // Last reference, super block, etc.
#define BLOCK_MAP_FLAG_OFFSET                                       4   // Reference has an offset
#define BLOCK_MAP_FLAG_BUNDLE_ID                                    2   // Reference has a bundle id
#define BLOCK_MAP_FLAG_CONTINUE                                     2   // Reference continues a prior super block
#define BLOCK_MAP_FLAG_CONTINUE_LAST                                4   // Continued super block is last for the reference
#define BLOCK_MAP_REFERENCE_SHIFT                                   3   // Shift bits for reference
#define BLOCK_MAP_FLAG_SUPER_BLOCK_SIZE_REMAINDER                   1   // Remainder for super block size
#define BLOCK_MAP_SUPER_BLOCK_SIZE_SHIFT                            1   // Shift bits for super block size
#define BLOCK_MAP_FLAG_SUPER_BLOCK_CHANGE                           2   // The super block size has changed
#define BLOCK_MAP_FLAG_SUPER_BLOCK_TOTAL_OFFSET                     4   // Block total/offset when not complete
#define BLOCK_MAP_SUPER_BLOCK_SHIFT                                 3   // Shift bits for super block
#define BLOCK_MAP_FLAG_BLOCK_TOTAL_OFFSET                           1   // Block total has an offset
#define BLOCK_MAP_BLOCK_TOTAL_SHIFT                                 1   // Shift bits for block total

typedef enum
{
    blockMapFlagVersion = 0,                                        // Version (currently always 0)
} BlockMapFlag;

// Stores current information about a reference to avoid needed to encode it again
typedef struct BlockMapReference
{
    unsigned int reference;                                         // Reference
    uint64_t superBlockSize;                                        // Super block size
    uint64_t bundleId;                                              // Bundle id
    uint64_t offset;                                                // Offset
    uint64_t size;                                                  // Stored super block size (with compression, etc.)
    uint64_t block;                                                 // Block no
} BlockMapReference;

// Reference comparator
static int
lstComparatorBlockMapReference(const void *const blockMapRef1, const void *const blockMapRef2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, blockMapRef1);
        FUNCTION_TEST_PARAM_P(VOID, blockMapRef2);
    FUNCTION_TEST_END();

    ASSERT(blockMapRef1 != NULL);
    ASSERT(blockMapRef2 != NULL);

    const unsigned int reference1 = ((const BlockMapReference *)blockMapRef1)->reference;
    const unsigned int reference2 = ((const BlockMapReference *)blockMapRef2)->reference;

    FUNCTION_TEST_RETURN(INT, LST_COMPARATOR_CMP(reference1, reference2));
}

FN_EXTERN BlockMap *
blockMapNewRead(IoRead *const map, const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, map);
    FUNCTION_LOG_END();

    // Read flags. Currently the version flag must always be zero. This may be used in the future to indicate if the map version
    // has changed.
    CHECK(FormatError, (ioReadVarIntU64(map) & (1 << blockMapFlagVersion)) == 0, "block map version must be zero");

    // Read all references in packed format
    BlockMap *const this = blockMapNew();
    List *const refList = lstNewP(sizeof(BlockMapReference), .comparator = lstComparatorBlockMapReference);
    Buffer *const checksum = bufNew(checksumSize);
    int64_t sizeLast = 0;
    bool referenceContinue = false;

    do
    {
        // Read reference
        const uint64_t referenceEncoded = ioReadVarIntU64(map);
        BlockMapItem blockMapItem = {.reference = (unsigned int)(referenceEncoded >> BLOCK_MAP_REFERENCE_SHIFT)};
        BlockMapReference *referenceData = lstFind(refList, &(BlockMapReference){.reference = blockMapItem.reference});

        // If this is the first time this reference has been read
        if (referenceData == NULL)
        {
            // Read bundle id
            if (referenceEncoded & BLOCK_MAP_FLAG_BUNDLE_ID)
                blockMapItem.bundleId = ioReadVarIntU64(map);

            // Read offset
            if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                blockMapItem.offset = ioReadVarIntU64(map);

            // Default super block size
            blockMapItem.superBlockSize = blockSize;

            // Add reference to list
            BlockMapReference referenceDataAdd =
            {
                .reference = blockMapItem.reference,
                .superBlockSize = blockMapItem.superBlockSize,
                .bundleId = blockMapItem.bundleId,
                .offset = blockMapItem.offset,
            };

            referenceData = lstAdd(refList, &referenceDataAdd);
        }
        // Else this reference has been read before
        else
        {
            blockMapItem.superBlockSize = referenceData->superBlockSize;
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

                superBlockEncoded |= BLOCK_MAP_FLAG_SUPER_BLOCK_TOTAL_OFFSET;
                referenceContinue = false;
            }
            // Else read the super block size for the reference
            else
            {
                superBlockEncoded = ioReadVarIntU64(map);

                // If this is the first size read then just read the size. Otherwise read the difference from the prior size and
                // add sizeLast.
                blockMapItem.size = superBlockEncoded >> BLOCK_MAP_SUPER_BLOCK_SHIFT;

                if (sizeLast != 0)
                    blockMapItem.size = (uint64_t)(cvtInt64FromZigZag(blockMapItem.size) + sizeLast);

                // If the super block size has changed then read it
                if (superBlockEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_CHANGE)
                {
                    const uint64_t superBlockSizeEncoded = ioReadVarIntU64(map);
                    blockMapItem.superBlockSize = (superBlockSizeEncoded >> BLOCK_MAP_SUPER_BLOCK_SIZE_SHIFT) * blockSize;

                    if (superBlockSizeEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_SIZE_REMAINDER)
                        blockMapItem.superBlockSize += ioReadVarIntU64(map);

                    referenceData->superBlockSize = blockMapItem.superBlockSize;
                }

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

            // Read or calculate block total
            uint64_t blockTotal;

            if (superBlockEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_TOTAL_OFFSET)
            {
                const uint64_t blockTotalEncoded = ioReadVarIntU64(map);
                blockTotal = (blockTotalEncoded >> BLOCK_MAP_BLOCK_TOTAL_SHIFT) + 1;

                // Offset block no from expected
                if (blockTotalEncoded & BLOCK_MAP_FLAG_BLOCK_TOTAL_OFFSET)
                    referenceData->block += ioReadVarIntU64(map);
            }
            else
                blockTotal = blockMapItem.superBlockSize / blockSize + (blockMapItem.superBlockSize % blockSize == 0 ? 0 : 1);

            // Read checksums
            for (uint64_t blockIdx = 0; blockIdx < blockTotal; blockIdx++)
            {
                // Set block no
                blockMapItem.block = referenceData->block + blockIdx;

                // Read checksum
                bufUsedZero(checksum);
                ioRead(map, checksum);
                memcpy(blockMapItem.checksum, bufPtr(checksum), bufUsed(checksum));

                // Add to block list
                lstAdd((List *)this, &blockMapItem);
            }

            // Update block in reference with all blocks read
            referenceData->block += blockTotal;

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
blockMapWrite(const BlockMap *const this, IoWrite *const output, const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_MAP, this);
        FUNCTION_LOG_PARAM(IO_WRITE, output);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(SIZE, checksumSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(blockMapSize(this) > 0);
    ASSERT(blockSize > 0);
    ASSERT(output != NULL);

    // Write flags
    ioWriteVarIntU64(output, 0);

    // Write all references in packed format
    List *const refList = lstNewP(sizeof(BlockMapReference), .comparator = lstComparatorBlockMapReference);
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
        BlockMapReference *referenceData = lstFind(refList, &(BlockMapReference){.reference = reference->reference});

        if (referenceData == NULL)
        {
            // Add bundle id and offset flags
            if (reference->bundleId > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_BUNDLE_ID;

            if (reference->offset > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_OFFSET;

            // Write the references
            ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);

            // Write bundle id and offset
            if (referenceEncoded & BLOCK_MAP_FLAG_BUNDLE_ID)
                ioWriteVarIntU64(output, reference->bundleId);

            if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                ioWriteVarIntU64(output, reference->offset);

            // Add reference to list
            const BlockMapReference referenceAdd =
            {
                .reference = reference->reference,
                .superBlockSize = blockSize,
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

            // If the offset is identical then reference is continuing an already started super block. The super block size and
            // block no should be reused. Note that writing the reference is deferred until we know if the continued super block is
            // the last one for the reference.
            if (reference->offset == referenceData->offset)
            {
                ASSERT(reference->superBlockSize == referenceData->superBlockSize);

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

            // If block offset or total need to be stored then add the flag
            const unsigned int blockTotal = superBlockIdx - blockIdx;
            ASSERT(blockTotal > 0);

            if (referenceContinue || superBlock->block != 0 ||
                blockTotal != superBlock->superBlockSize / blockSize + (superBlock->superBlockSize % blockSize == 0 ? 0 : 1))
            {
                superBlockEncoded |= BLOCK_MAP_FLAG_SUPER_BLOCK_TOTAL_OFFSET;
            }

            // Write the continued reference now that we know if this will be the last super block in the reference
            if (referenceContinue)
            {
                ASSERT(superBlock->superBlockSize == referenceData->superBlockSize);

                if (superBlockEncoded & BLOCK_MAP_FLAG_LAST)
                    referenceEncoded |= BLOCK_MAP_FLAG_CONTINUE_LAST;

                ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);
                referenceContinue = false;
            }
            // Else write the super block size for the reference
            else
            {
                // Set offset, size, and block for the super block
                referenceData->offset = superBlock->offset;
                referenceData->size = superBlock->size;
                referenceData->block = 0;

                // If the super block size has changed then add the flag
                ASSERT(reference->superBlockSize > 0);

                if (superBlock->superBlockSize != referenceData->superBlockSize)
                    superBlockEncoded |= BLOCK_MAP_FLAG_SUPER_BLOCK_CHANGE;

                // If this is the first size written then just write the size. Otherwise write the difference from the prior size.
                // This depends on the expectation that the compressed size of equal-sized blocks will be similar in order to be
                // most efficient.
                ioWriteVarIntU64(
                    output,
                    superBlockEncoded |
                    (sizeLast == 0 ? superBlock->size : cvtInt64ToZigZag((int64_t)superBlock->size - sizeLast)) <<
                    BLOCK_MAP_SUPER_BLOCK_SHIFT);

                // If the super block size has changed then write it
                if (superBlockEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_CHANGE)
                {
                    const uint64_t superBlockSizeEncoded =
                        (superBlock->superBlockSize / blockSize) << BLOCK_MAP_SUPER_BLOCK_SIZE_SHIFT |
                        (superBlock->superBlockSize % blockSize == 0 ? 0 : BLOCK_MAP_FLAG_SUPER_BLOCK_SIZE_REMAINDER);

                    ioWriteVarIntU64(output, superBlockSizeEncoded);

                    if (superBlockSizeEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_SIZE_REMAINDER)
                        ioWriteVarIntU64(output, superBlock->superBlockSize % blockSize);

                    referenceData->superBlockSize = superBlock->superBlockSize;
                }
            }

            sizeLast = (int64_t)superBlock->size;

            // Write block total if the super block does not include all blocks with no block offset
            if (superBlockEncoded & BLOCK_MAP_FLAG_SUPER_BLOCK_TOTAL_OFFSET)
            {
                // Write total blocks in the super block
                const uint64_t blockTotalEncoded =
                    (blockTotal - 1) << BLOCK_MAP_BLOCK_TOTAL_SHIFT |
                    (superBlock->block - referenceData->block > 0 ? BLOCK_MAP_FLAG_BLOCK_TOTAL_OFFSET : 0);

                ioWriteVarIntU64(output, blockTotalEncoded);

                // If there is a gap in block no from the prior super block. This can happen when a super block is continued or
                // had blocks at the beginning overridden by a newer super block.
                if (blockTotalEncoded & BLOCK_MAP_FLAG_BLOCK_TOTAL_OFFSET)
                {
                    ioWriteVarIntU64(output, superBlock->block - referenceData->block);
                    referenceData->block = superBlock->block;
                }
            }

            ASSERT(superBlock->block >= referenceData->block);

            // Increment reference block by number of blocks written
            referenceData->block += blockTotal;

            // Write checksums
            for (; blockIdx < superBlockIdx; blockIdx++)
            {
                ASSERT(
                    superBlock == blockMapGet(this, blockIdx) ||
                    (blockIdx > 0 && (blockMapGet(this, blockIdx)->block == blockMapGet(this, blockIdx - 1)->block + 1)));

                ioWrite(output, BUF(blockMapGet(this, blockIdx)->checksum, checksumSize));
            }
        }
    }

    lstFree(refList);

    FUNCTION_LOG_RETURN_VOID();
}
