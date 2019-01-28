/***********************************************************************************************************************************
IO Filter Group
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/filter.intern.h"
#include "common/io/filter/group.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Filter and buffer structure

Contains the filter object and inout/output buffers.
***********************************************************************************************************************************/
typedef struct IoFilterData
{
    const Buffer *input;                                            // Input buffer for filter
    Buffer *inputLocal;                                             // Non-null if a locally created buffer that can be cleared
    IoFilter *filter;                                               // Filter to apply
    Buffer *output;                                                 // Output buffer for filter
} IoFilterData;

// Macros for logging
#define FUNCTION_LOG_IO_FILTER_DATA_TYPE                                                                                           \
    IoFilterData *
#define FUNCTION_LOG_IO_FILTER_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "IoFilterData", buffer, bufferSize)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilterGroup
{
    MemContext *memContext;                                         // Mem context
    List *filterList;                                               // List of filters to apply
    unsigned int firstOutputFilter;                                 // Index of the first output filter
    KeyValue *filterResult;                                         // Filter results (if any)
    bool inputSame;                                                 // Same input required again?
    bool done;                                                      // Is processing done?

#ifdef DEBUG
    bool opened;                                                    // Has the filter set been opened?
    bool flushing;                                                  // Is output being flushed?
    bool closed;                                                    // Has the filter set been closed?
#endif
};

/***********************************************************************************************************************************
New Object
***********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilterGroup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoFilterGroup")
    {
        this = memNew(sizeof(IoFilterGroup));
        this->memContext = memContextCurrent();
        this->done = true;
        this->filterList = lstNew(sizeof(IoFilterData));
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Add a filter
***********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(IO_FILTER, filter);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->opened && !this->closed);
    ASSERT(filter != NULL);

    // Move the filter to this object's mem context
    ioFilterMove(filter, this->memContext);

    // Add the filter
    IoFilterData filterData = {.filter = filter};
    lstAdd(this->filterList, &filterData);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Get a filter
***********************************************************************************************************************************/
static IoFilterData *
ioFilterGroupGet(IoFilterGroup *this, unsigned int filterIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_TEST_PARAM(UINT, filterIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_FILTER_DATA, (IoFilterData *)lstGet(this->filterList, filterIdx));
}

/***********************************************************************************************************************************
Open filter group

Setup the filter group and allocate any required buffers.
***********************************************************************************************************************************/
void
ioFilterGroupOpen(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // If the last filter is not an output filter then add a filter to buffer/copy data.  Input filters won't copy to an output
        // buffer so we need some way to get the data to the output buffer.
        if (lstSize(this->filterList) == 0 || !ioFilterOutput((ioFilterGroupGet(this, lstSize(this->filterList) - 1))->filter))
            ioFilterGroupAdd(this, ioBufferFilter(ioBufferNew()));

        // Create filter input/output buffers.  Input filters do not get an output buffer since they don't produce output.
        Buffer *lastOutputBuffer = NULL;

        for (unsigned int filterIdx = 0; filterIdx < lstSize(this->filterList); filterIdx++)
        {
            IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);

            // Assign the last output buffer to the input.  At first there won't be an input filter because it will be passed into
            // the process function as an input.
            if (lastOutputBuffer != NULL)
            {
                filterData->input = lastOutputBuffer;
                filterData->inputLocal = lastOutputBuffer;
            }

            // Is this an output filter?
            if (ioFilterOutput(filterData->filter))
            {
                // If this is the first output buffer found, store the index so it can be easily found during processing
                if (lastOutputBuffer == NULL)
                    this->firstOutputFilter = filterIdx;

                // If this is not the last output filter then create a new output buffer for it.  The output buffer for the last
                // filter will be provided to the process function.
                if (filterIdx < lstSize(this->filterList) - 1)
                {
                    lastOutputBuffer = bufNew(ioBufferSize());
                    filterData->output = lastOutputBuffer;
                }
            }
        }
    }
    MEM_CONTEXT_END();

    // Filter group is open
#ifdef DEBUG
    this->opened = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Process filters
***********************************************************************************************************************************/
void
ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input, Buffer *output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);
    ASSERT(!this->flushing || input == NULL);
    ASSERT(output != NULL);
    ASSERT(bufRemains(output) > 0);

    // Once input goes to NULL then flushing has started
#ifdef DEBUG
    if (input == NULL)
        this->flushing = true;
