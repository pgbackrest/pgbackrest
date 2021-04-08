/***********************************************************************************************************************************
IO Filter Interface Internal

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
#ifndef COMMON_IO_FILTER_FILTER_INTERN_H
#define COMMON_IO_FILTER_FILTER_INTERN_H

#include "common/io/filter/filter.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct IoFilterInterface
{
    // Indicates that filter processing is done.  This is used for filters that have additional data to be flushed even after all
    // input has been processed.  Compression and encryption filters will usually need to implement done.  If done is not
    // implemented then it will always return true if all input has been consumed, i.e. if inputSame returns false.
    bool (*done)(const void *driver);

    // Processing function for filters that do not produce output.  Note that result must be implemented in this case (or else what
    // would be the point.
    void (*in)(void *driver, const Buffer *);

    // Processing function for filters that produce output.  InOut filters will typically implement inputSame and may also implement
    // done.
    void (*inOut)(void *driver, const Buffer *, Buffer *);

    // InOut filters must be prepared for an output buffer that is too small to accept all the processed output.  In this case the
    // filter must implement inputSame and set it to true when there is more output to be produced for a given input.  On the next
    // call to inOut the same input will be passed along with a fresh output buffer with space for more processed output.
    bool (*inputSame)(const void *driver);

    // If the filter produces a result then this function must be implemented to return the result.  A result can be anything that
    // is not processed output, e.g. a count of total bytes or a cryptographic hash.
    Variant *(*result)(void *driver);
} IoFilterInterface;

#define ioFilterNewP(type, driver, paramList, ...)                                                                                 \
    ioFilterNew(type, driver, paramList, (IoFilterInterface){__VA_ARGS__})

IoFilter *ioFilterNew(const String *type, void *driver, VariantList *paramList, IoFilterInterface);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Filter input only (a result is expected)
void ioFilterProcessIn(IoFilter *this, const Buffer *input);

// Filter input and produce output
void ioFilterProcessInOut(IoFilter *this, const Buffer *input, Buffer *output);

// Move filter to a new parent mem context
__attribute__((always_inline)) static inline IoFilter *
ioFilterMove(IoFilter *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Is the filter done?
bool ioFilterDone(const IoFilter *this);

// Driver for the filter
void *ioFilterDriver(IoFilter *this);

// Does the filter need the same input again? If the filter cannot get all its output into the output buffer then it may need access
// to the same input again.
bool ioFilterInputSame(const IoFilter *this);

// Interface for the filter
const IoFilterInterface *ioFilterInterface(const IoFilter *this);

// Does filter produce output? All In filters produce output.
bool ioFilterOutput(const IoFilter *this);

// List of filter parameters
const VariantList *ioFilterParamList(const IoFilter *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_FILTER_INTERFACE_TYPE                                                                                      \
    IoFilterInterface *
#define FUNCTION_LOG_IO_FILTER_INTERFACE_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(&value, "IoFilterInterface", buffer, bufferSize)

#endif
