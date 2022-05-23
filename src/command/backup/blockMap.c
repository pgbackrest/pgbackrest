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

    // !!!
    unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
    size_t bufferPos = 0;
    unsigned int referenceLast = UINT_MAX;

    for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(this); blockMapIdx++)
    {
        const BlockMapItem *const blockMapItem = blockMapGet(this, blockMapIdx);

        if (referenceLast != blockMapItem->reference)
        {
            if (referenceLast != UINT_MAX)
            {
                bufferPos = 0;
                cvtUInt64ToVarInt128(0, buffer, &bufferPos, sizeof(buffer));
                ioWrite(output, BUF(buffer, bufferPos));
            }

            // Add reference
            bufferPos = 0;
            cvtUInt64ToVarInt128(blockMapItem->reference, buffer, &bufferPos, sizeof(buffer));
            ioWrite(output, BUF(buffer, bufferPos));

            // !!! WRITE REFERENCE, BUNDLE ID, CHECKSUM

            // Check if the reference is already in the list
            BlockMapRef *const blockMapRef = lstFind(
                refList, &(BlockMapRef){.reference = blockMapItem->reference, .bundleId = blockMapItem->bundleId});

            if (blockMapRef == NULL)
            {
                // Add bundle id
                bufferPos = 0;
                cvtUInt64ToVarInt128(blockMapItem->bundleId, buffer, &bufferPos, sizeof(buffer));
                ioWrite(output, BUF(buffer, bufferPos));

                // Add offset
                bufferPos = 0;
                cvtUInt64ToVarInt128(blockMapItem->offset, buffer, &bufferPos, sizeof(buffer));
                ioWrite(output, BUF(buffer, bufferPos));

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
                ioWrite(output, BUF(buffer, bufferPos));

                // Update the offset
                blockMapRef->offset = blockMapItem->offset;
            }

            referenceLast = blockMapItem->reference;
        }

        // Add rolling offset delta
        bufferPos = 0;
        cvtUInt64ToVarInt128(blockMapItem->size, buffer, &bufferPos, sizeof(buffer));
        ioWrite(output, BUF(buffer, bufferPos));

        // Add checksum
        ioWrite(output, BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE));
    }

    bufferPos = 0;
    cvtUInt64ToVarInt128(0, buffer, &bufferPos, sizeof(buffer));
    ioWrite(output, BUF(buffer, bufferPos));

    FUNCTION_TEST_RETURN_VOID();
}
