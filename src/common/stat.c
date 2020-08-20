/***********************************************************************************************************************************
Statistics Collector
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/memContext.h"
#include "common/stat.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Stat output constants
***********************************************************************************************************************************/
VARIANT_STRDEF_EXTERN(STAT_VALUE_TOTAL_VAR,                         STAT_VALUE_TOTAL);

/***********************************************************************************************************************************
Cumulative statistics
***********************************************************************************************************************************/
typedef struct Stat
{
    const String *key;
    uint64_t total;
} Stat;

/***********************************************************************************************************************************
Local data
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct
    List *stat;                                                     // Cumulative stats
} statLocalData;

/**********************************************************************************************************************************/
void
statInit(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(statLocalData.memContext == NULL);

    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN("StatLocalData")
        {
            statLocalData.memContext = MEM_CONTEXT_NEW();
            statLocalData.stat = lstNewP(sizeof(Stat), .sortOrder = sortOrderAsc, .comparator = lstComparatorStr);
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the specified stat. If it doesn't already exist it will be created.
***********************************************************************************************************************************/
static Stat *
statGetOrCreate(const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(key != NULL);

    // Attempt to find the stat
    Stat *stat = lstFind(statLocalData.stat, &key);

    // If not found then create it
    if (stat == NULL)
    {
        // Add the new stat
        MEM_CONTEXT_BEGIN(lstMemContext(statLocalData.stat))
        {
            lstAdd(statLocalData.stat, &(Stat){.key = strDup(key)});
        }
        MEM_CONTEXT_END();

        // Sort stats so this stat will be easier to find later
        lstSort(statLocalData.stat, sortOrderAsc);

        // The stat might have moved so we'll need to find it and make sure we have the correct pointer
        stat = lstFind(statLocalData.stat, &key);
        ASSERT(stat != NULL);
    }

    FUNCTION_TEST_RETURN(stat);
}

/**********************************************************************************************************************************/
void
statInc(const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(statLocalData.memContext != NULL);
    ASSERT(key != NULL);

    statGetOrCreate(key)->total++;

    FUNCTION_TEST_RETURN();
}

/**********************************************************************************************************************************/
KeyValue *
statToKv(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(statLocalData.memContext != NULL);

    KeyValue *result = kvNew();

    for (unsigned int statIdx = 0; statIdx < lstSize(statLocalData.stat); statIdx++)
    {
        Stat *stat = lstGet(statLocalData.stat, statIdx);

        KeyValue *statKv = kvPutKv(result, VARSTR(stat->key));
        kvAdd(statKv, STAT_VALUE_TOTAL_VAR, VARUINT64(stat->total));
    }

    FUNCTION_TEST_RETURN(result);
}
