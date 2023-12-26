/***********************************************************************************************************************************
Harness for Testing Block Deltas
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/blockDelta.h"

#include "common/harnessBlockIncr.h"
#include "common/harnessDebug.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/**********************************************************************************************************************************/
String *
hrnBlockDeltaRender(const BlockMap *const blockMap, const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_HARNESS_PARAM(SIZE, blockSize);
        FUNCTION_HARNESS_PARAM(SIZE, checksumSize);
    FUNCTION_HARNESS_END();

    ASSERT(blockMap != NULL);
    ASSERT(blockSize > 0);

    String *const result = strNew();
    BlockDelta *const blockDelta = blockDeltaNew(blockMap, blockSize, checksumSize, NULL, cipherTypeNone, NULL, compressTypeNone);

    for (unsigned int readIdx = 0; readIdx < blockDeltaReadSize(blockDelta); readIdx++)
    {
        const BlockDeltaRead *const read = blockDeltaReadGet(blockDelta, readIdx);

        strCatFmt(
            result, "read {reference: %u, bundleId: %" PRIu64 ", offset: %" PRIu64 ", size: %" PRIu64 "}\n", read->reference,
            read->bundleId, read->offset, read->size);

        for (unsigned int superBlockIdx = 0; superBlockIdx < lstSize(read->superBlockList); superBlockIdx++)
        {
            const BlockDeltaSuperBlock *const superBlock = lstGet(read->superBlockList, superBlockIdx);

            strCatFmt(
                result, "  super block {max: %" PRIu64 ", size: %" PRIu64 "}\n", superBlock->superBlockSize, superBlock->size);

            for (unsigned int blockIdx = 0; blockIdx < lstSize(superBlock->blockList); blockIdx++)
            {
                const BlockDeltaBlock *const block = lstGet(superBlock->blockList, blockIdx);

                strCatFmt(result, "    block {no: %" PRIu64 ", offset: %" PRIu64 "}\n", block->no, block->offset);
            }
        }
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}
