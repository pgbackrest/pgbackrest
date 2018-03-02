/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#include <sys/time.h>

#include "common/time.h"
#include "common/type.h"

/***********************************************************************************************************************************
Epoch time in microseconds
***********************************************************************************************************************************/
TimeUSec
timeUSec()
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return ((TimeUSec)currentTime.tv_sec * USEC_PER_SEC) + (TimeUSec)currentTime.tv_usec;
}

/***********************************************************************************************************************************
Sleep for specified microseconds
***********************************************************************************************************************************/
void
sleepUSec(TimeUSec sleepUSec)
{
    struct timeval delay;
    delay.tv_sec = (__time_t)(sleepUSec / USEC_PER_SEC);
    delay.tv_usec = (__time_t)(sleepUSec % USEC_PER_SEC);
    select(0, NULL, NULL, NULL, &delay);
}
