/***********************************************************************************************************************************
Filter Group
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Filter group structure
***********************************************************************************************************************************/
struct IoFilterGroup
{
    MemContext *memContext;                                         // Mem context
    List *filterList;                                               // List of filters to apply to read buffer
    KeyValue *filterResult;                                         // Filter results (if any)
};

/***********************************************************************************************************************************
Create new filter group
***********************************************************************************************************************************/
IoFilterGroup *
ioFilterGroupNew(void)
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    IoFilterGroup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoFilterGroup")
    {
        this = memNew(sizeof(IoFilterGroup));
        this->memContext = memContextCurrent();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_FILTER_GROUP, this);
}

/***********************************************************************************************************************************
Add a filter
***********************************************************************************************************************************/
void
ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_DEBUG_PARAM(IO_FILTER, filter);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(filter != NULL);
    FUNCTION_DEBUG_END();

    // Create the filter list if it has not been created
    if (this->filterList == NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->filterList = lstNew(sizeof(IoFilter *));
        }
        MEM_CONTEXT_END();
    }

    // Move the filter to this object's mem context
    ioFilterMove(filter, this->memContext);

    // Add the filter
    lstAdd(this->filterList, &filter);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Process filters
***********************************************************************************************************************************/
void
ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_DEBUG_PARAM(BUFFER, input);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(input != NULL);
    FUNCTION_DEBUG_END();

    for (unsigned int filterIdx = 0; filterIdx < lstSize(this->filterList); filterIdx++)
        ioFilterProcessIn(*(IoFilter **)lstGet(this->filterList, filterIdx), input);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Close filter group
***********************************************************************************************************************************/
void
ioFilterGroupClose(IoFilterGroup *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    for (unsigned int filterIdx = 0; filterIdx < lstSize(this->filterList); filterIdx++)
    {
        IoFilter *filter = *(IoFilter **)lstGet(this->filterList, filterIdx);

        const Variant *filterResult = ioFilterResult(filter);

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
            kvPut(this->filterResult, varNewStr(ioFilterType(filter)), filterResult);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get filter results
***********************************************************************************************************************************/
const Variant *
ioFilterGroupResult(const IoFilterGroup *this, const String *filterType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, this);
        FUNCTION_TEST_PARAM(STRING, filterType);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(filterType != NULL);
    FUNCTION_TEST_END();

    const Variant *result = NULL;

    if (this->filterResult != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            result = kvGet(this->filterResult, varNewStr(filterType));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RESULT(CONST_VARIANT, result);
}

/***********************************************************************************************************************************
Free the filter group
***********************************************************************************************************************************/
void
ioFilterGroupFree(IoFilterGroup *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
