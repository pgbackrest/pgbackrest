/***********************************************************************************************************************************
Posix Storage Test Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_POSIX_H
#define TEST_COMMON_HARNESS_STORAGE_POSIX_H

#include <limits.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Inject a d_off == 0 cookie on the atEntry-th readdir call (1-indexed) per scan. Pass UINT_MAX for count to inject indefinitely.
void hrnStoragePosixReadDirZeroCookieSet(unsigned int atEntry, unsigned int count);

#endif
