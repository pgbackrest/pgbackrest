/***********************************************************************************************************************************
Remote Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/control/common.h"
#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "config/config.h"
#include "config/protocol.h"
#include "db/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "storage/remote/protocol.h"

/**********************************************************************************************************************************/
void
cmdRemote(int fdRead, int fdWrite)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *name = strNewFmt(PROTOCOL_SERVICE_REMOTE "-%u", cfgOptionUInt(cfgOptProcess));
        IoRead *read = ioFdReadNew(name, fdRead, cfgOptionUInt64(cfgOptProtocolTimeout));
        ioReadOpen(read);
        IoWrite *write = ioFdWriteNew(name, fdWrite, cfgOptionUInt64(cfgOptProtocolTimeout));
        ioWriteOpen(write);

        ProtocolServer *server = protocolServerNew(name, PROTOCOL_SERVICE_REMOTE_STR, read, write);
        protocolServerHandlerAdd(server, storageRemoteProtocol);
        protocolServerHandlerAdd(server, dbProtocol);
        protocolServerHandlerAdd(server, configProtocol);

        // Acquire a lock if this command needs one.  We'll use the noop that is always sent from the client right after the
        // handshake to return an error.  We can't take a lock earlier than this because we want the error to go back through the
        // protocol layer.
        volatile bool success = false;

        TRY_BEGIN()
        {
            // Read the command.  No need to parse it since we know this is the first noop.
            ioReadLine(read);

            // Only try the lock if this is process 0, i.e. the remote started from the main process
            if (cfgOptionUInt(cfgOptProcess) == 0)
            {
                // Acquire a lock if this command requires a lock
                if (cfgLockRemoteRequired())
                {
                    // Make sure the local host is not stopped
                    lockStopTest();

                    // Acquire the lock
                    lockAcquire(
                        cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgOptionStr(cfgOptExecId), cfgLockType(), 0,
                        true);
                }
            }

            // Notify the client of success
            protocolServerResponse(server, NULL);
            success = true;
        }
        CATCH_ANY()
        {
            protocolServerError(server, errorCode(), STR(errorMessage()), STR(errorStackTrace()));
        }
        TRY_END();

        // If not successful we'll just exit
        if (success)
            protocolServerProcess(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
