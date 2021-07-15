/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/server/server.h"
#include "common/debug.h"
#include "common/io/tls/server.h"
#include "common/io/socket/server.h"
#include "config/config.h"
#include "config/protocol.h"
#include "db/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "storage/remote/protocol.h"

/***********************************************************************************************************************************
Command handlers
***********************************************************************************************************************************/
static const ProtocolServerHandler commandRemoteHandlerList[] =
{
    PROTOCOL_SERVER_HANDLER_DB_LIST
    PROTOCOL_SERVER_HANDLER_OPTION_LIST
    PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST
};

/**********************************************************************************************************************************/
void
cmdServer(uint64_t connectionMax)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(connectionMax > 0);

    const String *const host = STRDEF("localhost");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *const socketServer = sckServerNew(
            host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptProtocolTimeout));
        IoServer *const tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert), cfgOptionUInt64(cfgOptProtocolTimeout));

        // Accept connections until connection max is reached. !!! THIS IS A HACK TO LIMIT THE LOOP AND ALLOW TESTING. IT SHOULD BE
        // REPLACED WITH A STOP REQUEST FROM AN AUTHENTICATED CLIENT.
        do
        {
            IoSession *const socketSession = ioServerAccept(socketServer, NULL);
            IoSession *const tlsSession = ioServerAccept(tlsServer, socketSession);

            ProtocolServer *const server = protocolServerNew(
                PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(tlsSession),
                ioSessionIoWrite(tlsSession));

            // Get the command and put data end. No need to check parameters since we know this is the first noop.
            CHECK(protocolServerCommandGet(server).id == PROTOCOL_COMMAND_NOOP);
            protocolServerDataEndPut(server);

            protocolServerProcess(
                server, NULL, commandRemoteHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandRemoteHandlerList));

            ioSessionFree(tlsSession);
        }
        while (--connectionMax > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
