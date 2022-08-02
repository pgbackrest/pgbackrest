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
    IoRead *io;                                                     // Driver read interface
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_TYPE                                                                                   \
    StorageReadInterface
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                      \
    objToLog(&value, "StorageReadInterface", buffer, bufferSize)

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

    FUNCTION_LOG_RETURN(BOOL, ioReadOpen(this->io));
}

/***********************************************************************************************************************************
Read from a file and retry when there is a read failure
***********************************************************************************************************************************/
static size_t
storageRead(THIS_VOID, Buffer *buffer, bool block)
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
        result = ioRead(this->io, buffer);
    }
    CATCH_ANY()
    {
        // !!! NOT SURE HOW TO HANDLE THE ERROR SINCE THE DRIVER MEM CONTEXT CANNOT BE FREED

        RETHROW();
    }
    TRY_END();

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

    ioReadClose(this->io);

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

    FUNCTION_TEST_RETURN(BOOL, ioReadEof(this->io));
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

    FUNCTION_TEST_RETURN(INT, ioReadFd(this->io));
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

StorageRead *
storageReadNew(void *const driver, const StorageReadInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_READ_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    StorageRead *this = NULL;

    this = memNew(sizeof(StorageRead));

    *this = (StorageRead)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .interface = interface,
            .io = ioReadNew(this, storageIoReadInterface),
        },
        .io = ioReadNew(driver, interface->ioInterface),
        .driver = driver,
    };

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}

/**********************************************************************************************************************************/
String *
storageReadToLog(const StorageRead *this)
{
    return strNewFmt(
        "{type: %s, name: %s, ignoreMissing: %s}", strZ(strIdToStr(storageReadType(this))), strZ(strToLog(storageReadName(this))),
        cvtBoolToConstZ(storageReadIgnoreMissing(this)));
}
