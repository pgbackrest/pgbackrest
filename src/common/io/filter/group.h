/***********************************************************************************************************************************
IO Filter Group

Process data through an arbitrary group of filters in the order added by the user using ioFilterGroupAdd(). After processing results
can be gathered using ioFilterGroupResult() for any filters that produce results.

Processing is complex and asymmetric for read/write so should be done via the IoRead and IoWrite objects. General users need only
call ioFilterGroupNew(), ioFilterGroupAdd(), and ioFilterGroupResult().
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_GROUP_H
#define COMMON_IO_FILTER_GROUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFilterGroup IoFilterGroup;

#include "common/io/filter/filter.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *ioFilterGroupNew(void);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoFilterGroupPub
{
    List *filterList;                                               // List of filters to apply
    bool inputSame;                                                 // Same input required again?
    bool done;                                                      // Is processing done?

#ifdef DEBUG
    bool opened;                                                    // Has the filter set been opened?
    bool closed;                                                    // Has the filter set been closed?
#endif
} IoFilterGroupPub;

// Is the filter group done processing?
FN_INLINE_ALWAYS bool
ioFilterGroupDone(const IoFilterGroup *const this)
{
    ASSERT_INLINE(THIS_PUB(IoFilterGroup)->opened && !THIS_PUB(IoFilterGroup)->closed);
    return THIS_PUB(IoFilterGroup)->done;
}

// Should the same input be passed again? A buffer of input can produce multiple buffers of output, e.g. when a file containing all
// zeroes is being decompressed.
FN_INLINE_ALWAYS bool
ioFilterGroupInputSame(const IoFilterGroup *const this)
{
    ASSERT_INLINE(THIS_PUB(IoFilterGroup)->opened && !THIS_PUB(IoFilterGroup)->closed);
    return THIS_PUB(IoFilterGroup)->inputSame;
}

// Get all filters and their parameters so they can be passed to a remote
FN_EXTERN Pack *ioFilterGroupParamAll(const IoFilterGroup *this);

// Get filter results. If the same filter was used more than once then idx can be used to specify which one to get.
typedef struct IoFilterGroupResultParam
{
    VAR_PARAM_HEADER;
    unsigned int idx;
} IoFilterGroupResultParam;

#define ioFilterGroupResultP(this, filterType, ...)                                                                                \
    ioFilterGroupResult(this, filterType, (IoFilterGroupResultParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *ioFilterGroupResult(const IoFilterGroup *this, StringId filterType, IoFilterGroupResultParam param);

#define ioFilterGroupResultPackP(this, filterType, ...)                                                                            \
    ioFilterGroupResultPack(this, filterType, (IoFilterGroupResultParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN const Pack *ioFilterGroupResultPack(const IoFilterGroup *this, StringId filterType, IoFilterGroupResultParam param);

// Get/set all filter results
FN_EXTERN Pack *ioFilterGroupResultAll(const IoFilterGroup *this);
FN_EXTERN void ioFilterGroupResultAllSet(IoFilterGroup *this, const Pack *filterResult);

// Return total number of filters
FN_INLINE_ALWAYS unsigned int
ioFilterGroupSize(const IoFilterGroup *const this)
{
    return lstSize(THIS_PUB(IoFilterGroup)->filterList);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a filter
FN_EXTERN IoFilterGroup *ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter);

// Insert a filter before an index
FN_EXTERN IoFilterGroup *ioFilterGroupInsert(IoFilterGroup *this, unsigned int listIdx, IoFilter *filter);

// Clear filters
FN_EXTERN IoFilterGroup *ioFilterGroupClear(IoFilterGroup *this);

// Open filter group
FN_EXTERN void ioFilterGroupOpen(IoFilterGroup *this);

// Process filters
FN_EXTERN void ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input, Buffer *output);

// Close filter group and gather results
FN_EXTERN void ioFilterGroupClose(IoFilterGroup *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioFilterGroupFree(IoFilterGroup *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void ioFilterGroupToLog(const IoFilterGroup *this, StringStatic *debugLog);

#define FUNCTION_LOG_IO_FILTER_GROUP_TYPE                                                                                          \
    IoFilterGroup *
#define FUNCTION_LOG_IO_FILTER_GROUP_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_OBJECT_FORMAT(value, ioFilterGroupToLog, buffer, bufferSize)

#endif
