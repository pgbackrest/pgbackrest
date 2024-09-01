/***********************************************************************************************************************************
Time Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_TIME_H
#define TEST_COMMON_HARNESS_TIME_H

#include "common/time.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Set times to be returned from timeMSec()
void hrnTimeMSecSet(const TimeMSec *timeList, size_t timeListSize);

// Sleep remainder of the current second
void hrnSleepRemainder(void);

#endif
