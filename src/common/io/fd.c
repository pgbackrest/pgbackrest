/***********************************************************************************************************************************
File Descriptor Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef __sun__                                                      // Illumos needs sys/siginfo for sigset_t inside poll.h
#include <sys/siginfo.h>
#endif
#include <poll.h>

#include "common/debug.h"
#include "common/io/fd.h"
#include "common/log.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Use poll() to determine when data is ready to read/write on a socket. Retry after EINTR with whatever time is left on the timer.
***********************************************************************************************************************************/
// Helper to determine when poll() should be retried
static bool
fdReadyRetry(int pollResult, int errNo, bool first, TimeMSec *timeout, TimeMSec timeEnd)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, pollResult);
        FUNCTION_TEST_PARAM(INT, errNo);
        FUNCTION_TEST_PARAM(BOOL, first);
        FUNCTION_TEST_PARAM_P(TIME_MSEC, timeout);
        FUNCTION_TEST_PARAM(TIME_MSEC, timeEnd);
    FUNCTION_TEST_END();

    ASSERT(timeout != NULL);

    // No retry by default
    bool result = false;

    // Process errors
    if (pollResult == -1)
    {
        // Don't error on an interrupt. If the interrupt lasts long enough there may be a timeout, though.
        if (errNo != EINTR)
            THROW_SYS_ERROR_CODE(errNo, KernelError, "unable to poll socket");

        // Always retry on the first iteration
        if (first)
        {
            result = true;
        }
        // Else retry if there is time left
        else
        {
            TimeMSec timeCurrent = timeMSec();

            if (timeEnd > timeCurrent)
            {
                *timeout = timeEnd - timeCurrent;
                result = true;
            }
        }
    }

    FUNCTION_TEST_RETURN(result);
}

bool
fdReady(int fd, bool read, bool write, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(BOOL, read);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd >= 0);
    ASSERT(read || write);
    ASSERT(timeout < INT_MAX);

    // Poll settings
    struct pollfd inputFd = {.fd = fd};

    if (read)
        inputFd.events |= POLLIN;

    if (write)
        inputFd.events |= POLLOUT;

    // Wait for ready or timeout
    TimeMSec timeEnd = timeMSec() + timeout;
    bool first = true;

    // Initialize result and errno to look like a retryable error. We have no good way to test this function with interrupts so this
    // at least ensures that the condition is retried.
    int result = -1;
    int errNo = EINTR;

    while (fdReadyRetry(result, errNo, first, &timeout, timeEnd))
    {
        result = poll(&inputFd, 1, (int)timeout);

        errNo = errno;
        first = false;
    }

    FUNCTION_LOG_RETURN(BOOL, result > 0);
}
