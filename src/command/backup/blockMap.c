/***********************************************************************************************************************************
Block Incremental Map

The block map is stored as a flag and a series of block info that are abbreviated when sequential blocks are from the same
reference:

!!! FIX THIS DESCRIPTION

- Varint-128 that contains info about the map (e.g. are the super blocks and block equal size).

- Each block logically contains all fields in BlockMapItem but not all fields are encoded for each block and some are encoded as
  deltas:

  - Varint-128 encoded reference (which is an index into the reference list maintained in the manifest). If the prior block has the
    same reference then this is omitted.

  - If this is the first time the reference appears it will be followed by a bundle id (0 if no bundle). The bundle id is always the
    same for a reference so it does not need to be encoded more than once.

  - Offset where the super block is located. For the first super block after the reference, the offset is varint-128 encoded. After
    that it is a delta of the prior offset for the reference. The offset is not encoded if the super block is sequential to the
    prior super block in the reference. In this case the offset can be calculated by adding the prior size to the prior offset.

  - Super block size. For the first super block this is the varint-128 encoded size. Afterwards it is a delta of the previous super
    block using the following formula: cvtInt64ToZigZag(blockSize - blockSizeLast) + 1. Adding one is required so the size delta is
    never zero, which is used as the stop byte.

  - Block no within the super block.

  - SHA1 checksum of the block.

  - If the next block is from a different super block then a varint-128 encoded zero stop byte is added.

  - If the next super block is from a different reference then a varint-128 encoded zero stop byte is added.

The block map is terminated by a varint-128 encoded zero stop byte.
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h> // !!!

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

    BlockMap *const this = blockMapNew();
    List *const refList = lstNewP(sizeof(BlockMapRef), .comparator = lstComparatorBlockMapRef);
    Buffer *const checksum = bufNew(HASH_TYPE_SHA1_SIZE);
    int64_t sizeLast = 0;
    bool referenceContinue = false;
    const bool blockEqual = ioReadVarIntU64(map);

    //fprintf(stdout, "!!!READ MAP\n");

    do
    {
        const uint64_t referenceEncoded = ioReadVarIntU64(map);
        BlockMapItem blockMapItem = {.reference = (unsigned int)(referenceEncoded >> BLOCK_MAP_REFERENCE_SHIFT)};
        BlockMapRef *referenceData = lstFind(refList, &(BlockMapRef){.reference = blockMapItem.reference});

        if (referenceData == NULL)
        {
            if (referenceEncoded & BLOCK_MAP_FLAG_BUNDLE_ID)
                blockMapItem.bundleId = ioReadVarIntU64(map);

            if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                blockMapItem.offset = ioReadVarIntU64(map);

            //fprintf(stdout, "!!!  REF NEW %u OFFSET %zu\n", blockMapItem.reference, blockMapItem.offset);

            BlockMapRef referenceDataAdd =
            {
                .reference = blockMapItem.reference,
                .bundleId = blockMapItem.bundleId,
                .offset = blockMapItem.offset,
            };

            // Add reference to list
            referenceData = lstAdd(refList, &referenceDataAdd);
        }
        // Else increment the offset
        else
        {
            blockMapItem.bundleId = referenceData->bundleId;

            //fprintf(stdout, "!!!  REF PRIOR %u PRIOR OFFSET %zu PRIOR SIZE %zu\n", blockMapItem.reference, referenceData->offset, referenceData->size);

            if (referenceEncoded & BLOCK_MAP_FLAG_CONTINUE)
            {
                //fprintf(stdout, "!!!    REF CONTINUE\n");

                blockMapItem.offset = referenceData->offset;
                blockMapItem.size = referenceData->size;
                referenceContinue = true;
            }
            else
            {
                blockMapItem.offset = referenceData->offset + referenceData->size;

                if (referenceEncoded & BLOCK_MAP_FLAG_OFFSET)
                    blockMapItem.offset += ioReadVarIntU64(map);

                referenceData->offset = blockMapItem.offset;
                referenceData->size = 0; // !!! NEED THIS?
            }
        }

        bool superBlockFirst = true;

        do
        {
            uint64_t superBlockEncoded = 0;

            if (referenceContinue)
            {
                if (referenceEncoded & BLOCK_MAP_FLAG_CONTINUE_LAST) // {uncovered - !!!}
                    superBlockEncoded = BLOCK_MAP_FLAG_LAST;

                referenceContinue = false;
            }
            else
            {
                superBlockEncoded = ioReadVarIntU64(map);

                if (sizeLast == 0)
                    blockMapItem.size = superBlockEncoded >> BLOCK_MAP_SUPER_BLOCK_SHIFT;
                else
                    blockMapItem.size = (uint64_t)(cvtInt64FromZigZag(superBlockEncoded >> BLOCK_MAP_SUPER_BLOCK_SHIFT) + sizeLast);

                if (superBlockFirst)
                    referenceData->offset = blockMapItem.offset;
                else
                    referenceData->offset += (uint64_t)sizeLast;

                referenceData->size = blockMapItem.size;
                referenceData->block = 0;
            }

            //fprintf(stdout, "!!!    SB OFFSET %zu SIZE %zu\n", referenceData->offset, referenceData->size);

            superBlockFirst = false;
            sizeLast = (int64_t)blockMapItem.size;

            do
            {
                uint64_t blockEncoded = BLOCK_MAP_FLAG_LAST;

                if (!blockEqual)
                {
                    blockEncoded = ioReadVarIntU64(map);
                    blockMapItem.block = (blockEncoded >> BLOCK_MAP_BLOCK_SHIFT) + referenceData->block;
                    referenceData->block = blockMapItem.block;
                }

                // Read checksum
                bufUsedZero(checksum);
                ioRead(map, checksum);
                memcpy(blockMapItem.checksum, bufPtr(checksum), bufUsed(checksum));

                lstAdd((List *)this, &blockMapItem);

                if (blockEncoded & BLOCK_MAP_FLAG_LAST)
                    break;
            }
            while (true);

            blockMapItem.offset += blockMapItem.size;

            if (superBlockEncoded & BLOCK_MAP_FLAG_LAST)
                break;
        }
        while (true);

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

    //fprintf(stdout, "!!!WRITE MAP\n");

    // !!! Add a flag var to indicate when super block size == block size to save some bytes
    ioWriteVarIntU64(output, blockEqual ? 1 : 0);

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

        BlockMapRef *referenceData = lstFind(refList, &(BlockMapRef){.reference = reference->reference});

        if (referenceData == NULL)
        {
            //fprintf(stdout, "!!!  REF NEW %u OFFSET %zu\n", reference->reference, reference->offset);

            if (reference->bundleId > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_BUNDLE_ID;

            if (reference->offset > 0)
                referenceEncoded |= BLOCK_MAP_FLAG_OFFSET;

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
        else
        {
            ASSERT(reference->reference == referenceData->reference);
            ASSERT(reference->bundleId == referenceData->bundleId);
            ASSERT(reference->offset >= referenceData->offset);

            //fprintf(stdout, "!!!  REF PRIOR %u OFFSET %zu PRIOR OFFSET %zu PRIOR SIZE %zu\n", reference->reference, reference->offset, referenceData->offset, referenceData->size);

            if (reference->offset == referenceData->offset)
            {
                //fprintf(stdout, "!!!    REF CONTINUE\n");

                referenceEncoded |= BLOCK_MAP_FLAG_CONTINUE;
                referenceContinue = true;
            }
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

            if (referenceContinue)
            {
                if (superBlockEncoded & BLOCK_MAP_FLAG_LAST) // {uncovered - !!!}
                    referenceEncoded |= BLOCK_MAP_FLAG_CONTINUE_LAST;

                ioWriteVarIntU64(output, referenceEncoded | reference->reference << BLOCK_MAP_REFERENCE_SHIFT);
                referenceContinue = false;
            //fprintf(stdout, "!!!    SB CONTINUE\n");
            }
            else
            {
                if (sizeLast == 0)
                    ioWriteVarIntU64(output, superBlockEncoded | superBlock->size << BLOCK_MAP_SUPER_BLOCK_SHIFT);
                else
                {
                    ioWriteVarIntU64(
                        output,
                        superBlockEncoded | cvtInt64ToZigZag((int64_t)superBlock->size - sizeLast) << BLOCK_MAP_SUPER_BLOCK_SHIFT);
                }

            //fprintf(stdout, "!!!    SB OFFSET %zu SIZE %zu\n", superBlock->offset, superBlock->size);

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

                if (!blockEqual)
                {
                    // Write block no
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

    // !!!MAYBE CHECKSUM??? OR IS THIS COVERED BY VERIFY???

    lstFree(refList);

    FUNCTION_LOG_RETURN_VOID();
}
