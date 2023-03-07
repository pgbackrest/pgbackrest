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
    void *driver;                                                   // Driver
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_TYPE                                                                                  \
    StorageWriteInterface
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                     \
    objNameToLog(&value, "StorageWriteInterface", buffer, bufferSize)

/***********************************************************************************************************************************
This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
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

    StorageWrite *this = NULL;

    OBJ_NEW_BEGIN(StorageWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = OBJ_NEW_ALLOC();

        *this = (StorageWrite)
        {
            .pub =
            {
                .interface = interface,
                .io = ioWriteNew(driver, interface->ioInterface),
            },
            .driver = objMove(driver, objMemContext(this)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
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
