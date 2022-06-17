/***********************************************************************************************************************************
MSVC compatibility header for <time.h>
***********************************************************************************************************************************/
#ifndef TIME_WIN32_H
#define TIME_WIN32_H

#if defined(_WIN64) && defined(_MSC_VER)

struct tm *localtime_r(const time_t *timer, struct tm *result);

#endif

#endif
