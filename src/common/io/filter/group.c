/***********************************************************************************************************************************
IO Filter Group
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/filter.intern.h"
#include "common/io/filter/group.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/object.h"

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
    objToLog(value, "IoFilterData", buffer, bufferSize)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilterGroup
{
    MemContext *memContext;                                         // Mem context
    List *filterList;                                               // List of filters to apply
    const Buffer *input;                                            // Input buffer passed in for processing
    KeyValue *filterResult;                                         // Filter results (if any)
    bool inputSame;                                                 // Same input required again?
    bool done;                                                      // Is processing done?

#ifdef DEBUG
    bool opened;                                                    // Has the filter set been opened?
    bool flushing;                                                  // Is output being flushed?
    bool closed;                                                    // Has the filter set been closed?
#endif
};

OBJECT_DEFINE_FREE(IO_FILTER_GROUP);

/**********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilterGroup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoFilterGroup")
    {
        this = memNew(sizeof(IoFilterGroup));

        *this = (IoFilterGroup)
        {
            .memContext = memContextCurrent(),
            .done = false,
            .filterList = lstNewP(sizeof(IoFilterData)),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupInsert(IoFilterGroup *this, unsigned int listIdx, IoFilter *filter)
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
    lstInsert(this->filterList, listIdx, &filterData);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Get a filter
***********************************************************************************************************************************/
static IoFilterData *
ioFilterGroupGet(const IoFilterGroup *this, unsigned int filterIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_TEST_PARAM(UINT, filterIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((IoFilterData *)lstGet(this->filterList, filterIdx));
}

/**********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupClear(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->opened);

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
        ioFilterFree(ioFilterGroupGet(this, filterIdx)->filter);

    lstClear(this->filterList);

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Setup the filter group and allocate any required buffers
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
        if (ioFilterGroupSize(this) == 0 ||
            !ioFilterOutput((ioFilterGroupGet(this, ioFilterGroupSize(this) - 1))->filter))
        {
            ioFilterGroupAdd(this, ioBufferNew());
        }

        // Create filter input/output buffers.  Input filters do not get an output buffer since they don't produce output.
        Buffer **lastOutputBuffer = NULL;

        for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
        {
            IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);

            // If there is no last output buffer yet, then use the input buffer that will be provided by the caller
            if (lastOutputBuffer == NULL)
            {
                filterData->input = &this->input;
            }
            // Else assign the last output buffer to the input
            else
            {
                // This cast is required because the compiler can't guarantee the const-ness of this object, i.e. it could be
                // modified in other parts of the code.  This is actually expected and the only reason we need this const is to
                // match the const-ness of the input buffer provided by the caller.
                filterData->input = (const Buffer **)lastOutputBuffer;
                filterData->inputLocal = *lastOutputBuffer;
            }

            // If this is not the last output filter then create a new output buffer for it.  The output buffer for the last filter
            // will be provided to the process function.
            if (ioFilterOutput(filterData->filter) && filterIdx < ioFilterGroupSize(this) - 1)
            {
                filterData->output = bufNew(ioBufferSize());
                lastOutputBuffer = &filterData->output;
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

/**********************************************************************************************************************************/
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

        // Search from the end of the list for a filter that needs the same input.  This indicates that the filter was not able to
        // empty the input buffer on the last call.  Maybe it won't this time either -- we can but try.
        if (this->inputSame)
        {
            this->inputSame = false;
            filterIdx = ioFilterGroupSize(this);

            do
            {
                filterIdx--;

                if (ioFilterInputSame((ioFilterGroupGet(this, filterIdx))->filter))
                {
                    this->inputSame = true;
                    break;
                }
            }
            while (filterIdx != 0);

            // If no filter is found that needs the same input that means we are done with the current input.  So end the loop and
            // get some more input.
            if (!this->inputSame)
                break;
        }

        // Process forward from the filter that has input to process.  This may be a filter that needs the same input or it may be
        // new input for the first filter.
        for (; filterIdx < ioFilterGroupSize(this); filterIdx++)
        {
            IoFilterData *filterData = ioFilterGroupGet(this, filterIdx);

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
                        this->inputSame = true;
                    }
                    // Else clear the buffer if it was locally allocated.  If the input buffer was passed in then the caller is
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

            // If the filter is done and has no more output then null the output buffer.  Downstream filters have a pointer to this
            // buffer so their inputs will also change to null and they'll flush.
            if (filterData->output != NULL && ioFilterDone(filterData->filter) && bufUsed(filterData->output) == 0)
                filterData->output = NULL;
        }
    }
    while (!bufFull(output) && this->inputSame);

    // Scan the filter list to determine if inputSame is set or done is not set for any filter.  We can't trust this->inputSame
    // when it is true without going through the loop above again.  We need to scan to set this->done anyway so set this->inputSame
    // in the same loop.
    this->done = true;
    this->inputSame = false;

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
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

/**********************************************************************************************************************************/
void
ioFilterGroupClose(IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
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
            kvAdd(this->filterResult, VARSTR(ioFilterType(filterData->filter)), filterResult);
        }
        MEM_CONTEXT_TEMP_END();
    }

    // Filter group is open
#ifdef DEBUG
    this->closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
ioFilterGroupDone(const IoFilterGroup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_TEST_RETURN(this->done);
}

/**********************************************************************************************************************************/
bool
ioFilterGroupInputSame(const IoFilterGroup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->opened && !this->closed);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
Variant *
ioFilterGroupParamAll(const IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->opened);
    ASSERT(this->filterList != NULL);

    VariantList *result = varLstNew();

    for (unsigned int filterIdx = 0; filterIdx < ioFilterGroupSize(this); filterIdx++)
    {
        IoFilter *filter = ioFilterGroupGet(this, filterIdx)->filter;
        const VariantList *paramList = ioFilterParamList(filter);

        KeyValue *filterParam = kvNew();
        kvAdd(filterParam, VARSTR(ioFilterType(filter)), paramList ? varNewVarLst(paramList) : NULL);

        varLstAdd(result, varNewKv(filterParam));
    }

    FUNCTION_LOG_RETURN(VARIANT, varNewVarLst(result));
}

/**********************************************************************************************************************************/
const Variant *
ioFilterGroupResult(const IoFilterGroup *this, const String *filterType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_LOG_PARAM(STRING, filterType);
    FUNCTION_LOG_END();

    ASSERT(this->opened);
    ASSERT(filterType != NULL);

    const Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = kvGet(this->filterResult, VARSTR(filterType));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_CONST(VARIANT, result);
}

/**********************************************************************************************************************************/
const Variant *
ioFilterGroupResultAll(const IoFilterGroup *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->closed);

    FUNCTION_LOG_RETURN_CONST(VARIANT, varNewKv(this->filterResult));
}

/**********************************************************************************************************************************/
void
ioFilterGroupResultAllSet(IoFilterGroup *this, const Variant *filterResult)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (filterResult != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->filterResult = kvDup(varKv(filterResult));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
unsigned int
ioFilterGroupSize(const IoFilterGroup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(lstSize(this->filterList));
}

/**********************************************************************************************************************************/
String *
ioFilterGroupToLog(const IoFilterGroup *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s"
#ifdef DEBUG
            ", opened %s, flushing %s, closed %s"
#endif
            "}",
        cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done)
#ifdef DEBUG
        , cvtBoolToConstZ(this->opened), cvtBoolToConstZ(this->flushing), cvtBoolToConstZ(this->closed)
#endif
    );
}
