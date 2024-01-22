/***********************************************************************************************************************************
xxHash Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/crypto/xxhash.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Include local xxHash code
***********************************************************************************************************************************/
#define XXH_INLINE_ALL

#include "common/crypto/xxhash.vendor.c.inc"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct XxHash
{
    size_t size;                                                    // Size of hash to return
    XXH3_state_t *state;                                            // xxHash state
} XxHash;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_XX_HASH_TYPE                                                                                              \
    XxHash *
#define FUNCTION_LOG_XX_HASH_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "XxHash", buffer, bufferSize)

/***********************************************************************************************************************************
Free hash context
***********************************************************************************************************************************/
static void
xxHashFreeResource(THIS_VOID)
{
    THIS(XxHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(XX_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    XXH3_freeState(this->state);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a Buffer
***********************************************************************************************************************************/
static void
xxHashProcess(THIS_VOID, const Buffer *const message)
{
    THIS(XxHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(XX_HASH, this);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(message != NULL);

    XXH3_128bits_update(this->state, bufPtrConst(message), bufUsed(message));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get string representation of the hash as a filter result
***********************************************************************************************************************************/
static Pack *
xxHashResult(THIS_VOID)
{
    THIS(XxHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(XX_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        XXH128_canonical_t canonical;
        XXH128_canonicalFromHash(&canonical, XXH3_128bits_digest(this->state));

        pckWriteBinP(packWrite, BUF(canonical.digest, this->size));
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
xxHashNew(const size_t size)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, size);
    FUNCTION_LOG_END();

    ASSERT(size >= 1 && size <= XX_HASH_SIZE_MAX);

    OBJ_NEW_BEGIN(XxHash, .callbackQty = 1)
    {
        *this = (XxHash){.size = size};

        this->state = XXH3_createState();
        XXH3_128bits_reset(this->state);

        // Set free callback to ensure hash context is freed
        memContextCallbackSet(objMemContext(this), xxHashFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, ioFilterNewP(XX_HASH_FILTER_TYPE, this, NULL, .in = xxHashProcess, .result = xxHashResult));
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
xxHashOne(const size_t size, const Buffer *const message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SIZE, size);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(size >= 1 && size <= XX_HASH_SIZE_MAX);
    ASSERT(message != NULL);

    Buffer *const result = bufNew(size);

    XXH128_canonical_t canonical;
    XXH128_canonicalFromHash(&canonical, XXH3_128bits(bufPtrConst(message), bufUsed(message)));

    memcpy(bufPtr(result), canonical.digest, size);
    bufUsedSet(result, size);

    FUNCTION_LOG_RETURN(BUFFER, result);
}
