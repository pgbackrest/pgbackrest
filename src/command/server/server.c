/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/server/server.h"
#include "common/debug.h"
#include "common/io/tls/server.h"
#include "config/config.h"

/**********************************************************************************************************************************/
void
cmdServer(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *tlsServer = tlsServerNew(
           STRDEF("127.0.0.1"), cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert),
           cfgOptionUInt64(cfgOptProtocolTimeout));

        (void)tlsServer; // !!!

// port 8242
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
