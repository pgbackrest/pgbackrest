/***********************************************************************************************************************************
Storage Iterator
***********************************************************************************************************************************/
#ifndef STORAGE_ITERATOR_H
#define STORAGE_ITERATOR_H

/***********************************************************************************************************************************
StorageIterator object
***********************************************************************************************************************************/
typedef struct StorageIterator StorageIterator;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageIterator *storageItrNew(
    void *driver, const String *path, StorageInfoLevel level, bool errorOnMissing, bool nullOnMissing, bool recurse,
    SortOrder sortOrder, const String *expression);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline StorageIterator *
storageItrMove(StorageIterator *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

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
