/***********************************************************************************************************************************
IO Filter Interface

Filters can modify an i/o stream (e.g. GzipCompress, GzipDecompress), generate a result (e.g. IoSize, CryptoHash), or even do both.

A filter is created using a constructor implemented by each filter (e.g. ioBufferNew).  Filter processing is managed by
IoFilterGroup so the only user facing functions are ioFilterResult() and ioFilterType().

Information on implementing a filter is in filter.internal.h.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_FILTER_H
#define COMMON_IO_FILTER_FILTER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define IO_FILTER_TYPE                                              IoFilter
#define IO_FILTER_PREFIX                                            ioFilter

typedef struct IoFilter IoFilter;

#include "common/type/string.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
Variant *ioFilterResult(const IoFilter *this);
const String *ioFilterType(const IoFilter *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void ioFilterFree(IoFilter *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_FILTER_TYPE                                                                                                \
    IoFilter *
#define FUNCTION_LOG_IO_FILTER_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "IoFilter", buffer, bufferSize)

#endif
