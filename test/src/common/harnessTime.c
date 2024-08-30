/***********************************************************************************************************************************
Time Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessDebug.h"
#include "common/harnessTime.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

static struct
{
    const TimeMSec *timeList;                                       // Times to return from timeMSec()
    size_t timeListSize;                                            // Time list size
    size_t timeListCurrent;                                         // Time list current position
} hrnTimeLocal;

/**********************************************************************************************************************************/
void
hrnTimeMSecSet(const TimeMSec *const timeList, const size_t timeListSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM_P(VOID, timeList);
        FUNCTION_HARNESS_PARAM(SIZE, timeListSize);
    FUNCTION_HARNESS_END();

    ASSERT(hrnTimeLocal.timeList == NULL);
    ASSERT(timeListSize > 0);

    hrnTimeLocal.timeList = timeList;
    hrnTimeLocal.timeListSize = timeListSize;
    hrnTimeLocal.timeListCurrent = 0;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnSleepRemainder(void)
{
    FUNCTION_HARNESS_VOID();

    TimeMSec target;

    do
    {
        const TimeMSec current = timeMSec();
        target = (current / 1000 + 1) * 1000;

        sleepMSec(target - current);
    }
    while (timeMSec() < target);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
TimeMSec
timeMSec(void)
{
    FUNCTION_HARNESS_VOID();

    TimeMSec result;

    // Return normal result
    if (hrnTimeLocal.timeList == NULL)
    {
        result = timeMSec_SHIMMED();
    }
    // Else return a value from the list
    else
    {
        result = hrnTimeLocal.timeList[hrnTimeLocal.timeListCurrent];

        // Reset list when the last value has been returned
        hrnTimeLocal.timeListCurrent++;

        if (hrnTimeLocal.timeListCurrent == hrnTimeLocal.timeListSize)
            hrnTimeLocal.timeList = NULL;
    }

    FUNCTION_HARNESS_RETURN(TIME_MSEC, result);
}
