/***********************************************************************************************************************************
MSVC compatible implementation of gettimeofday()

Inspired by:
    https://github.com/postgres/postgres/blob/master/src/port/gettimeofday.c
***********************************************************************************************************************************/

#include "sys/time.h"

#include <Windows.h>

struct tm *localtime_r(const time_t *restrict timer, struct tm *restrict result)
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
