/***********************************************************************************************************************************
Posix Common File Routines
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_COMMON_H
#define STORAGE_DRIVER_POSIX_COMMON_H

#include "common/error.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int storageDriverPosixFileOpen(const String *name, int flags, mode_t mode, bool ignoreMissing, bool file, const char *purpose);
void storageDriverPosixFileSync(int handle, const String *name, bool file, bool closeOnError);
void storageDriverPosixFileClose(int handle, const String *name, bool file);

#endif
