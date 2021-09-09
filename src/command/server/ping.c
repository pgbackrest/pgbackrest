/***********************************************************************************************************************************
Server Ping Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/server/ping.h"
#include "common/debug.h"
#include "common/io/tls/client.h"
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
        // Connect to server without any verification
        const String *const host = STRDEF("localhost");
        const TimeMSec timeout = cfgOptionUInt64(cfgOptIoTimeout);

        IoClient *const tlsClient = tlsClientNew(
            sckClientNew(host, cfgOptionUInt(cfgOptTlsServerPort), timeout, timeout), host, timeout, timeout, false, NULL, NULL,
            NULL, NULL, NULL);
        IoSession *const tlsSession = ioClientOpen(tlsClient);

        // Send ping
        ProtocolClient *const protocolClient = protocolClientNew(
            strNewFmt(PROTOCOL_SERVICE_REMOTE " socket protocol on '%s'", strZ(host)), PROTOCOL_SERVICE_REMOTE_STR,
            ioSessionIoRead(tlsSession), ioSessionIoWrite(tlsSession));
        protocolClientNoExit(protocolClient);
        protocolClientNoOp(protocolClient);
        protocolClientFree(protocolClient);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
