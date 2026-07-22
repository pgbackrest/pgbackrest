/***********************************************************************************************************************************
Storage Read Multi Files and Ranges
***********************************************************************************************************************************/
#ifndef STORAGE_READ_MULTI_H
#define STORAGE_READ_MULTI_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadMulti StorageReadMulti;

#include "common/type/object.h"
#include "common/type/param.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
typedef struct StorageReadMultiAddParam
{
    VAR_PARAM_HEADER;

    // Is the file compressible?
    bool compressible;

    // Where to start reading in the file
    const uint64_t offset;

    // Limit bytes to read from the file (must be varTypeUInt64). NULL for no limit.
    const Variant *limit;

    // Target a specific file version (NULL for current version). This is resolved automatically when the storage is time-targeted
    // but may also be set explicitly, e.g. when the version has already been resolved on the client side of a remote.
    const String *versionId;
} StorageReadMultiAddParam;

#include "storage/readMulti.intern.h"

#define storageReadMultiAddP(this, pathExp, ...)                                                                                        \
    storageReadMultiAdd(this, pathExp, (StorageReadMultiAddParam){VAR_PARAM_INIT, __VA_ARGS__})

// Add a file to the read. All files must be added before the io is opened since some drivers, e.g. remote, send the entire request
// list to be processed when the io is opened.
FN_EXTERN void storageReadMultiAdd(StorageReadMulti *this, const String *fileExp, StorageReadMultiAddParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
FN_INLINE_ALWAYS IoRead *
storageReadMultiIo(const StorageReadMulti *const this)
{
    return THIS_PUB(StorageReadMulti)->io;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
storageReadMultiFree(StorageReadMulti *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void storageReadMultiToLog(const StorageReadMulti *this, StringStatic *debugLog);

#define FUNCTION_LOG_STORAGE_READ_MULTI_TYPE                                                                                       \
    StorageReadMulti *
#define FUNCTION_LOG_STORAGE_READ_MULTI_FORMAT(value, buffer, bufferSize)                                                          \
    FUNCTION_LOG_OBJECT_FORMAT(value, storageReadMultiToLog, buffer, bufferSize)

#endif
