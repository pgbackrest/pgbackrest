/***********************************************************************************************************************************
Restore Delta Map
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/blockHash.h"
#include "common/crypto/common.h"
#include "common/crypto/xxhash.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockHash
{
    MemContext *memContext;                                         // Mem context of filter

    size_t blockSize;                                               // Block size for checksums
    size_t checksumSize;                                            // Checksum size
    size_t blockCurrent;                                            // Size of current block
    IoFilter *hash;                                                 // Hash of current block
    List *list;                                                     // List of hashes
} BlockHash;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_HASH_TYPE                                                                                               \
    BlockHash *
#define FUNCTION_LOG_BLOCK_HASH_FORMAT(value, buffer, bufferSize)                                                                  \
    objNameToLog(value, "BlockHash", buffer, bufferSize)

/***********************************************************************************************************************************
Generate block hash list
***********************************************************************************************************************************/
static void
blockHashProcess(THIS_VOID, const Buffer *const input)
{
    THIS(BlockHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_HASH, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    size_t inputOffset = 0;

    // Loop until input is consumed
    while (inputOffset != bufUsed(input))
    {
        // Create hash object if needed
        if (this->hash == NULL)
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->hash = xxHashNew(this->checksumSize);
                this->blockCurrent = 0;
            }
            MEM_CONTEXT_END();
        }

        // Calculate how much data to hash and perform hash
        const size_t blockRemains = this->blockSize - this->blockCurrent;
        const size_t inputRemains = bufUsed(input) - inputOffset;
        const size_t blockHash = blockRemains < inputRemains ? blockRemains : inputRemains;

        ioFilterProcessIn(this->hash, BUF(bufPtrConst(input) + inputOffset, blockHash));

        // Update amount of data hashed
        inputOffset += blockHash;
        this->blockCurrent += blockHash;

        // If the block size has been reached then output the hash
        if (this->blockCurrent == this->blockSize)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                lstAdd(this->list, bufPtrConst(pckReadBinP(pckReadNew(ioFilterResult(this->hash)))));

                ioFilterFree(this->hash);
                this->hash = NULL;
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get a binary representation of the hash list
***********************************************************************************************************************************/
static Pack *
blockHashResult(THIS_VOID)
{
    THIS(BlockHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        // If there is a remainder in the hash
        if (this->hash)
            lstAdd(this->list, bufPtrConst(pckReadBinP(pckReadNew(ioFilterResult(this->hash)))));

        pckWriteBinP(packWrite, BUF(lstGet(this->list, 0), lstSize(this->list) * this->checksumSize));
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
blockHashNew(const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(SIZE, checksumSize);
    FUNCTION_LOG_END();

    ASSERT(blockSize != 0);

    // Allocate memory to hold process state
    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(BlockHash, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        BlockHash *const driver = OBJ_NAME(OBJ_NEW_ALLOC(), IoFilter::BlockHash);

        *driver = (BlockHash)
        {
            .memContext = memContextCurrent(),
            .blockSize = blockSize,
            .checksumSize = checksumSize,
            .list = lstNewP(checksumSize),
        };

        this = ioFilterNewP(BLOCK_HASH_FILTER_TYPE, driver, NULL, .in = blockHashProcess, .result = blockHashResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
