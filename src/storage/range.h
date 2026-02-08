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
// New range list
FN_INLINE_ALWAYS StorageRangeList *
storageRangeListNew(void)
{
    return (StorageRangeList *)OBJ_NAME(lstNewP(sizeof(StorageRange)), StorageRangeList::List);
}

// Duplicate range list
FN_EXTERN StorageRangeList *storageRangeListDup(const StorageRangeList *this);

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
Macros for constant range list

Note that lists created in this way are declared as const so can't be modified or freed by the storageRangeList*() methods. Casting
to StorageRangeList * will result in a segfault due to modifying read-only memory.

By convention all List constant identifiers are appended with _STGRNGLST.
***********************************************************************************************************************************/
// Create a range list constant inline
#define STGRNGLSTDEF(offsetParam, limitParam)                                                                                      \
    ((const StorageRangeList *)LSTDEF(((StorageRange[1]){{.offset = offsetParam, .limit = limitParam}})))

// Used to define range list constants that will be externed using STGRNGLST_DECLARE(). Must be used in a .c file.
#define STGRNGLST_EXTERN(name, offset, limit)                                                                                      \
    VR_EXTERN_DEFINE const StorageRangeList *const name = STGRNGLSTDEF(offset, limit)

// Used to declare externed range list constants defined with STGRNGLST_EXTERN(). Must be used in a .h file.
#define STGRNGLST_DECLARE(name)                                                                                                    \
    VR_EXTERN_DECLARE const StorageRangeList *const name

/***********************************************************************************************************************************
Constant range lists that are generally useful
***********************************************************************************************************************************/
STGRNGLST_DECLARE(DEFAULT_STGRNGLST);

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
