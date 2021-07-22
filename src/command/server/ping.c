/***********************************************************************************************************************************
Server Ping Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/server/ping.h"
#include "common/debug.h"
#include "common/io/socket/client.h"
#include "config/config.h"
#include "protocol/client.h"
#include "protocol/helper.h"

void
cmdServerPing(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const host = STRDEF("localhost");

        // Connect to server
        IoClient *socketClient = sckClientNew(host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptIoTimeout));
        IoSession *socketSession = ioClientOpen(socketClient);

        // Send ping
        ProtocolClient *protocolClient = protocolClientNew(
            strNewFmt(PROTOCOL_SERVICE_REMOTE " socket protocol on '%s'", strZ(host)), PROTOCOL_SERVICE_REMOTE_STR,
            ioSessionIoRead(socketSession), ioSessionIoWrite(socketSession));
        protocolClientNoExit(protocolClient);
        protocolClientNoOp(protocolClient);
        protocolClientFree(protocolClient);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
