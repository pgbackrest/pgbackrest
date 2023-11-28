/***********************************************************************************************************************************
Harness for Io Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessSocket.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim install state
***********************************************************************************************************************************/
static struct
{
    // Shim clientOpen() to use a fake file descriptor
    bool localShimSckClientOpen;

    // Shim sckClientOpenWait() to return false for a name one time
    const char *clientOpenWaitName;
} hrnIoStatic;

/**********************************************************************************************************************************/
void
hrnSckClientOpenWaitShimInstall(const char *const name)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, name);
    FUNCTION_HARNESS_END();

    hrnIoStatic.clientOpenWaitName = name;

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Shim sckClientOpen()
***********************************************************************************************************************************/
static bool
sckClientOpenWait(const int fd, int errNo, const TimeMSec timeout, const char *const name)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, fd);
        FUNCTION_HARNESS_PARAM(INT, errNo);
        FUNCTION_HARNESS_PARAM(TIME_MSEC, timeout);
        FUNCTION_HARNESS_PARAM(STRINGZ, name);
    FUNCTION_HARNESS_END();

    bool result = false;

    if (hrnIoStatic.clientOpenWaitName != NULL && strcmp(hrnIoStatic.clientOpenWaitName, name) == 0)
    {
        sleepMSec(timeout);
        hrnIoStatic.clientOpenWaitName = NULL;
    }
    else
        result = sckClientOpenWait_SHIMMED(fd, errNo, timeout, name);

    FUNCTION_HARNESS_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Shim sckClientOpen()
***********************************************************************************************************************************/
IoSession *
sckClientOpen(THIS_VOID)
{
    THIS(SocketClient);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(SOCKET_CLIENT, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    IoSession *result = NULL;

    // When shim is installed create IoSession with a known fd
    if (hrnIoStatic.localShimSckClientOpen)
    {
        result = sckSessionNew(ioSessionRoleClient, HRN_SCK_FILE_DESCRIPTOR, this->host, this->port, this->timeoutSession);

        // Remove the callback so we will not try to close the fake descriptor
        memContextCallbackClear(objMemContext(((IoSessionPub *)result)->driver));
    }
    // Else call normal function
    else
        result = sckClientOpen_SHIMMED(this);

    FUNCTION_HARNESS_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
void
hrnSckClientOpenShimInstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnIoStatic.localShimSckClientOpen = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnSckClientOpenShimUninstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnIoStatic.localShimSckClientOpen = false;

    FUNCTION_HARNESS_RETURN_VOID();
}
