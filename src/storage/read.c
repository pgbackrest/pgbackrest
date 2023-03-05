/***********************************************************************************************************************************
Storage Read Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageRead
{
    StorageReadPub pub;                                             // Publicly accessible variables
    void *driver;                                                   // Driver
    uint64_t offsetCurrent;                                         // Current offset into the file
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

    size_t result = 0;

    TRY_BEGIN()
    {
        result = this->pub.interface->ioInterface.read(this->driver, buffer, block);
    }
    CATCH_ANY()
    {
        // ioReadClose(this->io);

        // this->pub.interface.offset = offsetCurrent;

        // if (this->pub.interface.limit != NULL)
        // {
        //     varFree(this->pub.interface.limit);

        //     MEM_CONTEXT_OBJ_BEGIN(this->driver)
        //     {
        //         this->pub.interface.limit = varNewUInt64(
        //     }
        //     MEM_CONTEXT_OBJ_END();
        // }

        // !!! NOT SURE HOW TO HANDLE THE ERROR SINCE THE DRIVER MEM CONTEXT CANNOT BE FREED

        RETHROW();
    }
    TRY_END();

    this->offsetCurrent += result;

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
static const IoReadInterface storageIoReadInterface =
{
    .open = storageReadOpen,
    .read = storageRead,
    .close = storageReadClose,
    .eof = storageReadEof,
    .fd = storageReadFd,
};

FN_EXTERN StorageRead *
storageReadNew(void *driver, StorageReadInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_READ_INTERFACE, interface);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = OBJ_NEW_ALLOC();

        *this = (StorageRead)
        {
            .pub =
            {
                .interface = interface,
                .io = ioReadNew(this, storageIoReadInterface),
                .offset = interface->offset,
                .limit = varDup(interface->limit),
            },
            .driver = objMove(driver, objMemContext(this)),
        };
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
