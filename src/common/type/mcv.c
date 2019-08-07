/***********************************************************************************************************************************
Calculate Most Common Value
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/object.h"
#include "common/type/list.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct MostCommonValue
{
    MemContext *memContext;                                         // Mem context
    VariantType type;                                               // Type of variant to calculate most common value for
    List *list;                                                     // List of unique values
};

typedef struct MostCommonValueEntry
{
    const Variant *value;                                           // Value to be counted
    unsigned int total;                                             // Total count for the value
} MostCommonValueEntry;

OBJECT_DEFINE_FREE(MOST_COMMON_VALUE);

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
MostCommonValue *
mcvNew(VariantType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    MostCommonValue *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("MostCommonValue")
    {
        this = memNew(sizeof(MostCommonValue));
        this->memContext = MEM_CONTEXT_NEW();
        this->type = type;
        this->list = lstNew(sizeof(Variant *));
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Update counts for a value
***********************************************************************************************************************************/
MostCommonValue *
mcvUpdate(MostCommonValue *this, Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MOST_COMMON_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool found = false;

    for (unsigned int listIdx = 0; listIdx < lstSize(this->list); listIdx++)
    {
        MostCommonValueEntry *entry = (MostCommonValue *)lstGet(listIdx);

        if (varEq(value, entry))
        {
            entry->total++;
            found = true;
            break;
        }
    }

    if (!found)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            MostCommonValueEntry entry = {.value = varDup(value), .total = 1};
            lstAdd(this->list, &entry);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get most common value
***********************************************************************************************************************************/
const Variant *
mcvResult(const MostCommonValue *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MOST_COMMON_VALUE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const Variant *result = NULL;

    FUNCTION_TEST_RETURN(result);
}