#endif

    // Assign the input buffer up to the first output filter.  After this point the input buffers were locally created during open.
    if (!this->inputSame)
    {
        for (unsigned int filterIdx = 0; filterIdx <= this->firstOutputFilter; filterIdx++)
            (ioFilterGroupGet(this, filterIdx))->input = input;
    }

    // Assign the output buffer
    (ioFilterGroupGet(this, lstSize(this->filterList) - 1))->output = output;

    //
    do
    {
        // Start from the first filter by default
        unsigned int filterIdx = 0;

        // Search from the end of the list for a filter that needs the same input.  This indicates that the filter was not able to
        // empty the input buffer on the last call.  Maybe it won't this time either -- we can but try.
        if (this->inputSame)
        {
            this->inputSame = false;
            filterIdx = lstSize(this->filterList);

            do
            {
                filterIdx--;

                if (ioFilterInputSame((ioFilterGroupGet(this, filterIdx))->filter))
                {
                    this->inputSame = true;
                    break;
                }
            }
            while (filterIdx != this->firstOutputFilter);

            // If no filter is found that needs the same input that means we are done with the current input.  So end the loop and
            // get some more input.
            if (!this->inputSame)
                break;
        }

        // Process forward from the filter that has input to process.  This may be a filter that needs the same input or it may be
        // new input for the first filter.
        for (; filterIdx < lstSize(this->filterList); filterIdx++)
        {
            IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);

            // If the filter produces output
            if (ioFilterOutput(filterData->filter))
            {
                // Keep processing while the filter is not done or there is input
                if (!ioFilterDone(filterData->filter) || filterData->input != NULL)
                {
                    // If we are flushing and the prior filter is done and is not producing any more output then this filter should
                    // be flushing as well.  Set filterData->input = NULL so it knows there is no more input coming.
                    //
                    // If the filter is already done then there is no need to set input to NULL because it has already flushed and
                    // the filter shouldn't need to hand NULL input if it doesn't need it to know when to flush.
                    //
                    // Checking filterIdx - 1 is safe because the first filter's filterData->input is always set to NULL when input
                    // is NULL.
                    if (input == NULL && filterData->input != NULL && !ioFilterDone(filterData->filter) &&
                        ioFilterDone(ioFilterGroupGet(this, filterIdx - 1)->filter) && bufUsed(filterData->input) == 0)
                    {
                        filterData->input = NULL;
                    }

                    ioFilterProcessInOut(filterData->filter, filterData->input, filterData->output);

                    // If inputSame is set then the output buffer for this filter is full and it will need to be pre-processed with
                    // the same input once the output buffer is cleared.
                    if (ioFilterInputSame(filterData->filter))
                        this->inputSame = true;

                    // Else clear the buffer if it was locally allocated.  If this is an input buffer that was passed in then the
                    // caller is responsible for clearing it.
                    else if (filterData->inputLocal != NULL)
                        bufUsedZero(filterData->inputLocal);
                }
            }
            // Else the filter does not produce output.  No need to flush these filters because they don't buffer data.
            else if (filterData->input != NULL)
                ioFilterProcessIn(filterData->filter, filterData->input);
        }
    }
    while (!bufFull(output) && this->inputSame);

    // Scan the filter list to determine if inputSame is set or done is not set for any filter.  We can't trust this->inputSame
    // when it is true without going through the loop above again.  We need to scan to set this->done anyway so set this->inputSame
    // in the same loop.
    this->done = true;
    this->inputSame = false;

    for (unsigned int filterIdx = 0; filterIdx < lstSize(this->filterList); filterIdx++)
    {
        IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);

        // When inputSame then this->done = false and we can exit the loop immediately
        if (ioFilterInputSame(filterData->filter))
        {
            this->done = false;
            this->inputSame = true;
            break;
        }

        // Set this->done = false if any filter is not done
        if (!ioFilterDone(filterData->filter))
            this->done = false;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close filter group and gather results
***********************************************************************************************************************************/
void
ioFilterGroupClose(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    for (unsigned int filterIdx = 0; filterIdx < lstSize(this->filterList); filterIdx++)
    {
        IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);
        const Variant *filterResult = ioFilterResult(filterData->filter);

        if (this->filterResult == NULL)
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->filterResult = kvNew();
            }
            MEM_CONTEXT_END();
        }

        MEM_CONTEXT_TEMP_BEGIN()
        {
            kvAdd(this->filterResult, varNewStr(ioFilterType(filterData->filter)), filterResult);
        }
        MEM_CONTEXT_TEMP_END();
    }

    // Filter group is open
#ifdef DEBUG
    this->closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is the filter group done processing?
***********************************************************************************************************************************/
bool
ioFilterGroupDone(const IoFilterGroup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_TEST_RETURN(BOOL, this->done);
}

/***********************************************************************************************************************************
Should the same input be passed again?

A buffer of input can produce multiple buffers of output, e.g. when a file containing all zeroes is being decompressed.
***********************************************************************************************************************************/
bool
ioFilterGroupInputSame(const IoFilterGroup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/***********************************************************************************************************************************
Get filter results
***********************************************************************************************************************************/
const Variant *
ioFilterGroupResult(const IoFilterGroup *this, const String *filterType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(STRING, filterType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && this->closed);
    ASSERT(filterType != NULL);

    const Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = kvGet(this->filterResult, varNewStr(filterType));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_CONST(VARIANT, result);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
ioFilterGroupToLog(const IoFilterGroup *this)
{
    return strNewFmt("{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

/***********************************************************************************************************************************
Free the filter group
***********************************************************************************************************************************/
void
ioFilterGroupFree(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
