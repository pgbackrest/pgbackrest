/***********************************************************************************************************************************
Remote Storage Driver Internal
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_STORAGE_INTERN_H
#define STORAGE_DRIVER_REMOTE_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverRemote StorageDriverRemote;

#include "storage/driver/remote/storage.h"

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_TYPE                                                                                    \
    StorageDriverRemote *
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_FORMAT(value, buffer, bufferSize)                                                       \
    objToLog(value, "StorageDriverRemote", buffer, bufferSize)

#endif
