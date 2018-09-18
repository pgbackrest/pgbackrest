/***********************************************************************************************************************************
IO Filter Interface
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilter
{
    MemContext *memContext;                                         // Mem context of filter
    const String *type;                                             // Filter type
    void *driver;                                                   // Filter driver
    IoFilterInterface interface;                                    // Filter interface
};

/***********************************************************************************************************************************
New object

Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoFilter *
ioFilterNew(const String *type, void *driver, IoFilterInterface interface)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(IO_FILTER_INTERFACE, interface);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(driver != NULL);
        // One of processIn or processInOut must be set
        FUNCTION_TEST_ASSERT(interface.in != NULL || interface.inOut != NULL);
        // But not both of them
        FUNCTION_TEST_ASSERT(!(interface.in != NULL && interface.inOut != NULL));
        // If the filter does not produce output then it should produce a result
        FUNCTION_TEST_ASSERT(
            interface.in == NULL || (interface.result != NULL && interface.done == NULL && interface.inputSame == NULL));
        // Filters that produce output will not always be able to dump all their output and will need to get the same input again
        FUNCTION_TEST_ASSERT(interface.inOut == NULL || interface.inputSame != NULL);
    FUNCTION_DEBUG_END();

    IoFilter *this = memNew(sizeof(IoFilter));
    this->memContext = memContextCurrent();
    this->type = type;
    this->driver = driver;
    this->interface = interface;

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
        FUNCTION_TEST_ASSERT(this->interface.in != NULL);
    FUNCTION_TEST_END();

    this->interface.in(this->driver, input);

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
        FUNCTION_TEST_ASSERT(this->interface.inOut != NULL);
    FUNCTION_TEST_END();

    this->interface.inOut(this->driver, input, output);

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

    FUNCTION_TEST_RESULT(BOOL, this->interface.done != NULL ? this->interface.done(this->driver) : !ioFilterInputSame(this));
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

    FUNCTION_TEST_RESULT(BOOL, this->interface.inputSame != NULL ? this->interface.inputSame(this->driver) : false);
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

    FUNCTION_TEST_RESULT(BOOL, this->interface.inOut != NULL);
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

    FUNCTION_TEST_RESULT(VARIANT, this->interface.result ? this->interface.result(this->driver) : NULL);
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
