/***********************************************************************************************************************************
Remote Command
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "config/config.h"
#include "config/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "storage/driver/remote/protocol.h"

/***********************************************************************************************************************************
Remote command
***********************************************************************************************************************************/
void
cmdRemote(int handleRead, int handleWrite)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *name = strNewFmt(PROTOCOL_SERVICE_REMOTE "-%d", cfgOptionInt(cfgOptProcess));
        IoRead *read = ioHandleReadIo(ioHandleReadNew(name, handleRead, (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * 1000)));
        ioReadOpen(read);
        IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(name, handleWrite));
        ioWriteOpen(write);

        ProtocolServer *server = protocolServerNew(name, PROTOCOL_SERVICE_REMOTE_STR, read, write);
        protocolServerHandlerAdd(server, storageDriverRemoteProtocol);
        protocolServerHandlerAdd(server, configProtocol);

        // Acquire a lock if this command needs one.  We'll use the noop that is always sent from the client right after the
        // handshake to return an error.
        volatile bool success = false;

        TRY_BEGIN()
        {
            // Read the command.  No need to parse it since we know this is the first noop.
            ioReadLine(read);

            // Only try the lock if this is process 0, i.e. the remote started from the main process
            if (cfgOptionInt(cfgOptProcess) == 0)
            {
                ConfigCommand commandId = cfgCommandId(strPtr(cfgOptionStr(cfgOptCommand)));

                // Acquire a lock if this command requires a lock
                if (cfgLockRemoteRequired(commandId))
                    lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockRemoteType(commandId), 0, true);
            }

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
            protocolServerProcess(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
