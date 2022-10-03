/***********************************************************************************************************************************
IO Filter Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoFilter
{
    IoFilterPub pub;                                                // Publicly accessible variables
    bool flushing;                                                  // Has the filter started flushing?
};

/***********************************************************************************************************************************
Allocations will be in the memory context of the caller.
***********************************************************************************************************************************/
IoFilter *
ioFilterNew(const StringId type, void *const driver, Pack *const paramList, const IoFilterInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(PACK, paramList);
        FUNCTION_LOG_PARAM(IO_FILTER_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
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
        .pub =
        {
            .memContext = memContextCurrent(),
            .type = type,
            .driver = driver,
            .paramList = paramList,
            .interface = interface,
        },
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
    ASSERT(this->pub.interface.in != NULL);
    CHECK(AssertError, input == NULL || !bufEmpty(input), "expected data or flush");
    CHECK(AssertError, !this->flushing || input == NULL, "no data allowed after flush");

    if (input == NULL)
        this->flushing = true;
    else
        this->pub.interface.in(this->pub.driver, input);

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
    ASSERT(this->pub.interface.inOut != NULL);
    CHECK(AssertError, input == NULL || !bufEmpty(input), "expected data or flush");
    CHECK(AssertError, !this->flushing || input == NULL, "no data allowed after flush");

    if (input == NULL && !this->flushing)
        this->flushing = true;

    if (!ioFilterDone(this))
        this->pub.interface.inOut(this->pub.driver, input, output);

    // If input same is requested then there must be some output otherwise there is no point in requesting the same input
    CHECK(AssertError, !ioFilterInputSame(this) || !bufEmpty(output), "expected input to be consumed or some output");

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
        result = this->pub.interface.done != NULL ? this->pub.interface.done(this->pub.driver) : !ioFilterInputSame(this);

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
bool
ioFilterInputSame(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->pub.interface.inputSame != NULL ? this->pub.interface.inputSame(this->pub.driver) : false);
}

/**********************************************************************************************************************************/
Pack *
ioFilterResult(const IoFilter *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(PACK, this->pub.interface.result ? this->pub.interface.result(this->pub.driver) : NULL);
}

/**********************************************************************************************************************************/
String *
ioFilterToLog(const IoFilter *this)
{
    return strNewFmt("{type: %s}", strZ(strIdToStr(ioFilterType(this))));
}
