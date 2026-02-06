/***********************************************************************************************************************************
Storage Range
***********************************************************************************************************************************/
#ifndef STORAGE_RANGE_H
#define STORAGE_RANGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageRangeList StorageRangeList;

#include "common/type/variant.h"

typedef struct StorageRange
{
    uint64_t offset;
    const Variant *limit;
} StorageRange;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_INLINE_ALWAYS StorageRangeList *
storageRangeListNew(void)
{
    return (StorageRangeList *)OBJ_NAME(lstNewP(sizeof(StorageRange)), StorageRangeList::List);
}

FN_EXTERN StorageRangeList *storageRangeListNewOne(uint64_t offset, const Variant *limit);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a range
FN_EXTERN StorageRange *storageRangeListAdd(StorageRangeList *this, uint64_t offset, const Variant *limit);

// Get a range
FN_INLINE_ALWAYS StorageRange *
storageRangeListGet(const StorageRangeList *const this, const unsigned int listIdx)
{
    return (StorageRange *)lstGet((const List *)this, listIdx);
}

// Range list size
FN_INLINE_ALWAYS unsigned int
storageRangeListSize(const StorageRangeList *const this)
{
    return lstSize((const List *)this);
}

// Is the range list empty?
FN_INLINE_ALWAYS unsigned int
storageRangeListEmpty(const StorageRangeList *const this)
{
    return storageRangeListSize(this) == 0;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
storageRangeListFree(StorageRangeList *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_RANGE_TYPE                                                                                            \
    StorageRange *
#define FUNCTION_LOG_STORAGE_RANGE_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(value, "StorageRange", buffer, bufferSize)

#define FUNCTION_LOG_STORAGE_RANGE_LIST_TYPE                                                                                       \
    StorageRangeList *
#define FUNCTION_LOG_STORAGE_RANGE_LIST_FORMAT(value, buffer, bufferSize)                                                          \
    FUNCTION_LOG_OBJECT_FORMAT(value, lstToLog, buffer, bufferSize)

#endif
