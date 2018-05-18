/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#include "stdio.h"
#include "sys/time.h"

#include "common/debug.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constants describing number of sub-units in an interval
***********************************************************************************************************************************/
#define MSEC_PER_USEC                                               ((TimeMSec)1000)

/***********************************************************************************************************************************
Epoch time in milliseconds
***********************************************************************************************************************************/
TimeMSec
timeMSec()
{
    FUNCTION_TEST_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_TEST_RESULT(UINT64, ((TimeMSec)currentTime.tv_sec * MSEC_PER_SEC) + (TimeMSec)currentTime.tv_usec / MSEC_PER_USEC);
}

/***********************************************************************************************************************************
Sleep for specified milliseconds
***********************************************************************************************************************************/
void
sleepMSec(TimeMSec sleepMSec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, sleepMSec);
    FUNCTION_TEST_END();

    struct timeval delay;
    delay.tv_sec = (time_t)(sleepMSec / MSEC_PER_SEC);
    delay.tv_usec = (time_t)(sleepMSec % MSEC_PER_SEC * 1000);
    select(0, NULL, NULL, NULL, &delay);

    FUNCTION_TEST_RESULT_VOID();
}
