/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/time.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "protocol/client.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolServer
{
    ProtocolServerPub pub;                                          // Publicly accessible variables
    const String *name;
};

/**********************************************************************************************************************************/
ProtocolServer *
protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(STRING, service);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);
    ASSERT(read != NULL);
    ASSERT(write != NULL);

    ProtocolServer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ProtocolServer")
    {
        this = memNew(sizeof(ProtocolServer));

        *this = (ProtocolServer)
        {
            .pub =
            {
                .memContext = memContextCurrent(),
                .read = read,
                .write = write,
            },
            .name = strDup(name),
        };

        // Send the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            KeyValue *greetingKv = kvNew();
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_NAME_STR), VARSTRZ(PROJECT_NAME));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_SERVICE_STR), VARSTR(service));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_VERSION_STR), VARSTRZ(PROJECT_VERSION));

            ioWriteStrLine(protocolServerIoWrite(this), jsonFromKv(greetingKv));
            ioWriteFlush(protocolServerIoWrite(this));
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER, this);
}

/**********************************************************************************************************************************/
void
protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
        FUNCTION_LOG_PARAM(STRING, stack);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(code != 0);
    ASSERT(message != NULL);
    ASSERT(stack != NULL);

    KeyValue *error = kvNew();
    kvPut(error, VARSTR(PROTOCOL_ERROR_STR), VARINT(code));
    kvPut(error, VARSTR(PROTOCOL_OUTPUT_STR), VARSTR(message));
    kvPut(error, VARSTR(PROTOCOL_ERROR_STACK_STR), VARSTR(stack));

    ioWriteStrLine(protocolServerIoWrite(this), jsonFromKv(error));
    ioWriteFlush(protocolServerIoWrite(this));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(VARIANT_LIST, retryInterval);
        FUNCTION_LOG_PARAM_P(VOID, handlerList);
        FUNCTION_LOG_PARAM(UINT, handlerListSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(handlerList != NULL);
    ASSERT(handlerListSize > 0);

    // Loop until exit command is received
    bool exit = false;

    do
    {
        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Read command
                KeyValue *commandKv = jsonToKv(ioReadLine(protocolServerIoRead(this)));
                const String *command = varStr(kvGet(commandKv, VARSTR(PROTOCOL_KEY_COMMAND_STR)));
                VariantList *paramList = varVarLst(kvGet(commandKv, VARSTR(PROTOCOL_KEY_PARAMETER_STR)));

                // Find the handler
                ProtocolServerCommandHandler handler = NULL;

                for (unsigned int handlerIdx = 0; handlerIdx < handlerListSize; handlerIdx++)
                {
                    if (strEqZ(command, handlerList[handlerIdx].command))
                    {
                        handler = handlerList[handlerIdx].handler;
                        break;
                    }
                }

                // If handler was found then process
                if (handler != NULL)
                {
                    // Send the command to the handler.  Run the handler in the server's memory context in case any persistent data
                    // needs to be stored by the handler.
                    MEM_CONTEXT_BEGIN(this->pub.memContext)
                    {
                        // Initialize retries in case of command failure
                        bool retry = false;
                        unsigned int retryRemaining = retryInterval != NULL ? varLstSize(retryInterval) : 0;

                        // Handler retry loop
                        do
                        {
                            retry = false;

                            TRY_BEGIN()
                            {
                                handler(paramList, this);
                            }
                            CATCH_ANY()
                            {
                                // Are there retries remaining?
                                if (retryRemaining > 0)
                                {
                                    // Get the sleep interval for this retry
                                    TimeMSec retrySleepMs = varUInt64(
                                        varLstGet(retryInterval, varLstSize(retryInterval) - retryRemaining));

                                    // Log the retry
                                    LOG_DEBUG_FMT(
                                        "retry %s after %" PRIu64 "ms: %s", errorTypeName(errorType()), retrySleepMs,
                                        errorMessage());

                                    // Sleep if there is an interval
                                    if (retrySleepMs > 0)
                                        sleepMSec(retrySleepMs);

                                    // Decrement retries remaining and retry
                                    retryRemaining--;
                                    retry = true;

                                    // Send keep alives to remotes. A retry means the command is taking longer than usual so make
                                    // sure the remote does not timeout.
                                    protocolKeepAlive();
                                }
                                else
                                    RETHROW();
                            }
                            TRY_END();
                        }
                        while (retry);
                    }
                    MEM_CONTEXT_END();
                }
                // Else check built-in commands
                else
                {
                    if (strEq(command, PROTOCOL_COMMAND_NOOP_STR))
                        protocolServerResponse(this, NULL);
                    else if (strEq(command, PROTOCOL_COMMAND_EXIT_STR))
                        exit = true;
                    else
                        THROW_FMT(ProtocolError, "invalid command '%s'", strZ(command));
                }

                // Send keep alives to remotes.  When a local process is doing work that does not involve the remote it is important
                // that the remote does not timeout.  This will send a keep alive once per unit of work that is performed by the
                // local process.
                protocolKeepAlive();
            }
            MEM_CONTEXT_TEMP_END();
        }
        CATCH_ANY()
        {
            // Report error to the client
            protocolServerError(this, errorCode(), STR(errorMessage()), STR(errorStackTrace()));
        }
        TRY_END();
    }
    while (!exit);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolServerResponse(ProtocolServer *this, const Variant *output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(VARIANT, output);
    FUNCTION_LOG_END();

    KeyValue *result = kvNew();

    if (output != NULL)
        kvAdd(result, VARSTR(PROTOCOL_OUTPUT_STR), output);

    ioWriteStrLine(protocolServerIoWrite(this), jsonFromKv(result));
    ioWriteFlush(protocolServerIoWrite(this));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolServerWriteLine(ProtocolServer *this, const String *line)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(STRING, line);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Dot indicates the start of an lf-terminated line
    ioWrite(protocolServerIoWrite(this), DOT_BUF);

    // Write the line if it exists
    if (line != NULL)
        ioWriteStr(protocolServerIoWrite(this), line);

    // Terminate with a linefeed
    ioWrite(protocolServerIoWrite(this), LF_BUF);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
protocolServerToLog(const ProtocolServer *this)
{
    return strNewFmt("{name: %s}", strZ(this->name));
}
