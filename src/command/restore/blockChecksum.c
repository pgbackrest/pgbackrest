/***********************************************************************************************************************************
Block Hash List
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/blockChecksum.h"
#include "common/crypto/common.h"
#include "common/crypto/xxhash.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockChecksum
{
    size_t blockSize;                                               // Block size for checksums
    size_t checksumSize;                                            // Checksum size
    size_t blockCurrent;                                            // Size of current block
    IoFilter *checksum;                                             // Checksum of current block
    List *list;                                                     // List of checksums
} BlockChecksum;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_CHECKSUM_TYPE                                                                                           \
    BlockChecksum *
#define FUNCTION_LOG_BLOCK_CHECKSUM_FORMAT(value, buffer, bufferSize)                                                              \
    objNameToLog(value, "BlockChecksum", buffer, bufferSize)

/***********************************************************************************************************************************
Generate block checksum list
***********************************************************************************************************************************/
static void
blockChecksumProcess(THIS_VOID, const Buffer *const input)
{
    THIS(BlockChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_CHECKSUM, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    size_t inputOffset = 0;

    // Loop until input is consumed
    while (inputOffset != bufUsed(input))
    {
        // Create checksum object if needed
        if (this->checksum == NULL)
        {
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->checksum = xxHashNew(this->checksumSize);
                this->blockCurrent = 0;
            }
            MEM_CONTEXT_OBJ_END();
        }

        // Calculate how much data to checksum and perform checksum
        const size_t blockRemains = this->blockSize - this->blockCurrent;
        const size_t inputRemains = bufUsed(input) - inputOffset;
        const size_t blockChecksumSize = blockRemains < inputRemains ? blockRemains : inputRemains;

        ioFilterProcessIn(this->checksum, BUF(bufPtrConst(input) + inputOffset, blockChecksumSize));

        // Update amount of data checksummed
        inputOffset += blockChecksumSize;
        this->blockCurrent += blockChecksumSize;

        // If the block size has been reached then output the checksum
        if (this->blockCurrent == this->blockSize)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                lstAdd(this->list, bufPtrConst(pckReadBinP(pckReadNew(ioFilterResult(this->checksum)))));

                ioFilterFree(this->checksum);
                this->checksum = NULL;
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get a binary representation of the checksum list
***********************************************************************************************************************************/
static Pack *
blockChecksumResult(THIS_VOID)
{
    THIS(BlockChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_CHECKSUM, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        // If there is a remainder in the checksum
        if (this->checksum)
            lstAdd(this->list, bufPtrConst(pckReadBinP(pckReadNew(ioFilterResult(this->checksum)))));

        pckWriteBinP(packWrite, BUF(lstGet(this->list, 0), lstSize(this->list) * this->checksumSize));
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
blockChecksumNew(const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(SIZE, checksumSize);
    FUNCTION_LOG_END();

    ASSERT(blockSize != 0);
    ASSERT(checksumSize != 0);

    // Allocate memory to hold process state
    OBJ_NEW_BEGIN(BlockChecksum, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (BlockChecksum)
        {
            .blockSize = blockSize,
            .checksumSize = checksumSize,
            .list = lstNewP(checksumSize),
        };
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU64P(packWrite, blockSize);
        pckWriteU64P(packWrite, checksumSize);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(BLOCK_CHECKSUM_FILTER_TYPE, this, paramList, .in = blockChecksumProcess, .result = blockChecksumResult));
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
blockChecksumNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const size_t blockSize = (size_t)pckReadU64P(paramListPack);
        const size_t checksumSize = (size_t)pckReadU64P(paramListPack);

        result = ioFilterMove(blockChecksumNew(blockSize, checksumSize), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
