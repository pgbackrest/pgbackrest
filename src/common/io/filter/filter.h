/***********************************************************************************************************************************
IO Filter Interface

Filters can modify an i/o stream (e.g. GzCompress, GzDecompress), generate a result (e.g. IoSize, CryptoHash), or even do both.

A filter is created using a constructor implemented by each filter (e.g. ioBufferNew). Filter processing is managed by IoFilterGroup
so the only user facing functions are ioFilterResult() and ioFilterType().

Information on implementing a filter is in filter.internal.h.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_FILTER_H
#define COMMON_IO_FILTER_FILTER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFilter IoFilter;

#include "common/io/filter/filter.intern.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get filter result
FN_EXTERN Pack *ioFilterResult(const IoFilter *this);

// Identifies the filter and is used when pulling results from the filter group
FN_INLINE_ALWAYS StringId
ioFilterType(const IoFilter *const this)
{
    return THIS_PUB(IoFilter)->type;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioFilterFree(IoFilter *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void ioFilterToLog(const IoFilter *this, StringStatic *debugLog);

#define FUNCTION_LOG_IO_FILTER_TYPE                                                                                                \
    IoFilter *
#define FUNCTION_LOG_IO_FILTER_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_OBJECT_FORMAT(value, ioFilterToLog, buffer, bufferSize)

#endif
