/***********************************************************************************************************************************
IO Filter Interface

Two types of filters are implemented using this interface:  In and InOut.

In filters accept input and produce a result, but do not modify the input.  An example is the IoSize filter which counts all bytes
that pass through it.

InOut filters accept input and produce output (and perhaps a result).  Because the input/output buffers may not be the same size the
filter must be prepared to accept the same input again (by implementing IoFilterInputSame) if the output buffer is too small to
accept all processed data.  If the filter holds state even when inputSame is false then it may also implement IoFilterDone to
indicate that the filter should be flushed (by passing NULL inputs) after all input has been processed.  InOut filters should strive
to fill the output buffer as much as possible, i.e., if the output buffer is not full after processing then inputSame should be
false.  An example is the IoBuffer filter which buffers data between unequally sized input/output buffers.

Each filter has a type that allows it to be identified in the filter list.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_FILTER_H
#define COMMON_IO_FILTER_FILTER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFilter IoFilter;

#include "common/type/string.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Function pointer types
***********************************************************************************************************************************/
typedef bool (*IoFilterDone)(const void *data);
typedef bool (*IoFilterInputSame)(const void *data);
typedef void (*IoFilterProcessIn)(void *data, const Buffer *);
typedef void (*IoFilterProcessInOut)(void *data, const Buffer *, Buffer *);
typedef Variant *(*IoFilterResult)(const void *data);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
IoFilter *ioFilterNew(
    const String *type, void *data, IoFilterDone done, IoFilterInputSame input, IoFilterProcessIn processIn,
    IoFilterProcessInOut processInOut, IoFilterResult result);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void ioFilterProcessIn(IoFilter *this, const Buffer *input);
void ioFilterProcessInOut(IoFilter *this, const Buffer *input, Buffer *output);
IoFilter *ioFilterMove(IoFilter *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool ioFilterDone(const IoFilter *this);
bool ioFilterInputSame(const IoFilter *this);
bool ioFilterOutput(const IoFilter *this);
const Variant *ioFilterResult(const IoFilter *this);
const String *ioFilterType(const IoFilter *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_IO_FILTER_TYPE                                                                                              \
    IoFilter *
#define FUNCTION_DEBUG_IO_FILTER_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "IoFilter", buffer, bufferSize)

#endif
