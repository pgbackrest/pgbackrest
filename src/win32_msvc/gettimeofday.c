/***********************************************************************************************************************************
MSVC compatible implementation of gettimeofday()

Inspired by:
    https://github.com/postgres/postgres/blob/master/src/port/gettimeofday.c
***********************************************************************************************************************************/

#include "sys/time.h"

#include <Windows.h>

// FILETIME of Jan 1 1970 00:00:00
#define EPOCH 116444736000000000ULL

/*
 * FILETIME represents the number of 100-nanosecond intervals since
 * January 1, 1601 (UTC).
 */
#define FILETIME_UNITS_PER_SEC	10000000ULL
#define FILETIME_UNITS_PER_USEC 10ULL

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    // As per https://man7.org/linux/man-pages/man2/gettimeofday.2.html,
    // "The use of the timezone structure is obsolete"
    UNREFERENCED_PARAMETER(tz);

    if (!tv) {
        return -1;
    }

    FILETIME fileTime;
	ULARGE_INTEGER uLargeInt;

    // Supported from Windows 8 / Windows Server 2012
	GetSystemTimePreciseAsFileTime(&fileTime);

	uLargeInt.LowPart  = fileTime.dwLowDateTime;
	uLargeInt.HighPart = fileTime.dwHighDateTime;

	tv->tv_sec = (long) ((uLargeInt.QuadPart - EPOCH) / FILETIME_UNITS_PER_SEC);
	tv->tv_usec = (long) (((uLargeInt.QuadPart - EPOCH) % FILETIME_UNITS_PER_SEC)
						  / FILETIME_UNITS_PER_USEC);

	return 0;
}
