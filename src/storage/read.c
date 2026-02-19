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
    void *driver;                                                   // Driver
    IoReadInterface ioInterface;                                    // Driver Io interface
    uint64_t bytesRead;                                             // Bytes that have been successfully read
    bool retry;                                                     // Are read retries allowed?
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_TYPE                                                                                   \
    StorageReadInterface
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(&value, "StorageReadInterface", buffer, bufferSize)

/***********************************************************************************************************************************
Get driver interface
***********************************************************************************************************************************/
FN_INLINE_ALWAYS StorageReadInterface *
storageReadDriverInterface(StorageRead *const this)
{
    return (StorageReadInterface *)this->driver;
}

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

    const bool result = this->ioInterface.open(this->driver);

    // Now that the file is open disable ignore missing. On retry the file must not be missing so we want a hard error.
    storageReadDriverInterface(this)->ignoreMissing = false;

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
                this->ioInterface.read(this->driver, buffer, block);

                // Account for bytes that have been read
                result += bufUsed(buffer) - bufUsedBegin;
                this->bytesRead += bufUsed(buffer) - bufUsedBegin;

                // Set try to 1 to exit the loop
                try = 1;
            }
            CATCH_ANY()
            {
                // If there is another try remaining then close the file and reopen it to the new position, taking into account any
                // bytes that have already been read
                if (try > 1)
                {
                    // Close the file
                    this->ioInterface.close(this->driver);

                    // Ignore partial reads and restart from the last successful read
                    bufUsedSet(buffer, bufUsedBegin);

                    // Update offset and limit (when present) based on how many bytes have been successfully read
                    storageReadDriverInterface(this)->offset = storageReadInterface(this)->offset + this->bytesRead;

                    if (storageReadInterface(this)->limit != NULL)
                    {
                        varFree(storageReadDriverInterface(this)->limit);

                        MEM_CONTEXT_OBJ_BEGIN(this->driver)
                        {
                            storageReadDriverInterface(this)->limit = varNewUInt64(
                                varUInt64(storageReadInterface(this)->limit) - this->bytesRead);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Open file with new offset/limit
                    this->ioInterface.open(this->driver);
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

    this->ioInterface.close(this->driver);

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

    FUNCTION_TEST_RETURN(BOOL, this->ioInterface.eof(this->driver));
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

    FUNCTION_TEST_RETURN(INT, this->ioInterface.fd(this->driver));
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
    void *const driver, const StringId type, const String *const name, const bool ignoreMissing, const uint64_t offset,
    const Variant *const limit, const IoReadInterface *const ioInterface, const StorageReadNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM_P(VOID, ioInterface);
        FUNCTION_LOG_PARAM(BOOL, param.retry);
        FUNCTION_LOG_PARAM(BOOL, param.version);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(driver != NULL);
    ASSERT(type != 0);
    ASSERT(name != NULL);
    ASSERT(ioInterface != NULL);
    ASSERT(ioInterface->eof != NULL);
    ASSERT(ioInterface->close != NULL);
    ASSERT(ioInterface->open != NULL);
    ASSERT(ioInterface->read != NULL);

    // Remove fd method if it does not exist in the driver
    IoReadInterface storageIoReadInterfaceCopy = storageIoReadInterface;

    if (ioInterface->fd == NULL)
        storageIoReadInterfaceCopy.fd = NULL;

    OBJ_NEW_BEGIN(StorageRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageRead)
        {
            .driver = objMove(driver, memContextCurrent()),
            .ioInterface = *ioInterface,
            .retry = param.retry,
            .pub =
            {
                .interface =
                {
                    .name = strDup(name),
                    .ignoreMissing = ignoreMissing,
                    .offset = offset,
                    .limit = varDup(limit),
                    .version = param.version,
                    .versionId = strDup(param.versionId),
                },
                .type = type,
                .io = ioReadNew(this, storageIoReadInterfaceCopy),
            },
        };
    }
    OBJ_NEW_END();

    // Copy the interface into the driver and duplicate variables that must be local to the driver
    MEM_CONTEXT_OBJ_BEGIN(this->driver)
    {
        *(storageReadDriverInterface(this)) = this->pub.interface;
        storageReadDriverInterface(this)->limit = varDup(storageReadInterface(this)->limit);
    }
    MEM_CONTEXT_OBJ_END();

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
