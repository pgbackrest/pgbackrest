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
    void *driver;
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_TYPE                                                                                  \
    StorageWriteInterface
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                     \
    objToLog(&value, "StorageWriteInterface", buffer, bufferSize)

/***********************************************************************************************************************************
This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageWrite *
storageWriteNew(void *driver, const StorageWriteInterface *interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    StorageWrite *this = memNew(sizeof(StorageWrite));

    *this = (StorageWrite)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .interface = interface,
            .io = ioWriteNew(driver, interface->ioInterface),
        },
        .driver = driver,
    };

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}

/**********************************************************************************************************************************/
String *
storageWriteToLog(const StorageWrite *this)
{
    return strNewFmt(
        "{type: %s, name: %s, modeFile: %04o, modePath: %04o, createPath: %s, syncFile: %s, syncPath: %s, atomic: %s}",
        strZ(strIdToStr(storageWriteType(this))), strZ(strToLog(storageWriteName(this))), storageWriteModeFile(this),
        storageWriteModePath(this), cvtBoolToConstZ(storageWriteCreatePath(this)), cvtBoolToConstZ(storageWriteSyncFile(this)),
        cvtBoolToConstZ(storageWriteSyncPath(this)), cvtBoolToConstZ(storageWriteAtomic(this)));
}
