/***********************************************************************************************************************************
Storage Read Interface
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageRead
{
    StorageReadPub pub;                                             // Publicly accessible variables
    const Storage *storage;                                         // Storage
    void *driver;                                                   // Driver
    uint64_t bytesRead;                                             // Bytes that have been successfully read
    bool retry;                                                     // Are read retries allowed?
    bool compressible;                                              // Is the read compressible?
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_TYPE                                                                                   \
    StorageReadInterface
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(&value, "StorageReadInterface", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadOpen(THIS_VOID)
{
    THIS(StorageRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    // Open if not versioned or if versionId is not null
    if (!this->pub.version || this->pub.versionId != NULL)
        result = storageReadDriverInterface(this->driver)->open(this->driver, this->pub.ignoreMissing);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file and retry when there is a read failure
***********************************************************************************************************************************/
static size_t
storageRead(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    const size_t bufUsedBegin = bufUsed(buffer);
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Use a specific number of tries here instead of a timeout because the underlying operations already have timeouts and
        // failures will generally happen after some delay, so it is not clear what timeout would be appropriate here.
        unsigned int try = this->retry ? 3 : 1;

        // While tries remaining
        while (try > 0)
        {
            TRY_BEGIN()
            {
                storageReadDriverInterface(this->driver)->read(this->driver, buffer, block);

                // Account for bytes that have been read
                result += bufUsed(buffer) - bufUsedBegin;
                this->bytesRead += bufUsed(buffer) - bufUsedBegin;

                // Set try to 1 to exit the loop
                try = 1;
            }
            CATCH_ANY()
            {
                // If there is another try remaining then close the file and reopen it to the new position, taking into account any
                // bytes that have already been read. Do not retry when the file is missing.
                if (try > 1 && !errorInstanceOf(&FileMissingError))
                {
                    // Close the file
                    storageReadDriverInterface(this->driver)->close(this->driver);

                    // Ignore partial reads and restart from the last successful read
                    bufUsedSet(buffer, bufUsedBegin);

                    // Driver with new offset/limit
                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        objFree(this->driver);

                        this->driver = storageInterfaceNewReadP(
                            storageDriver(this->storage), this->pub.name, .compressible = this->compressible,
                            .offset = this->pub.offset + this->bytesRead,
                            .limit = this->pub.limit != NULL ? varNewUInt64(varUInt64(this->pub.limit) - this->bytesRead) : NULL,
                            .versionId = this->pub.versionId);
                        storageReadDriverInterface(this->driver)->open(this->driver, false);
                    }
                    MEM_CONTEXT_OBJ_END();
                }
                else
                    RETHROW();
            }
            TRY_END();

            try--;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageReadClose(THIS_VOID)
{
    THIS(StorageRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageReadDriverInterface(this->driver)->close(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadEof(THIS_VOID)
{
    THIS(StorageRead);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, storageReadDriverInterface(this->driver)->eof(this->driver));
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
storageReadFd(const THIS_VOID)
{
    THIS(const StorageRead);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, storageReadDriverInterface(this->driver)->fd(this->driver));
}

/**********************************************************************************************************************************/
static const IoReadInterface storageIoReadInterface =
{
    .open = storageReadOpen,
    .read = storageRead,
    .close = storageReadClose,
    .eof = storageReadEof,
    .fd = storageReadFd,
};

FN_EXTERN StorageRead *
storageReadNew(
    const Storage *const storage, const String *const name, const bool ignoreMissing, const bool compressible,
    const uint64_t offset, const Variant *const limit, const bool version, const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, compressible);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(BOOL, version);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageRead)
        {
            .storage = storage,
            .driver = storageInterfaceNewReadP(
                storageDriver(storage), name, .compressible = compressible, .offset = offset, .limit = limit,
                .versionId = versionId),
            .retry = storageFeature(storage, storageFeatureReadRetry),
            .compressible = compressible,
            .pub =
            {
                .type = storageType(storage),
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .offset = offset,
                .limit = varDup(limit),
                .version = version,
                .versionId = strDup(versionId),
            },
        };

        ASSERT(storageReadDriverInterface(this->driver)->eof != NULL);
        ASSERT(storageReadDriverInterface(this->driver)->close != NULL);
        ASSERT(storageReadDriverInterface(this->driver)->open != NULL);
        ASSERT(storageReadDriverInterface(this->driver)->read != NULL);

        // Remove fd method if it does not exist in the driver
        IoReadInterface storageIoReadInterfaceCopy = storageIoReadInterface;

        if (storageReadDriverInterface(this->driver)->fd == NULL)
            storageIoReadInterfaceCopy.fd = NULL;

        this->pub.io = ioReadNew(this, storageIoReadInterfaceCopy);

        // Set filter group when interface function exists
        if (storageReadDriverInterface(this->driver)->filterGroup != NULL)
            storageReadDriverInterface(this->driver)->filterGroup(this->driver, ioReadFilterGroup(storageReadIo(this)));
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageReadToLog(const StorageRead *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(storageReadType(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcFmt(
        debugLog, ", name: %s, ignoreMissing: %s}", strZ(storageReadName(this)), cvtBoolToConstZ(storageReadIgnoreMissing(this)));
}
