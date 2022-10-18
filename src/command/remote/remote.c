/***********************************************************************************************************************************
Remote Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/pageChecksum.h"
#include "command/control/common.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/sink.h"
#include "common/io/filter/size.h"
#include "common/log.h"
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

/***********************************************************************************************************************************
Filter handlers
***********************************************************************************************************************************/
static const StorageRemoteFilterHandler storageRemoteFilterHandlerList[] =
{
    {.type = CIPHER_BLOCK_FILTER_TYPE, .handlerParam = cipherBlockNewPack},
    {.type = CRYPTO_HASH_FILTER_TYPE, .handlerParam = cryptoHashNewPack},
    {.type = PAGE_CHECKSUM_FILTER_TYPE, .handlerParam = pageChecksumNewPack},
    {.type = SINK_FILTER_TYPE, .handlerNoParam = ioSinkNew},
    {.type = SIZE_FILTER_TYPE, .handlerNoParam = ioSizeNew},
};

/**********************************************************************************************************************************/
void
cmdRemote(ProtocolServer *const server)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Set filter handlers
    storageRemoteFilterHandlerSet(storageRemoteFilterHandlerList, LENGTH_OF(storageRemoteFilterHandlerList));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Acquire a lock if this command needs one.  We'll use the noop that is always sent from the client right after the
        // handshake to return an error.  We can't take a lock earlier than this because we want the error to go back through the
        // protocol layer.
        volatile bool success = false;

        TRY_BEGIN()
        {
            // Get the command. No need to check parameters since we know this is the first noop.
            CHECK(FormatError, protocolServerCommandGet(server).id == PROTOCOL_COMMAND_NOOP, "noop expected");

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
            protocolServerDataEndPut(server);
            success = true;
        }
        CATCH_ANY()
        {
            protocolServerError(server, errorCode(), STR(errorMessage()), STR(errorStackTrace()));
        }
        TRY_END();

        // If not successful we'll just exit
        if (success)
            protocolServerProcess(server, NULL, commandRemoteHandlerList, LENGTH_OF(commandRemoteHandlerList));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
