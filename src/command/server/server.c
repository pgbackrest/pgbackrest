/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/server/server.h"
#include "common/debug.h"
#include "common/io/tls/server.h"
#include "common/io/socket/server.h"
#include "config/config.h"

/**********************************************************************************************************************************/
void
cmdServer(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    const String *const host = STRDEF("localhost");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert), cfgOptionUInt64(cfgOptProtocolTimeout));
        IoServer *socketServer = sckServerNew(host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptProtocolTimeout));

        // do
        // {
        //     IoSession *socketSession = ioServerAccept(socketServer, NULL);
        //     (void)socketSession; // !!!
        // }
        // while (true);

        (void)tlsServer; // !!!
        (void)socketServer; // !!!

// port 8242
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
