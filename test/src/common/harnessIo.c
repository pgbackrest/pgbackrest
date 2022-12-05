/***********************************************************************************************************************************
Harness for Protocol Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/fork.h"

#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessProtocol.h"
#include "common/wait.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim install state
***********************************************************************************************************************************/
static struct
{
    // Local process shims
    bool localShimIoSessionFd;
} hrnIoStatic;

/***********************************************************************************************************************************
Shim ioSessionFd
***********************************************************************************************************************************/
int ioSessionFd(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int fd = 0;

    // Call the shim when installed, return the value of 63581
    if (hrnIoStatic.localShimIoSessionFd)
        fd = 63581;
    else
        fd = ioSessionFd_SHIMMED(this);

    FUNCTION_TEST_RETURN(INT, fd);
}

/**********************************************************************************************************************************/
void hrnIoIoSessionFdShimInstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnIoStatic.localShimIoSessionFd = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void hrnIoIoSessionFdShimUninstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnIoStatic.localShimIoSessionFd = false;

    FUNCTION_HARNESS_RETURN_VOID();
}
