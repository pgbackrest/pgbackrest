/***********************************************************************************************************************************
IO Filter Group
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/group.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Filter and buffer structure

Contains the filter object and inout/output buffers.
***********************************************************************************************************************************/
typedef struct IoFilterData
{
    const Buffer **input;                                           // Input buffer for filter
    Buffer *inputLocal;                                             // Non-null if a locally created buffer that can be cleared
    IoFilter *filter;                                               // Filter to apply
    Buffer *output;                                                 // Output buffer for filter
} IoFilterData;

// Macros for logging
#define FUNCTION_LOG_IO_FILTER_DATA_TYPE                                                                                           \
    IoFilterData *
#define FUNCTION_LOG_IO_FILTER_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    objNameToLog(value, "IoFilterData", buffer, bufferSize)

/***********************************************************************************************************************************
Filter results
***********************************************************************************************************************************/
typedef struct IoFilterResult
{
    StringId type;                                                  // Filter type
    Pack *result;                                                   // Filter result
} IoFilterResult;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilterGroup
{
    IoFilterGroupPub pub;                                           // Publicly accessible variables
    const Buffer *input;                                            // Input buffer passed in for processing
    List *filterResult;                                             // Filter results (if any)

#ifdef DEBUG
    bool flushing;                                                  // Is output being flushed?
#endif
};

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
ioFilterGroupNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    OBJ_NEW_BEGIN(IoFilterGroup, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoFilterGroup)
        {
            .pub =
            {
                .done = false,
                .filterList = lstNewP(sizeof(IoFilterData)),
            },

            .filterResult = lstNewP(sizeof(IoFilterResult)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
ioFilterGroupAdd(IoFilterGroup *const this, IoFilter *const filter)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(IO_FILTER, filter);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.opened && !this->pub.closed);
    ASSERT(filter != NULL);

    // Move the filter to this object's mem context
    ioFilterMove(filter, objMemContext(this));

    // Add the filter
    IoFilterData filterData = {.filter = filter};
    lstAdd(this->pub.filterList, &filterData);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
ioFilterGroupInsert(IoFilterGroup *const this, const unsigned int listIdx, IoFilter *const filter)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(IO_FILTER, filter);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.opened && !this->pub.closed);
    ASSERT(filter != NULL);

    // Move the filter to this object's mem context
    ioFilterMove(filter, objMemContext(this));

    // Add the filter
    IoFilterData filterData = {.filter = filter};
    lstInsert(this->pub.filterList, listIdx, &filterData);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Get a filter
