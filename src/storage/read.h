/***********************************************************************************************************************************
Storage Read Interface
***********************************************************************************************************************************/
#ifndef STORAGE_READ_H
#define STORAGE_READ_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageRead StorageRead;

#include "common/io/read.h"
#include "common/type/object.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline StorageRead *
storageReadMove(StorageRead *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadPub
{
    MemContext *memContext;                                         // Mem context
    const StorageReadInterface *interface;                          // File data (name, driver type, etc.)
    IoRead *io;                                                     // Read interface
} StorageReadPub;

// Should a missing file be ignored?
__attribute__((always_inline)) static inline bool
storageReadIgnoreMissing(const StorageRead *this)
{
    return THIS_PUB(StorageRead)->interface->ignoreMissing;
}

// Read interface
__attribute__((always_inline)) static inline IoRead *
storageReadIo(const StorageRead *this)
{
    return THIS_PUB(StorageRead)->io;
}

// Is there a read limit? NULL for no limit.
__attribute__((always_inline)) static inline const Variant *
storageReadLimit(const StorageRead *this)
{
    return THIS_PUB(StorageRead)->interface->limit;
}

// File name
__attribute__((always_inline)) static inline const String *
storageReadName(const StorageRead *this)
{
    return THIS_PUB(StorageRead)->interface->name;
}

// Get file type
__attribute__((always_inline)) static inline const String *
storageReadType(const StorageRead *this)
{
    return THIS_PUB(StorageRead)->interface->type;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
storageReadFree(StorageRead *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageReadToLog(const StorageRead *this);

#define FUNCTION_LOG_STORAGE_READ_TYPE                                                                                             \
    StorageRead *
#define FUNCTION_LOG_STORAGE_READ_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageReadToLog, buffer, bufferSize)

#endif
