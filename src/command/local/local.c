/***********************************************************************************************************************************
Local Command
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "config/config.h"
#include "config/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Remote command
***********************************************************************************************************************************/
void
cmdLocal(int handleRead, int handleWrite)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-%d", cfgOptionInt(cfgOptProcess));
        IoRead *read = ioHandleReadIo(ioHandleReadNew(name, handleRead, (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * 1000)));
        ioReadOpen(read);
        IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(name, handleWrite));
        ioWriteOpen(write);

        ProtocolServer *server = protocolServerNew(name, PROTOCOL_SERVICE_LOCAL_STR, read, write);
        protocolServerProcess(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
