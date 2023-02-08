/***********************************************************************************************************************************
Restore Delta Map
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/deltaMap.h"
#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct DeltaMap
{
    MemContext *memContext;                                         // Mem context of filter

    size_t blockSize;                                               // Block size for checksums
    size_t blockCurrent;                                            // Size of current block
    IoFilter *hash;                                                 // Hash of current block
    List *list;                                                     // List if hashes
} DeltaMap;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_DELTA_MAP_TYPE                                                                                                \
    DeltaMap *
#define FUNCTION_LOG_DELTA_MAP_FORMAT(value, buffer, bufferSize)                                                                   \
    objNameToLog(value, "DeltaMap", buffer, bufferSize)

/***********************************************************************************************************************************
Generate delta map
***********************************************************************************************************************************/
static void
deltaMapProcess(THIS_VOID, const Buffer *const input)
{
    THIS(DeltaMap);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(DELTA_MAP, this);
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
                this->hash = cryptoHashNew(hashTypeSha1);
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
deltaMapResult(THIS_VOID)
{
    THIS(DeltaMap);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(DELTA_MAP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        // If there is a remainder in the hash
        if (this->hash)
            lstAdd(this->list, bufPtrConst(pckReadBinP(pckReadNew(ioFilterResult(this->hash)))));

        pckWriteBinP(packWrite, BUF(lstGet(this->list, 0), lstSize(this->list) * HASH_TYPE_SHA1_SIZE));
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
deltaMapNew(const size_t blockSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
    FUNCTION_LOG_END();

    ASSERT(blockSize != 0);

    // Allocate memory to hold process state
    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(DeltaMap, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        DeltaMap *const driver = OBJ_NAME(OBJ_NEW_ALLOC(), IoFilter::DeltaMap);

        *driver = (DeltaMap)
        {
            .memContext = memContextCurrent(),
            .blockSize = blockSize,
            .list = lstNewP(HASH_TYPE_SHA1_SIZE),
        };

        this = ioFilterNewP(
            DELTA_MAP_FILTER_TYPE, driver, paramList, .in = deltaMapProcess, .result = deltaMapResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
