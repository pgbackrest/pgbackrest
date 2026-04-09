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
FN_EXTERN StorageIterator *storageItrNew(
    void *driver, const String *path, StorageInfoLevel level, bool errorOnMissing, bool nullOnMissing, bool recurse,
    SortOrder sortOrder, time_t targetTime, const String *expression);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is there more info to be retrieved from the iterator?
FN_EXTERN bool storageItrMore(StorageIterator *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS StorageIterator *
storageItrMove(StorageIterator *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Get next info. An error will be thrown if there is no more data so use storageItrMore() to check. Note that StorageInfo pointer
// members (e.g. name) will be undefined after the next call to storageItrMore().
FN_EXTERN StorageInfo storageItrNext(StorageIterator *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
storageItrFree(StorageIterator *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void storageItrToLog(const StorageIterator *this, StringStatic *debugLog);

#define FUNCTION_LOG_STORAGE_ITERATOR_TYPE                                                                                         \
    StorageIterator *
#define FUNCTION_LOG_STORAGE_ITERATOR_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_OBJECT_FORMAT(value, storageItrToLog, buffer, bufferSize)

#endif
