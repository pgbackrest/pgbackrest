/***********************************************************************************************************************************
IO Filter Interface
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilter
{
    MemContext *memContext;                                         // Mem context of filter
    const String *type;                                             // Filter type
    void *data;                                                     // Filter data
    IoFilterDone done;                                              // Done processing?
    IoFilterInputSame inputSame;                                    // Does the filter need the same input again?
    IoFilterProcessIn processIn;                                    // Process in function
    IoFilterProcessInOut processInOut;                              // Process in/out function
    IoFilterResult result;                                          // Result function
};

/***********************************************************************************************************************************
New object

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoFilter *
ioFilterNew(
    const String *type, void *data, IoFilterDone done, IoFilterInputSame inputSame, IoFilterProcessIn processIn,
    IoFilterProcessInOut processInOut, IoFilterResult result)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(VOIDP, data);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, done);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, inputSame);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, processIn);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, processInOut);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, result);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(data != NULL);
        // One of processIn or processInOut must be set
        FUNCTION_TEST_ASSERT(processIn != NULL || processInOut != NULL);
        // But not both of them
        FUNCTION_TEST_ASSERT(!(processIn != NULL && processInOut != NULL));
        // If the filter does not produce output then it should produce a result
        FUNCTION_TEST_ASSERT(processIn == NULL || (result != NULL && done == NULL && inputSame == NULL));
        // Filters that produce output will not always be able to dump all their output and will need to get the same input again
        FUNCTION_TEST_ASSERT(processInOut == NULL || inputSame != NULL);
    FUNCTION_DEBUG_END();

    IoFilter *this = memNew(sizeof(IoFilter));
    this->memContext = memContextCurrent();
    this->type = type;
    this->data = data;
    this->done = done;
    this->inputSame = inputSame;
    this->processIn = processIn;
    this->processInOut = processInOut;
    this->result = result;

    FUNCTION_DEBUG_RESULT(IO_FILTER, this);
}

/***********************************************************************************************************************************
Filter input only (a result is expected)
***********************************************************************************************************************************/
void
ioFilterProcessIn(IoFilter *this, const Buffer *input)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
        FUNCTION_TEST_PARAM(BUFFER, input);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->processIn != NULL);
    FUNCTION_TEST_END();

    this->processIn(this->data, input);

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Filter input and produce output
***********************************************************************************************************************************/
void
ioFilterProcessInOut(IoFilter *this, const Buffer *input, Buffer *output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
        FUNCTION_TEST_PARAM(BUFFER, input);
        FUNCTION_TEST_PARAM(BUFFER, output);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(output != NULL);
        FUNCTION_TEST_ASSERT(this->processInOut != NULL);
    FUNCTION_TEST_END();

    this->processInOut(this->data, input, output);

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Move the object to a new context
***********************************************************************************************************************************/
IoFilter *
ioFilterMove(IoFilter *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(IO_FILTER, this);
}

/***********************************************************************************************************************************
Is the filter done?

If done is not defined by the filter then check inputSame.  If inputSame is true then the filter is not done.
***********************************************************************************************************************************/
bool
ioFilterDone(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->done != NULL ? this->done(this->data) : !ioFilterInputSame(this));
}

/***********************************************************************************************************************************
Does the filter need the same input again?

If the filter cannot get all its output into the output buffer then it may need access to the same input again.
***********************************************************************************************************************************/
bool
ioFilterInputSame(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->inputSame != NULL ? this->inputSame(this->data) : false);
}

/***********************************************************************************************************************************
Does filter produce output?

All InOut filters produce output.
***********************************************************************************************************************************/
bool
ioFilterOutput(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->processInOut != NULL);
}

/***********************************************************************************************************************************
Get filter result
***********************************************************************************************************************************/
const Variant *
ioFilterResult(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(VARIANT, this->result ? this->result(this->data) : NULL);
}

/***********************************************************************************************************************************
Get filter type

This name identifies the filter and is used when pulling results from the filter group.
***********************************************************************************************************************************/
const String *
ioFilterType(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->type);
}
