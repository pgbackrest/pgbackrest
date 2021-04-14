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
    void *driver;
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_TYPE                                                                                   \
    StorageReadInterface
#define FUNCTION_LOG_STORAGE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                      \
    objToLog(&value, "StorageReadInterface", buffer, bufferSize)

/**********************************************************************************************************************************/
StorageRead *
storageReadNew(void *driver, const StorageReadInterface *interface)
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
            .io = ioReadNew(driver, interface->ioInterface),
        },
        .driver = driver,
    };

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}

/**********************************************************************************************************************************/
String *
storageReadToLog(const StorageRead *this)
{
    return strNewFmt(
        "{type: %s, name: %s, ignoreMissing: %s}", strZ(storageReadType(this)), strZ(strToLog(storageReadName(this))),
        cvtBoolToConstZ(storageReadIgnoreMissing(this)));
}
