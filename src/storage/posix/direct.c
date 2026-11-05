/***********************************************************************************************************************************
Posix Storage Direct I/O
***********************************************************************************************************************************/
#include <build.h>

#ifndef STORAGE_POSIX_DIRECT_FLAG
#define STORAGE_POSIX_DIRECT_FLAG 0
#endif

#include <stdint.h>

#ifdef HAVE_STATX_DIOALIGN
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "storage/posix/direct.h"

#ifdef HAVE_STATX_DIOALIGN
/**********************************************************************************************************************************/
static size_t
storagePosixDirectAlign(const bool statxSuccess, const uint32_t mask, const uint32_t memoryAlign, const uint32_t offsetAlign)
{
    if (statxSuccess && (mask & STATX_DIOALIGN) != 0 && memoryAlign != 0 && offsetAlign != 0)
        return memoryAlign > offsetAlign ? memoryAlign : offsetAlign;

    return STORAGE_POSIX_DIRECT_ALIGN_DEFAULT;
}
#endif

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
storagePosixDirectNew(const int fd, StoragePosixDirect *const result)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, fd);
        FUNCTION_TEST_PARAM_P(VOID, result);
    FUNCTION_TEST_END();

    ASSERT(fd != -1);
    ASSERT(result != NULL);

    size_t align = STORAGE_POSIX_DIRECT_ALIGN_DEFAULT;

#ifdef HAVE_STATX_DIOALIGN
    struct statx info = {0};
    const bool statxSuccess = statx(fd, "", AT_EMPTY_PATH, STATX_DIOALIGN, &info) == 0;

    align = storagePosixDirectAlign(statxSuccess, info.stx_mask, info.stx_dio_mem_align, info.stx_dio_offset_align);
#else
    (void)fd;
#endif

    // Round the transfer size up to the alignment and add slack so the pointer can be aligned
    const size_t size = (ioBufferSize() + align - 1) / align * align;
    Buffer *const buffer = bufNew(size + align);

    *result = (StoragePosixDirect)
    {
        .align = align,
        .size = size,
        .buffer = buffer,
        .ptr = (uint8_t *)(((uintptr_t)bufPtr(buffer) + align - 1) / align * align),
    };

    FUNCTION_TEST_RETURN(BUFFER, buffer);
}
