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
    bool localShimUnlink;
} hrnSftpStatic;

/***********************************************************************************************************************************
Shim storageSftpUnlink
***********************************************************************************************************************************/
static int
storageSftpUnlink(THIS_VOID, const String *const file)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    int result = 0;

    // Call the shim when installed
    if (hrnSftpStatic.localShimUnlink)
    {
        if (strEqZ(strBase(file), "errorOnMissing"))
        {
            result = LIBSSH2_ERROR_SOCKET_DISCONNECT;
            libssh2_session_set_last_error(this->session, LIBSSH2_ERROR_SOCKET_DISCONNECT, NULL);
        }
    }
    else
        result = storageSftpUnlink_SHIMMED(this, file);

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
void
hrnSftpUnlinkShimInstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnSftpStatic.localShimUnlink = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnSftpUnlinkShimUninstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnSftpStatic.localShimUnlink = false;

    FUNCTION_HARNESS_RETURN_VOID();
}

///***********************************************************************************************************************************
//Shim storageSftpRemove
//***********************************************************************************************************************************/
//static void
//storageSftpRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
//{
//    THIS(StorageSftp);
////fprintf(stderr, "jrt in local remove shim function\n");
//    // Call the shim when installed
//    if (hrnSftpStatic.localShimRemove)
//    {
////fprintf(stderr, "jrt in shim remove function localShim %d\n", hrnSftpStatic.localShimRemove);
//
////fprintf(stderr, "jrt path %s\n", strZ(file));
//        if (strEqZ(strBase(file), "noperm"))
//            THROW_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
///*
//        //if (strEqZ(strBase(file), "missing") && param.errorOnMissing == true)
//        if (strEqZ(strBase(file), "missing"))
//        {
//            fprintf(stderr, "jrt sleep\n");
//            // Sleep so we get a timeout error
//
//            storageSftpRemove_SHIMMED(this, file, param);
//        }
//        */
//    }
//    // Else call the base function
//    else
//    {
//        storageSftpRemove_SHIMMED(this, file, param);
//    }
//    /*
//    THIS(StorageSftp);
//
//    FUNCTION_LOG_BEGIN(logLevelTrace);
//        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
//        FUNCTION_LOG_PARAM(STRING, file);
//        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
//    FUNCTION_LOG_END();
//
//    ASSERT(this != NULL);
//    ASSERT(file != NULL);
//
//    // Attempt to unlink the file
//    int rc = 0;
//    this->wait = waitNew(this->timeoutConnect);
//
//    do
//    {
//        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
//    } while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
//
//    if (rc)
//    {
//        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
//        {
//            if (param.errorOnMissing || !storageSftpLibssh2FxNoSuchFile(this, rc))
//                storageSftpEvalLibssh2Error(
//                    rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
//                    strNewFmt("unable to remove '%s'", strZ(file)),
//                    NULL);
//            //    THROW_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
//        }
//        else
//        {
//            if (param.errorOnMissing)
//                THROW_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
//        }
//    }
//
//    FUNCTION_LOG_RETURN_VOID();
//    */
//}
///**********************************************************************************************************************************/
//void
//hrnSftpRemoveShimInstall(void)
//{
//    FUNCTION_HARNESS_VOID();
//
//    hrnSftpStatic.localShimRemove = true;
//
//    FUNCTION_HARNESS_RETURN_VOID();
//}
//
///**********************************************************************************************************************************/
//void
//hrnSftpRemoveShimUninstall(void)
//{
//    FUNCTION_HARNESS_VOID();
//
//    hrnSftpStatic.localShimRemove = false;
//
//    FUNCTION_HARNESS_RETURN_VOID();
//}


/*
static void
protocolLocalExec(
    ProtocolHelperClient *helper, ProtocolStorageType protocolStorageType, unsigned int hostIdx, unsigned int processId)
{
    // Call the shim when installed
    if (hrnProtocolStatic.localShim)
    {
        FUNCTION_LOG_BEGIN(logLevelDebug);
            FUNCTION_LOG_PARAM_P(VOID, helper);
            FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
            FUNCTION_LOG_PARAM(UINT, hostIdx);
            FUNCTION_LOG_PARAM(UINT, processId);
        FUNCTION_LOG_END();

        // Create pipes to communicate with the subprocess. The names of the pipes are from the perspective of the parent process
        // since the child process will use them only briefly before exec'ing.
        int pipeRead[2];
        int pipeWrite[2];

        THROW_ON_SYS_ERROR(pipe(pipeRead) == -1, KernelError, "unable to create read pipe");
        THROW_ON_SYS_ERROR(pipe(pipeWrite) == -1, KernelError, "unable to create write pipe");

        // Exec command in the child process
        if (forkSafe() == 0)
        {
            // Cleanup inherited clients
            hrnProtocolClientCleanup();

            // Load configuration
            StringList *const paramList = protocolLocalParam(protocolStorageType, hostIdx, processId);
            hrnCfgLoadP(cfgCmdNone, paramList, .noStd = true);

            // Change log process id to aid in debugging
            hrnLogProcessIdSet(processId);

            // Run server with provided handlers
            String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-shim-%u", processId);
            ProtocolServer *server = protocolServerNew(
                name, PROTOCOL_SERVICE_LOCAL_STR, ioFdReadNewOpen(name, pipeWrite[0], 5000),
                ioFdWriteNewOpen(name, pipeRead[1], 5000));
            protocolServerProcess(
                server, cfgCommandJobRetry(), hrnProtocolStatic.localHandlerList, hrnProtocolStatic.localHandlerListSize);

            // Exit when done
            exit(0);
        }

        // Close the unused file descriptors
        close(pipeRead[1]);
        close(pipeWrite[0]);

        // Create protocol object
        helper->client = protocolClientNew(
            strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol", processId), PROTOCOL_SERVICE_LOCAL_STR,
            ioFdReadNewOpen(strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol read", processId), pipeRead[0], 5000),
            ioFdWriteNewOpen(strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u shim protocol write", processId), pipeWrite[1], 5000));

        // Send one noop to catch any errors that might happen after the greeting
        protocolClientNoOp(helper->client);

        FUNCTION_LOG_RETURN_VOID();
    }
    // Else call the base function
    else
        protocolLocalExec_SHIMMED(helper, protocolStorageType, hostIdx, processId);
}
*/

