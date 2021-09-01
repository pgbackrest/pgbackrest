/***********************************************************************************************************************************
Calculate Most Common Value
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/type/list.h"
#include "common/type/mcv.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct MostCommonValue
{
    List *list;                                                     // List of unique values
};

typedef struct MostCommonValueEntry
{
    const Variant *value;                                           // Value to be counted
    uint64_t total;                                                 // Total count for the value
} MostCommonValueEntry;

/**********************************************************************************************************************************/
MostCommonValue *
mcvNew(void)
{
    FUNCTION_TEST_VOID();

    MostCommonValue *this = NULL;

    OBJ_NEW_BEGIN(MostCommonValue)
    {
        this = OBJ_NEW_ALLOC();

        *this = (MostCommonValue)
        {
            .list = lstNewP(sizeof(MostCommonValueEntry)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
MostCommonValue *
mcvUpdate(MostCommonValue *this, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MOST_COMMON_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool found = false;

    // Increment the value if it already exists
    for (unsigned int listIdx = 0; listIdx < lstSize(this->list); listIdx++)
    {
        MostCommonValueEntry *entry = (MostCommonValueEntry *)lstGet(this->list, listIdx);

        if (varEq(value, entry->value))
        {
            entry->total++;
            found = true;
            break;
        }
    }

    // Add the value if it doesn't
    if (!found)
    {
        MEM_CONTEXT_BEGIN(objMemContext(this))
        {
            MostCommonValueEntry entry = {.value = varDup(value), .total = 1};
            lstAdd(this->list, &entry);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
const Variant *
mcvResult(const MostCommonValue *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MOST_COMMON_VALUE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const Variant *result = NULL;
    uint64_t resultTotal = 0;

    for (unsigned int listIdx = 0; listIdx < lstSize(this->list); listIdx++)
    {
        MostCommonValueEntry *entry = (MostCommonValueEntry *)lstGet(this->list, listIdx);

        if (entry->total > resultTotal ||
            // If boolean always return false when there is a tie.  This is to maintain compatibility with older mcv code/tests.
            (entry->total == resultTotal && varType(entry->value) == varTypeBool && !varBool(entry->value)))
        {
            result = entry->value;
            resultTotal = entry->total;
        }
    }

    FUNCTION_TEST_RETURN(result);
}
