/***********************************************************************************************************************************
Storage List
***********************************************************************************************************************************/
#ifndef STORAGE_LIST_H
#define STORAGE_LIST_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageList StorageList;

#include "common/type/string.h"
#include "storage/info.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageList *storageLstNew(StorageInfoLevel level);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageListPub
{
    List *list;                                                     // Storage list
    StorageInfoLevel level;                                         // Storage info level
} StorageListPub;

// Empty?
__attribute__((always_inline)) static inline bool
storageLstEmpty(const StorageList *const this)
{
    return lstEmpty(THIS_PUB(StorageList)->list);
}

// Storage info level
__attribute__((always_inline)) static inline StorageInfoLevel
storageLstLevel(const StorageList *const this)
{
    return THIS_PUB(StorageList)->level;
}

// List size
__attribute__((always_inline)) static inline unsigned int
storageLstSize(const StorageList *const this)
{
    return lstSize(THIS_PUB(StorageList)->list);
}

// List size
__attribute__((always_inline)) static inline void
storageLstSort(StorageList *const this, const SortOrder sortOrder)
{
    lstSort(THIS_PUB(StorageList)->list, sortOrder);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Insert info
void storageLstInsert(StorageList *this, unsigned int idx, const StorageInfo *info);

// Add info
__attribute__((always_inline)) static inline void
storageLstAdd(StorageList *const this, const StorageInfo *const info)
{
    storageLstInsert(this, storageLstSize(this), info);
}

// Get info. Note that StorageInfo pointer members (e.g. name) will be undefined after the next call to storageLstGet().
StorageInfo storageLstGet(StorageList *this, unsigned int idx);

// Move to a new parent mem context
__attribute__((always_inline)) static inline StorageList *
storageLstMove(StorageList *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
storageLstFree(StorageList *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageLstToLog(const StorageList *this);

#define FUNCTION_LOG_STORAGE_LIST_TYPE                                                                                             \
    StorageList *
#define FUNCTION_LOG_STORAGE_LIST_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageLstToLog, buffer, bufferSize)

#endif
