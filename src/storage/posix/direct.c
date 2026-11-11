/***********************************************************************************************************************************
Posix Storage Direct I/O
***********************************************************************************************************************************/
#include <build.h>

#ifndef STORAGE_POSIX_DIRECT_FLAG
#define STORAGE_POSIX_DIRECT_FLAG 0
#endif

#include <stdint.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "storage/posix/direct.h"

/**********************************************************************************************************************************/
FN_EXTERN int
storagePosixDirectFlag(void)
{
    FUNCTION_TEST_VOID();

#ifdef HAVE_O_DIRECT
    FUNCTION_TEST_RETURN(INT, STORAGE_POSIX_DIRECT_FLAG);
#else
    FUNCTION_TEST_RETURN(INT, 0);
#endif
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
storagePosixDirectNew(StoragePosixDirect *const result)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, result);
    FUNCTION_TEST_END();

    ASSERT(result != NULL);

    // Round the transfer size up to the alignment and add slack so the pointer can be aligned
    const size_t size = (ioBufferSize() + STORAGE_POSIX_DIRECT_ALIGN - 1) / STORAGE_POSIX_DIRECT_ALIGN * STORAGE_POSIX_DIRECT_ALIGN;
    Buffer *const buffer = bufNew(size + STORAGE_POSIX_DIRECT_ALIGN);

    *result = (StoragePosixDirect)
    {
        .size = size,
        .buffer = buffer,
        .ptr = (uint8_t *)(((uintptr_t)bufPtr(buffer) + STORAGE_POSIX_DIRECT_ALIGN - 1) / STORAGE_POSIX_DIRECT_ALIGN *
                           STORAGE_POSIX_DIRECT_ALIGN),
    };

    FUNCTION_TEST_RETURN(BUFFER, buffer);
}
