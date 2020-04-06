/***********************************************************************************************************************************
IO Filter Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilter
{
    MemContext *memContext;                                         // Mem context of filter
    const String *type;                                             // Filter type
    void *driver;                                                   // Filter driver
    const VariantList *paramList;                                   // Filter parameters
    IoFilterInterface interface;                                    // Filter interface

    bool flushing;                                                  // Has the filter started flushing?
};

OBJECT_DEFINE_MOVE(IO_FILTER);
OBJECT_DEFINE_FREE(IO_FILTER);

/***********************************************************************************************************************************
Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoFilter *
ioFilterNew(const String *type, void *driver, VariantList *paramList, IoFilterInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(IO_FILTER_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(driver != NULL);
    // One of processIn or processInOut must be set
    ASSERT(interface.in != NULL || interface.inOut != NULL);
    // But not both of them
    ASSERT(!(interface.in != NULL && interface.inOut != NULL));
    // If the filter does not produce output then it should produce a result
    ASSERT(interface.in == NULL || (interface.result != NULL && interface.done == NULL && interface.inputSame == NULL));

    IoFilter *this = memNew(sizeof(IoFilter));

    *this = (IoFilter)
    {
        .memContext = memContextCurrent(),
        .type = type,
        .driver = driver,
        .paramList = paramList,
        .interface = interface,
    };

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

/**********************************************************************************************************************************/
void
ioFilterProcessIn(IoFilter *this, const Buffer *input)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
        FUNCTION_TEST_PARAM(BUFFER, input);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->interface.in != NULL);
    CHECK(input == NULL || bufUsed(input) > 0);
    CHECK(!this->flushing || input == NULL);

    if (input == NULL)
        this->flushing = true;
    else
        this->interface.in(this->driver, input);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
ioFilterProcessInOut(IoFilter *this, const Buffer *input, Buffer *output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
        FUNCTION_TEST_PARAM(BUFFER, input);
        FUNCTION_TEST_PARAM(BUFFER, output);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL);
    ASSERT(this->interface.inOut != NULL);
    CHECK(input == NULL || bufUsed(input) > 0);
    CHECK(!this->flushing || input == NULL);

    if (input == NULL && !this->flushing)
        this->flushing = true;

    if (!ioFilterDone(this))
        this->interface.inOut(this->driver, input, output);

    CHECK(!ioFilterInputSame(this) || bufUsed(output) > 0);
    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
If done is not defined by the filter then check inputSame.  If inputSame is true then the filter is not done.  Even if the filter
is done the interface will not report done until the interface is flushing.
***********************************************************************************************************************************/
bool
ioFilterDone(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    if (this->flushing)
        result = this->interface.done != NULL ? this->interface.done(this->driver) : !ioFilterInputSame(this);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void *
ioFilterDriver(IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->driver);
}

/**********************************************************************************************************************************/
bool
ioFilterInputSame(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface.inputSame != NULL ? this->interface.inputSame(this->driver) : false);
}

/**********************************************************************************************************************************/
const IoFilterInterface *
ioFilterInterface(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(&this->interface);
}

/**********************************************************************************************************************************/
bool
ioFilterOutput(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface.inOut != NULL);
}

/**********************************************************************************************************************************/
const VariantList *
ioFilterParamList(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->paramList);
}

/**********************************************************************************************************************************/
Variant *
ioFilterResult(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface.result ? this->interface.result(this->driver) : NULL);
}

/**********************************************************************************************************************************/
const String *
ioFilterType(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->type);
}

/**********************************************************************************************************************************/
String *
ioFilterToLog(const IoFilter *this)
{
    return strNewFmt("{type: %s}", strPtr(this->type));
}