***********************************************************************************************************************************/
static IoFilterData *
ioFilterGroupGet(const IoFilterGroup *const this, const unsigned int filterIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_TEST_PARAM(UINT, filterIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_FILTER_DATA, (IoFilterData *)lstGet(this->pub.filterList, filterIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
ioFilterGroupClear(IoFilterGroup *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.opened);

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
        ioFilterFree(ioFilterGroupGet(this, filterIdx)->filter);

    lstClear(this->pub.filterList);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Setup the filter group and allocate any required buffers
***********************************************************************************************************************************/
FN_EXTERN void
ioFilterGroupOpen(IoFilterGroup *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // If the last filter is not an output filter then add a filter to buffer/copy data. Input filters won't copy to an output
        // buffer so we need some way to get the data to the output buffer.
        if (ioFilterGroupSize(this) == 0 ||
            !ioFilterOutput((ioFilterGroupGet(this, ioFilterGroupSize(this) - 1))->filter))
        {
            ioFilterGroupAdd(this, ioBufferNew());
        }

        // Create filter input/output buffers. Input filters do not get an output buffer since they don't produce output.
        Buffer **lastOutputBuffer = NULL;

        for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
        {
            IoFilterData *const filterData = ioFilterGroupGet(this, filterIdx);

            // If there is no last output buffer yet, then use the input buffer that will be provided by the caller
            if (lastOutputBuffer == NULL)
            {
                filterData->input = &this->input;
            }
            // Else assign the last output buffer to the input
            else
            {
                // This cast is required because the compiler can't guarantee the const-ness of this object, i.e. it could be
                // modified in other parts of the code. This is actually expected and the only reason we need this const is to match
                // the const-ness of the input buffer provided by the caller.
                filterData->input = (const Buffer **)lastOutputBuffer;
                filterData->inputLocal = *lastOutputBuffer;
            }

            // If this is not the last output filter then create a new output buffer for it. The output buffer for the last filter
            // will be provided to the process function.
            if (ioFilterOutput(filterData->filter) && filterIdx < ioFilterGroupSize(this) - 1)
            {
                filterData->output = bufNew(ioBufferSize());
                lastOutputBuffer = &filterData->output;
            }
        }
    }
    MEM_CONTEXT_OBJ_END();

    // Filter group is open
#ifdef DEBUG
    this->pub.opened = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioFilterGroupProcess(IoFilterGroup *const this, const Buffer *const input, Buffer *const output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);
    ASSERT(input == NULL || !bufEmpty(input));
    ASSERT(!this->flushing || input == NULL);
    ASSERT(output != NULL);
    ASSERT(bufRemains(output) > 0);

    // Once input goes to NULL then flushing has started
#ifdef DEBUG
    if (input == NULL)
        this->flushing = true;
#endif

    // Assign input and output buffers
    this->input = input;
    (ioFilterGroupGet(this, ioFilterGroupSize(this) - 1))->output = output;

    //
    do
    {
        // Start from the first filter by default
        unsigned int filterIdx = 0;

        // Search from the end of the list for a filter that needs the same input. This indicates that the filter was not able to
        // empty the input buffer on the last call. Maybe it won't this time either -- we can but try.
        if (ioFilterGroupInputSame(this))
        {
            this->pub.inputSame = false;
            filterIdx = ioFilterGroupSize(this);

            do
            {
                filterIdx--;

                if (ioFilterInputSame((ioFilterGroupGet(this, filterIdx))->filter))
                {
                    this->pub.inputSame = true;
                    break;
                }
            }
            while (filterIdx != 0);

            // If no filter is found that needs the same input that means we are done with the current input. So end the loop and
            // get some more input.
            if (!ioFilterGroupInputSame(this))
                break;
        }

        // Process forward from the filter that has input to process. This may be a filter that needs the same input or it may be
        // new input for the first filter.
        for (; filterIdx < ioFilterGroupSize(this); filterIdx++)
        {
            IoFilterData *const filterData = ioFilterGroupGet(this, filterIdx);

            // Process the filter if it is not done
            if (!ioFilterDone(filterData->filter))
            {
                // If the filter produces output
                if (ioFilterOutput(filterData->filter))
                {
                    ioFilterProcessInOut(filterData->filter, *filterData->input, filterData->output);

                    // If inputSame is set then the output buffer for this filter is full and it will need to be re-processed with
                    // the same input once the output buffer is cleared
                    if (ioFilterInputSame(filterData->filter))
                    {
                        this->pub.inputSame = true;
                    }
                    // Else clear the buffer if it was locally allocated. If the input buffer was passed in then the caller is
                    // responsible for clearing it.
                    else if (filterData->inputLocal != NULL)
                        bufUsedZero(filterData->inputLocal);

                    // If the output buffer is not full and the filter is not done then more data is required
                    if (!bufFull(filterData->output) && !ioFilterDone(filterData->filter))
                        break;
                }
                // Else the filter does not produce output
                else
                    ioFilterProcessIn(filterData->filter, *filterData->input);
            }

            // If the filter is done and has no more output then null the output buffer. Downstream filters have a pointer to this
            // buffer so their inputs will also change to null and they'll flush.
            if (filterData->output != NULL && ioFilterDone(filterData->filter) && bufUsed(filterData->output) == 0)
                filterData->output = NULL;
        }
    }
    while (!bufFull(output) && ioFilterGroupInputSame(this));

    // Scan the filter list to determine if inputSame is set or done is not set for any filter. We can't trust
    // ioFilterGroupInputSame() when it is true without going through the loop above again. We need to scan to set this->pub.done
    // anyway so set this->pub.inputSame in the same loop.
    this->pub.done = true;
    this->pub.inputSame = false;

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
    {
        const IoFilterData *const filterData = ioFilterGroupGet(this, filterIdx);

        // When inputSame then this->pub.done = false and we can exit the loop immediately
        if (ioFilterInputSame(filterData->filter))
        {
            this->pub.done = false;
            this->pub.inputSame = true;
            break;
        }

        // Set this->pub.done = false if any filter is not done
        if (!ioFilterDone(filterData->filter))
            this->pub.done = false;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioFilterGroupClose(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Gather results from the filters
    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
    {
        const IoFilter *const filter = ioFilterGroupGet(this, filterIdx)->filter;

        MEM_CONTEXT_BEGIN(lstMemContext(this->filterResult))
        {
            lstAdd(this->filterResult, &(IoFilterResult){.type = ioFilterType(filter), .result = ioFilterResult(filter)});
        }
        MEM_CONTEXT_END();
    }

    // Filter group is open
#ifdef DEBUG
    this->pub.closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
ioFilterGroupParamAll(const IoFilterGroup *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.opened);
    ASSERT(this->pub.filterList != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
        {
            IoFilter *filter = ioFilterGroupGet(this, filterIdx)->filter;

            pckWriteStrIdP(packWrite, ioFilterType(filter));
            pckWritePackP(packWrite, ioFilterParamList(filter));
        }

        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const Pack *
ioFilterGroupResultPack(const IoFilterGroup *const this, const StringId filterType, const IoFilterGroupResultParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(STRING_ID, filterType);
        FUNCTION_LOG_PARAM(UINT, param.idx);
    FUNCTION_LOG_END();

    ASSERT(this->pub.opened);
    ASSERT(filterType != 0);

    const Pack *result = NULL;

    // Search for the result
    unsigned int foundIdx = 0;

    for (unsigned int filterResultIdx = 0; filterResultIdx < lstSize(this->filterResult); filterResultIdx++)
    {
        const IoFilterResult *const filterResult = lstGet(this->filterResult, filterResultIdx);

        // If the filter matches check the index
        if (filterResult->type == filterType)
        {
            // If the index matches return the result
            if (foundIdx == param.idx)
            {
                result = filterResult->result;
                break;
            }

            // Increment the index and keep searching
            foundIdx++;
        }
    }

    FUNCTION_LOG_RETURN_CONST(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
ioFilterGroupResult(const IoFilterGroup *const this, const StringId filterType, const IoFilterGroupResultParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(STRING_ID, filterType);
        FUNCTION_LOG_PARAM(UINT, param.idx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(PACK_READ, pckReadNew(ioFilterGroupResultPack(this, filterType, param)));
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
ioFilterGroupResultAll(const IoFilterGroup *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.closed);

    Pack *result = NULL;

    // Pack the result list
    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        for (unsigned int filterResultIdx = 0; filterResultIdx < lstSize(this->filterResult); filterResultIdx++)
        {
            const IoFilterResult *const filterResult = lstGet(this->filterResult, filterResultIdx);

            pckWriteStrIdP(packWrite, filterResult->type);
            pckWritePackP(packWrite, filterResult->result);
        }

        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioFilterGroupResultAllSet(IoFilterGroup *const this, const Pack *const filterResultPack)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(PACK, filterResultPack);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (filterResultPack != NULL)
    {
        // Unpack the results into a list
        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackRead *const packRead = pckReadNew(filterResultPack);

            MEM_CONTEXT_BEGIN(lstMemContext(this->filterResult))
            {
                while (!pckReadNullP(packRead))
                {
                    const StringId type = pckReadStrIdP(packRead);
                    Pack *const result = pckReadPackP(packRead);

                    lstAdd(this->filterResult, &(IoFilterResult){.type = type, .result = result});
                }
            }
            MEM_CONTEXT_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioFilterGroupToLog(const IoFilterGroup *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog,
        "{inputSame: %s, done: %s"
#ifdef DEBUG
        ", opened %s, flushing %s, closed %s"
#endif
        "}",
        cvtBoolToConstZ(this->pub.inputSame), cvtBoolToConstZ(this->pub.done)
#ifdef DEBUG
        , cvtBoolToConstZ(this->pub.opened), cvtBoolToConstZ(this->flushing), cvtBoolToConstZ(this->pub.closed)
#endif
        );
}
