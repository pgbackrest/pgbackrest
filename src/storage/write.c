/***********************************************************************************************************************************
Storage Write Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageWrite
{
    StorageWritePub pub;                                            // Publicly accessible variables
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_TYPE                                                                                  \
    StorageWriteInterface
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                     \
    objNameToLog(&value, "StorageWriteInterface", buffer, bufferSize)

/***********************************************************************************************************************************
This object expects its context to be created in advance. This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
FN_EXTERN StorageWrite *
storageWriteNew(void *const driver, const StorageWriteInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    OBJ_NEW_BEGIN(StorageWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageWrite)
        {
            .pub =
            {
                .interface = interface,
                .io = ioWriteNew(driver, interface->ioInterface),
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}

/**********************************************************************************************************************************/
#define STORAGE_CHUNK_SIZE_MAX                                      (size_t)(1024 * 1024 * 1024)
#define STORAGE_CHUNK_INCR                                          (size_t)(8 * 1024 * 1024)

FN_EXTERN size_t
storageWriteChunkSize(
    const size_t chunkSizeDefault, const unsigned int splitDefault, const unsigned int splitMax, const unsigned int chunkIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, chunkSizeDefault);
        FUNCTION_TEST_PARAM(UINT, splitDefault);
        FUNCTION_TEST_PARAM(UINT, splitMax);
        FUNCTION_TEST_PARAM(UINT, chunkIdx);
    FUNCTION_TEST_END();

    ASSERT(chunkSizeDefault > 0);
    ASSERT(STORAGE_CHUNK_SIZE_MAX >= chunkSizeDefault);
    ASSERT(STORAGE_CHUNK_INCR > 0);
    ASSERT(splitMax > splitDefault);

    // If below the default split then return default chunk size
    if (chunkIdx < splitDefault)
    {
        FUNCTION_TEST_RETURN(SIZE, chunkSizeDefault);
    }
    // Else if above max split then return max chunk size
    else if (chunkIdx > splitMax)
        FUNCTION_TEST_RETURN(SIZE, STORAGE_CHUNK_SIZE_MAX);

    // Calculate ascending chunk size
    uint64_t result =
        (uint64_t)(STORAGE_CHUNK_SIZE_MAX - STORAGE_CHUNK_INCR) * (chunkIdx - splitDefault + 1) / (splitMax - splitDefault);

    // If ascending chunk size is less than default then return default
    if (result <= chunkSizeDefault)
    {
        result = chunkSizeDefault;
    }
    // Else if ascending chunk size is not evenly divisible by chunk increment then round up
    else if (result % STORAGE_CHUNK_INCR != 0)
        result += STORAGE_CHUNK_INCR - (result % STORAGE_CHUNK_INCR);

    // Chunk size should never exceed SIZE_MAX (might happen on 32-bit platforms)
    CHECK(AssertError, result <= SIZE_MAX, "chunk size exceeds SIZE_MAX");

    FUNCTION_TEST_RETURN(SIZE, (size_t)result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageWriteChunkBufferResize(const Buffer *const input, Buffer *const chunk, const size_t chunkSizeMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, input);
        FUNCTION_TEST_PARAM(BUFFER, chunk);
        FUNCTION_TEST_PARAM(SIZE, chunkSizeMax);
    FUNCTION_TEST_END();

    ASSERT(input != NULL);
    ASSERT(chunk != NULL);
    ASSERT(chunkSizeMax > 0);

    // Resize chunk buffer if it is less than max chunk size
    size_t chunkSize = bufSize(chunk);

    if (chunkSize < chunkSizeMax)
    {
        // If the input buffer is full there is very likely more data so increase size of the chunk buffer aggressively
        if (bufFull(input))
        {
            // If this is the first write set chunk size equal to double the input buffer size
            if (chunkSize == 0)
            {
                chunkSize = bufSize(input) * 2;
            }
            // Else double chunk size so as not to resize the chunk buffer too many times since prior data must be copied
            else
                chunkSize *= 2;
        }
        // Else no more writes are expected so allocate only what is needed
        else
            chunkSize += bufUsed(input) - bufRemains(chunk);

        // Chunk size cannot be larger than max chunk size
        if (chunkSize > chunkSizeMax)
            chunkSize = chunkSizeMax;

        // Resize the chunk buffer
        bufResize(chunk, chunkSize);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageWriteToLog(const StorageWrite *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(storageWriteType(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcFmt(
        debugLog, ", name: %s, modeFile: %04o, modePath: %04o, createPath: %s, syncFile: %s, syncPath: %s, atomic: %s}",
        strZ(storageWriteName(this)), storageWriteModeFile(this), storageWriteModePath(this),
        cvtBoolToConstZ(storageWriteCreatePath(this)), cvtBoolToConstZ(storageWriteSyncFile(this)),
        cvtBoolToConstZ(storageWriteSyncPath(this)), cvtBoolToConstZ(storageWriteAtomic(this)));
}
