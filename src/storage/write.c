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
FN_EXTERN size_t
storageWriteChunkSize(
    const size_t chunkSizeDefault, const size_t chunkSizeMax, const size_t chunkIncr, const unsigned int splitDefault,
    const unsigned int splitMax, const unsigned int chunkIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, chunkSizeDefault);
        FUNCTION_TEST_PARAM(SIZE, chunkSizeMax);
        FUNCTION_TEST_PARAM(SIZE, chunkIncr);
        FUNCTION_TEST_PARAM(UINT, splitDefault);
        FUNCTION_TEST_PARAM(UINT, splitMax);
        FUNCTION_TEST_PARAM(UINT, chunkIdx);
    FUNCTION_TEST_END();

    ASSERT(chunkSizeDefault > 0);
    ASSERT(chunkSizeMax >= chunkSizeDefault);
    ASSERT(chunkIncr > 0);
    ASSERT(splitMax > splitDefault);

    // If below the default split then return default chunk size
    if (chunkIdx < splitDefault)
    {
        FUNCTION_TEST_RETURN(SIZE, chunkSizeDefault);
    }
    // Else if above max split then return max chunk size
    else if (chunkIdx > splitMax)
        FUNCTION_TEST_RETURN(SIZE, chunkSizeMax);

    // Calculate ascending chunk size
    uint64_t result = (chunkSizeMax - chunkIncr) * (chunkIdx - splitDefault + 1) / (splitMax - splitDefault);

    // If ascending chunk size is less than default then return default
    if (result <= chunkSizeDefault + chunkIncr)
    {
        result = chunkSizeDefault + chunkIncr;
    }
    // Else if ascending chunk size is less evenly divisible by chunk increment then round up
    else if (result % chunkIncr != 0)
        result += chunkIncr - (result % chunkIncr);

    // Chunk size should never exceed SIZE_MAX (might happen on 32-bit platforms)
    CHECK(AssertError, result <= SIZE_MAX, "chunk size exceeds SIZE_MAX");

    FUNCTION_TEST_RETURN(SIZE, (size_t)result);
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
