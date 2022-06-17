/***********************************************************************************************************************************
MSVC compatibility header for <sys/time.h>
***********************************************************************************************************************************/

// For struct timeval
#include <WinSock2.h>

// Definition from https://man7.org/linux/man-pages/man2/gettimeofday.2.html
struct timezone {
    int tz_minuteswest;     /* minutes west of Greenwich */
    int tz_dsttime;         /* type of DST correction */
};

// timeval::tv_usec is defined as long in WinSock2.h
typedef long suseconds_t;

int gettimeofday(struct timeval *tv, struct timezone *tz);

struct tm *localtime_r(const time_t *restrict timer, struct tm *restrict result);
