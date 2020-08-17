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
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "config/config.h"
#include "config/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"

/**********************************************************************************************************************************/
void
cmdLocal(int fdRead, int fdWrite)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Configure two retries for local commands
        VariantList *retryInterval = varLstNew();
        varLstAdd(retryInterval, varNewUInt64(0));
        varLstAdd(retryInterval, varNewUInt64(15000));

        String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u", cfgOptionUInt(cfgOptProcess));
        IoRead *read = ioFdReadNew(name, fdRead, (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * 1000));
        ioReadOpen(read);
        IoWrite *write = ioFdWriteNew(name, fdWrite, (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * 1000));
        ioWriteOpen(write);

        ProtocolServer *server = protocolServerNew(name, PROTOCOL_SERVICE_LOCAL_STR, read, write);
        protocolServerHandlerAdd(server, archiveGetProtocol);
        protocolServerHandlerAdd(server, archivePushProtocol);
        protocolServerHandlerAdd(server, backupProtocol);
        protocolServerHandlerAdd(server, restoreProtocol);
        protocolServerHandlerAdd(server, verifyProtocol);
        protocolServerProcess(server, retryInterval);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
