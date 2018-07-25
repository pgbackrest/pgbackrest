/***********************************************************************************************************************************
IO Filter
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter structure
***********************************************************************************************************************************/
struct IoFilter
{
    MemContext *memContext;                                         // Mem context of filter
    const String *type;                                             // Filter type
    void *data;                                                     // Filter data
    IoFilterProcessIn processIn;                                    // Process in function
    IoFilterResult result;                                          // Result function
};

/***********************************************************************************************************************************
Create a new filter

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoFilter *
ioFilterNew(const String *type, void *data, IoFilterProcessIn processIn, IoFilterResult result)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(VOIDP, data);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, processIn);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, result);

        FUNCTION_DEBUG_ASSERT(type != NULL);
        FUNCTION_DEBUG_ASSERT(data != NULL);
        FUNCTION_DEBUG_ASSERT(processIn != NULL);
        FUNCTION_DEBUG_ASSERT(result != NULL);
    FUNCTION_DEBUG_END();

    IoFilter *this = memNew(sizeof(IoFilter));
    this->memContext = memContextCurrent();
    this->type = type;
    this->data = data;
    this->processIn = processIn;
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
    FUNCTION_TEST_END();

    this->processIn(this->data, input);

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Close the filter and return any results
***********************************************************************************************************************************/
const Variant *
ioFilterResult(IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(VARIANT, this->result ? this->result(this->data) : NULL);
}

/***********************************************************************************************************************************
Move the file object to a new context
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
Get filter type
***********************************************************************************************************************************/
const String *
ioFilterType(IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->type);
}
