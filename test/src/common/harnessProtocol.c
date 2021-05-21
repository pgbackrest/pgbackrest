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

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim initialization state
***********************************************************************************************************************************/
static struct
{
    bool localHandler;
    const ProtocolServerHandler *localHandlerList;
    unsigned int localHandlerListSize;
} hrnProtocolStatic;

/***********************************************************************************************************************************
Shim protocolLocalExec() to provide coverage as detailed in the hrnProtocolLocalShimInstall() documentation.
***********************************************************************************************************************************/
static void
protocolLocalExec(
    ProtocolHelperClient *helper, ProtocolStorageType protocolStorageType, unsigned int hostIdx, unsigned int processId)
{
    // Call the shim when initialized
    if (hrnProtocolStatic.localHandler)
    {
        FUNCTION_HARNESS_BEGIN();
            FUNCTION_HARNESS_PARAM_P(VOID, helper);
            FUNCTION_HARNESS_PARAM(ENUM, protocolStorageType);
            FUNCTION_HARNESS_PARAM(UINT, hostIdx);
            FUNCTION_HARNESS_PARAM(UINT, processId);
        FUNCTION_HARNESS_END();

        // Create pipes to communicate with the subprocess. The names of the pipes are from the perspective of the parent process
        // since the child process will use them only briefly before exec'ing.
        int pipeRead[2];
        int pipeWrite[2];

        THROW_ON_SYS_ERROR(pipe(pipeRead) == -1, KernelError, "unable to create read pipe");
        THROW_ON_SYS_ERROR(pipe(pipeWrite) == -1, KernelError, "unable to create write pipe");

        // Exec command in the child process
        if (forkSafe() == 0)
        {
            // Load configuration
            const StringList *const paramList = protocolLocalParam(protocolStorageType, hostIdx, processId);
            harnessCfgLoadRaw(strLstSize(paramList), strLstPtr(paramList));

            // Change log process id to aid in debugging
            hrnLogProcessIdSet(processId);

            // Run server with provided handlers
            String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-shim-%u", processId);
            IoRead *read = ioFdReadNew(name, pipeWrite[0], 5000);
            ioReadOpen(read);
            IoWrite *write = ioFdWriteNew(name, pipeRead[1], 5000);
            ioWriteOpen(write);

            ProtocolServer *server = protocolServerNew(name, PROTOCOL_SERVICE_LOCAL_STR, read, write);
            protocolServerProcess(server, NULL, hrnProtocolStatic.localHandlerList, hrnProtocolStatic.localHandlerListSize);

            // Exit when done
            exit(0);
        }

        // Close the unused file descriptors
        close(pipeRead[1]);
        close(pipeWrite[0]);

        // Create protocol object
        IoRead *read = ioFdReadNew(strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol read", processId), pipeRead[0], 5000);
        ioReadOpen(read);
        IoWrite *write = ioFdWriteNew(strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol write", processId), pipeWrite[1], 5000);
        ioWriteOpen(write);

        helper->client = protocolClientNew(
            strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol", processId), PROTOCOL_SERVICE_LOCAL_STR, read, write);

        FUNCTION_HARNESS_RETURN_VOID();
    }
    // Else call the base function
    else
        protocolLocalExec_SHIMMED(helper, protocolStorageType, hostIdx, processId);
}

/**********************************************************************************************************************************/
void
hrnProtocolLocalShimInstall(const ProtocolServerHandler *const handlerList, const unsigned int handlerListSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM_P(VOID, handlerList);
        FUNCTION_HARNESS_PARAM(UINT, handlerListSize);
    FUNCTION_HARNESS_END();

    hrnProtocolStatic.localHandler = true;
    hrnProtocolStatic.localHandlerList = handlerList;
    hrnProtocolStatic.localHandlerListSize = handlerListSize;

    FUNCTION_HARNESS_RETURN_VOID();
}
