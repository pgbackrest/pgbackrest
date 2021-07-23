/***********************************************************************************************************************************
Local Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/get/protocol.h"
#include "command/archive/push/protocol.h"
#include "command/backup/protocol.h"
#include "command/restore/protocol.h"
#include "command/verify/protocol.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.intern.h"
#include "config/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Command handlers
***********************************************************************************************************************************/
static const ProtocolServerHandler commandLocalHandlerList[] =
{
    PROTOCOL_SERVER_HANDLER_ARCHIVE_GET_LIST
    PROTOCOL_SERVER_HANDLER_ARCHIVE_PUSH_LIST
    PROTOCOL_SERVER_HANDLER_BACKUP_LIST
    PROTOCOL_SERVER_HANDLER_RESTORE_LIST
    PROTOCOL_SERVER_HANDLER_VERIFY_LIST
};

/**********************************************************************************************************************************/
void
cmdLocal(ProtocolServer *server)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerProcess(
            server, cfgCommandJobRetry(), commandLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandLocalHandlerList));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
