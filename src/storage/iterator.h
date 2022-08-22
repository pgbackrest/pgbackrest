/***********************************************************************************************************************************
Storage Iterator
***********************************************************************************************************************************/
#ifndef STORAGE_ITERATOR_H
#define STORAGE_ITERATOR_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageIterator StorageIterator;

#include "common/type/string.h"
#include "storage/info.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageIterator *storageItrNew(
    void *driver, const String *path, StorageInfoLevel level, bool errorOnMissing, bool nullOnMissing, bool recurse,
    SortOrder sortOrder, const String *expression);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is there more info to be retrieved from the iterator?
bool storageItrMore(StorageIterator *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline StorageIterator *
storageItrMove(StorageIterator *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Get next info. An error will be thrown if there is no more data so use storageItrMore() to check. Note that StorageInfo pointer
// members (e.g. name) will be undefined after the next call to storageItrMore().
StorageInfo storageItrNext(StorageIterator *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
storageItrFree(StorageIterator *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageItrToLog(const StorageIterator *this);

#define FUNCTION_LOG_STORAGE_ITERATOR_TYPE                                                                                         \
    StorageIterator *
#define FUNCTION_LOG_STORAGE_ITERATOR_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageItrToLog, buffer, bufferSize)

#endif
