/***********************************************************************************************************************************
Statistics Collector
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "common/time.h"

/***********************************************************************************************************************************
Cumulative statistics
***********************************************************************************************************************************/
typedef struct StatData
{
    const String *name;
    uint64_t total;
    // TimeMSec timeTotal;
} StatData;

/***********************************************************************************************************************************
Local data
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct

    List *stat;                                                     // Cumulative stats
} statLocalData;

/**********************************************************************************************************************************/
static void
statInit(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(statLocalData.memContext == NULL);

    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN("StatLocalData")
        {
            statLocalData.memContext = MEM_CONTEXT_NEW();
            statLocalData.stat = lstNewP(sizeof(StatData), .sortOrder = sortOrderAsc, .comparator = lstComparatorStr);
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static StatData *statGetOrCreate(const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    // Attempt to find the stat
    StatData *stat = lstFind(statLocalData.stat, &key);

    // If not found then create it
    if (stat == NULL)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(statLocalData.stat))
        {
            stat = lstAdd(statLocalData.stat, &(StatData){.name = strDup(key)});
        }
        MEM_CONTEXT_END();

        // Sort stats so this stat will be easier to find later
        lstSort(statLocalData.stat, sortOrderAsc);
    }

    FUNCTION_TEST_RETURN(stat);
}

/**********************************************************************************************************************************/
void statInc(const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(statLocalData.memContext != NULL);

    statGetOrCreate(key)->total++;

    FUNCTION_TEST_RETURN();
}
