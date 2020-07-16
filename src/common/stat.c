/***********************************************************************************************************************************
Statistics Collector
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Cumulative statistics
***********************************************************************************************************************************/
typedef struct StatData
{
    const String *name;
    uint64_t count;
    uint64_t total;
} StatData;

/***********************************************************************************************************************************
Current data for a statistic that has not ended
***********************************************************************************************************************************/
typedef struct StatCurrentData
{
    StatData *stat;
    TimeMSec timeBegin;
} StatData;

/***********************************************************************************************************************************
Local data
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct

    List *stat;                                                     // Cumulative stats
    List *current;                                                  // Stats currently being tracked
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
            statLocalData.current = lstNewP(sizeof(StatCurrentData));
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
    StatData *stat = lstFind(&key);

    // If not found then create it
    if (stat == NULL)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(statLocalData.stat))
        {
            stat = lstAdd(statLocalData.stat, &(StatDate){.name = strDup(key)});

            // It is only safe to sort if there are no current stats because there may be a pointer into the stats list which might
            // be moved by a new name. So we are definitely leaving some performance on the floor here.
            if (lstSize(statLocalData.current) == 0)
                lstSort(statLocalData.stat);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(stat);
}

/**********************************************************************************************************************************/
void statTimeBegin(const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(statLocalData.memContext != NULL);

    // Initialize current with the begin time
    StatCurrentData current =
    {
        .stat = statGetOrCreate(key),
        .timeBegin = timeMSec();
    }

    if (userData != NULL)
        FUNCTION_TEST_RETURN(strNew(userData->pw_name));

    FUNCTION_TEST_RETURN(NULL);
}

void statTimeEnd(const String *key);
{
}
