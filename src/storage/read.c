/***********************************************************************************************************************************
Storage Read Interface
***********************************************************************************************************************************/
#include "build.auto.h"

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
    uint64_t bytesRead;                                             // Bytes that have been successfully read
    StorageRangeList *rangeList;                                    // Range list (for reading ranges from a file)
    unsigned int rangeIdx;                                          // Current range
    bool rangeDefault;                                              // Is the range default (i.e. entire file)
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_TYPE                                                                                   \
    StorageReadInterface
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(&value, "StorageReadInterface", buffer, bufferSize)

/***********************************************************************************************************************************
Set range offset and limit
***********************************************************************************************************************************/
static void
storageReadRangeSet(StorageRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ, this);
    FUNCTION_TEST_END();

    const StorageRange *const range = storageRangeListGet(this->rangeList, this->rangeIdx);
    this->pub.interface->offset = range->offset;

    if (range->limit != NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this->driver)
        {
            this->pub.interface->limit = varDup(range->limit);
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN_VOID();
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

    FUNCTION_LOG_RETURN(BOOL, this->pub.interface->ioInterface.open(this->driver));
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
        unsigned int try = this->pub.interface->retry ? 3 : 1;

        // While tries remaining
        while (try > 0)
        {
            TRY_BEGIN()
            {
                this->pub.interface->ioInterface.read(this->driver, buffer, block);

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
                    this->pub.interface->ioInterface.close(this->driver);

                    // Ignore partial reads and restart from the last successful read
                    bufUsedSet(buffer, bufUsedBegin);

                    // The file must not be missing on retry. If we got here then the file must have existed originally and it if is
                    // missing now we want a hard error.
                    this->pub.interface->ignoreMissing = false;

                    // Update offset and limit (when present) based on how many bytes have been successfully read
                    const StorageRange *const range = storageRangeListGet(this->rangeList, 0);
                    this->pub.interface->offset = range->offset + this->bytesRead;

                    if (range->limit != NULL)
                    {
                        varFree(this->pub.interface->limit);

                        MEM_CONTEXT_OBJ_BEGIN(this->driver)
                        {
                            this->pub.interface->limit = varNewUInt64(varUInt64(range->limit) - this->bytesRead);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Open file with new offset/limit
                    this->pub.interface->ioInterface.open(this->driver);
                }
                else
                    RETHROW();
            }
            TRY_END();

            try--;
        }
    }
    MEM_CONTEXT_TEMP_END();

    // If read is complete and there are more ranges then reopen the file with the next range
    if (this->pub.interface->ioInterface.eof(this->driver) && this->rangeIdx < storageRangeListSize(this->rangeList) - 1)
    {
        // Close the file
        this->pub.interface->ioInterface.close(this->driver);

        // Set new range
        this->rangeIdx++;
        storageReadRangeSet(this);

        // Open file with new offset/limit
        this->pub.interface->ioInterface.open(this->driver);
    }

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

    this->pub.interface->ioInterface.close(this->driver);

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

    FUNCTION_TEST_RETURN(BOOL, this->pub.interface->ioInterface.eof(this->driver));
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

    FUNCTION_TEST_RETURN(INT, this->pub.interface->ioInterface.fd(this->driver));
}

/**********************************************************************************************************************************/
FN_EXTERN const StorageRangeList *
storageReadRangeList(const StorageRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(STORAGE_RANGE_LIST, this->rangeDefault ? NULL : this->rangeList);
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
storageReadNew(void *const driver, StorageReadInterface *const interface, const StorageRangeList *const rangeList)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_READ_INTERFACE, interface);
        FUNCTION_LOG_PARAM(STORAGE_RANGE_LIST, rangeList);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->ioInterface.eof != NULL);
    ASSERT(interface->ioInterface.close != NULL);
    ASSERT(interface->ioInterface.open != NULL);
    ASSERT(interface->ioInterface.read != NULL);

    // Remove fd method if it does not exist in the driver
    IoReadInterface storageIoReadInterfaceCopy = storageIoReadInterface;

    if (interface->ioInterface.fd == NULL)
        storageIoReadInterfaceCopy.fd = NULL;

    OBJ_NEW_BEGIN(StorageRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageRead)
        {
            .driver = objMove(driver, memContextCurrent()),
            .pub =
            {
                .interface = interface,
                .io = ioReadNew(this, storageIoReadInterfaceCopy),
                .ignoreMissing = interface->ignoreMissing,
            },
        };

        // If range is provided then duplicate it
        if (rangeList != NULL)
        {
            ASSERT(!storageRangeListEmpty(rangeList));

            this->rangeList = storageRangeListDup(rangeList);
            storageReadRangeSet(this);
        }
        // Else create a default range
        else
        {
            this->rangeList = storageRangeListNewOne(0, NULL);
            this->rangeDefault = true;
        }
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
