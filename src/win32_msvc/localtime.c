/***********************************************************************************************************************************
MSVC compatible implementation of gettimeofday()

Inspired by:
    https://github.com/postgres/postgres/blob/master/src/port/gettimeofday.c
***********************************************************************************************************************************/

#include "sys/time.h"

#include <time.h>

struct tm *localtime_r(const time_t *timer, struct tm *result)
{
    if (!timer || !result) {
        return NULL;
    }

    errno_t err = localtime_s(result, timer);
    if (err) {
        return NULL;
    }

    return result;
}
