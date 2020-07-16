/***********************************************************************************************************************************
Statistics Collector
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Stat info
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context to store data in this struct
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
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}
