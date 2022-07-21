/***********************************************************************************************************************************
MSVC compatibility header for <sys/time.h>
***********************************************************************************************************************************/

// For struct timeval
#include <WinSock2.h>

// timeval::tv_usec is defined as long in WinSock2.h
// Applied to both msvc and MingW
typedef long suseconds_t;

#if defined(_MSC_VER)

    // Definition from https://man7.org/linux/man-pages/man2/gettimeofday.2.html
    struct timezone {
        int tz_minuteswest;     /* minutes west of Greenwich */
        int tz_dsttime;         /* type of DST correction */
    };

    int gettimeofday(struct timeval *tv, struct timezone *tz);

#elif defined(__MINGW64__)

    #define gettimeofday mingw_gettimeofday

#endif
