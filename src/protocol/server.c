/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/time.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolServer
{
    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
    const String *name;                                             // Name displayed in logging
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

    OBJ_NEW_BEGIN(ProtocolServer)
    {
        this = OBJ_NEW_ALLOC();

        *this = (ProtocolServer)
        {
            .read = read,
            .write = write,
            .name = strDup(name),
        };

        // Send the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            KeyValue *greetingKv = kvNew();
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_NAME_STR), VARSTRZ(PROJECT_NAME));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_SERVICE_STR), VARSTR(service));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_VERSION_STR), VARSTRZ(PROJECT_VERSION));

            ioWriteStrLine(this->write, jsonFromKv(greetingKv));
            ioWriteFlush(this->write);
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the error and flush to be sure it gets sent immediately
        PackWrite *error = pckWriteNewIo(this->write);
        pckWriteU32P(error, protocolMessageTypeError);
        pckWriteI32P(error, code);
        pckWriteStrP(error, message);
        pckWriteStrP(error, stack);
        pckWriteEndP(error);

        ioWriteFlush(this->write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
ProtocolServerCommandGetResult
protocolServerCommandGet(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    ProtocolServerCommandGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const command = pckReadNewIo(this->read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(command);

        CHECK(type == protocolMessageTypeCommand);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.id = pckReadStrIdP(command);
            result.param = pckReadPackP(command);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(command);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
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
                // Get command
                ProtocolServerCommandGetResult command = protocolServerCommandGet(this);

                // Find the handler
                ProtocolServerCommandHandler handler = NULL;

                for (unsigned int handlerIdx = 0; handlerIdx < handlerListSize; handlerIdx++)
                {
                    if (command.id == handlerList[handlerIdx].command)
                    {
                        handler = handlerList[handlerIdx].handler;
                        break;
                    }
                }

                // If handler was found then process
                if (handler != NULL)
                {
                    // Send the command to the handler
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        // Variables to store first error message and retry messages
                        const ErrorType *errType = NULL;
                        String *errMessage = NULL;
                        const String *errMessageFirst = NULL;
                        const String *errStackTrace = NULL;

                        // Initialize retries in case of command failure
                        bool retry = false;
                        unsigned int retryRemaining = retryInterval != NULL ? varLstSize(retryInterval) : 0;
                        TimeMSec retrySleepMs = 0;

                        // Handler retry loop
                        do
                        {
                            retry = false;

                            TRY_BEGIN()
                            {
                                handler(pckReadNew(command.param), this);
                            }
                            CATCH_ANY()
                            {
                                // On first error record the error details. Only the first error will contain a stack trace since
                                // the first error is most likely to contain valuable information.
                                if (errType == NULL)
                                {
                                    errType = errorType();
                                    errMessage = strCatZ(strNew(), errorMessage());
                                    errMessageFirst = strNewZ(errorMessage());
                                    errStackTrace = strNewZ(errorStackTrace());
                                }
                                // Else on a retry error only record the error type and message. Retry errors are less likely to
                                // contain valuable information but may be helpful for debugging.
                                else
                                {
                                    strCatFmt(
                                        errMessage, "\n[%s] on retry after %" PRIu64 "ms", errorTypeName(errorType()),
                                        retrySleepMs);

                                    // Only append the message if it differs from the first message
                                    if (!strEqZ(errMessageFirst, errorMessage()))
                                        strCatFmt(errMessage, ": %s", errorMessage());
                                }

                                // Are there retries remaining?
                                if (retryRemaining > 0)
                                {
                                    // Get the sleep interval for this retry
                                    retrySleepMs = varUInt64(varLstGet(retryInterval, varLstSize(retryInterval) - retryRemaining));

                                    // Log the retry
                                    LOG_DEBUG_FMT(
                                        "retry %s after %" PRIu64 "ms: %s", errorTypeName(errorType()), retrySleepMs,
                                        errorMessage());

                                    // Sleep for interval
                                    sleepMSec(retrySleepMs);

                                    // Decrement retries remaining and retry
                                    retryRemaining--;
                                    retry = true;

                                    // Send keep-alive to remotes. A retry means the command is taking longer than usual so make
                                    // sure the remote does not timeout.
                                    protocolKeepAlive();
                                }
                                // Else report error to the client
                                else
                                    protocolServerError(this, errorTypeCode(errType), errMessage, errStackTrace);
                            }
                            TRY_END();
                        }
                        while (retry);
                    }
                    MEM_CONTEXT_TEMP_END();
                }
                // Else check built-in commands
                else
                {
                    switch (command.id)
                    {
                        case PROTOCOL_COMMAND_EXIT:
                            exit = true;
                            break;

                        case PROTOCOL_COMMAND_NOOP:
                            protocolServerDataEndPut(this);
                            break;

                        default:
                            THROW_FMT(
                                ProtocolError, "invalid command '%s' (0x%" PRIx64 ")", strZ(strIdToStr(command.id)), command.id);
                    }
                }

                // Send keep-alive to remotes. When a local process is doing work that does not involve the remote it is important
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

            // Rethrow so the process exits with an error
            RETHROW();
        }
        TRY_END();
    }
    while (!exit);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackRead *
protocolServerDataGet(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *data = pckReadNewIo(this->read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(data);

        CHECK(type == protocolMessageTypeData);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = pckReadPackReadP(data);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
void
protocolServerDataPut(ProtocolServer *const this, PackWrite *const data)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, data);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // End the pack
        if (data != NULL)
            pckWriteEndP(data);

        // Write the result
        PackWrite *resultMessage = pckWriteNewIo(this->write);
        pckWriteU32P(resultMessage, protocolMessageTypeData, .defaultWrite = true);
        pckWritePackP(resultMessage, pckWriteResult(data));
        pckWriteEndP(resultMessage);

        // Flush on NULL result since it might be used to synchronize
        if (data == NULL)
            ioWriteFlush(this->write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolServerDataEndPut(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the response and flush to be sure it gets sent immediately
        PackWrite *response = pckWriteNewIo(this->write);
        pckWriteU32P(response, protocolMessageTypeDataEnd, .defaultWrite = true);
        pckWriteEndP(response);
    }
    MEM_CONTEXT_TEMP_END();

    ioWriteFlush(this->write);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
protocolServerToLog(const ProtocolServer *this)
{
    return strNewFmt("{name: %s}", strZ(this->name));
}
