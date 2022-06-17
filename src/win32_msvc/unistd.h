/***********************************************************************************************************************************
MSVC compatibility header for <unistd.h>
***********************************************************************************************************************************/
#ifndef UNISTD_WIN32_H
#define UNISTD_WIN32_H

#if defined(_WIN64) && defined(_MSC_VER)

// For io functions
#include <io.h>

// Values from https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fileno?view=msvc-170
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define write(fd, buffer, count) _write(fd, buffer, (unsigned int)count)
#define open  _open
#define close _close
#define lseek _lseek

#endif

#endif
