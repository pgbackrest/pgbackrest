/***********************************************************************************************************************************
Storage File Routines For Posix
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_DRIVERFILE_H
#define STORAGE_DRIVER_POSIX_DRIVERFILE_H

#include "common/error.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int storageFilePosixOpen(const String *name, int flags, mode_t mode, bool ignoreMissing, bool file, const char *purpose);
void storageFilePosixSync(int handle, const String *name, bool file, bool closeOnError);
void storageFilePosixClose(int handle, const String *name, bool file);

#endif
