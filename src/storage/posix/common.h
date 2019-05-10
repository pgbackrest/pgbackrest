/***********************************************************************************************************************************
Posix Common File Routines
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_COMMON_H
#define STORAGE_POSIX_COMMON_H

#include "common/error.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int storagePosixFileOpen(const String *name, int flags, mode_t mode, bool ignoreMissing, bool file, const char *purpose);
void storagePosixFileSync(int handle, const String *name, bool file, bool closeOnError);
void storagePosixFileClose(int handle, const String *name, bool file);

#endif
