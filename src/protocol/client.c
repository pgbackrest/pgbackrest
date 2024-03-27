/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/time.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "version.h"

/***********************************************************************************************************************************
Client state enum
***********************************************************************************************************************************/
typedef enum
{
    // Client is waiting for a command
    protocolClientStateIdle = STRID5("idle", 0x2b0890),

    // Command put is in progress
    protocolClientStateCommandPut = STRID5("cmd-put", 0x52b0d91a30),

    // Getting data from server
    protocolClientStateDataGet = STRID5("data-get", 0xa14fb0d0240),
} ProtocolClientState;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolClient
{
    ProtocolClientPub pub;                                          // Publicly accessible variables
    ProtocolClientState state;                                      // Current client state
    IoWrite *write;                                                 // Write interface
    const String *name;                                             // Name displayed in logging
    const String *errorPrefix;                                      // Prefix used when throwing error
    TimeMSec keepAliveTime;                                         // Last time data was put to the server
    uint64_t sessionTotal;                                          // Total sessions (used to generate session ids)
    List *sessionList;                                              // Session list
};

struct ProtocolClientSession
{
    ProtocolClientSessionPub pub;                                   // Publicly accessible variables
    ProtocolClient *client;                                         // Protocol client
    StringId command;                                               // Command
    uint64_t sessionId;                                             // Session id
    bool async;                                                     // Async requests allowed?

    // Stored message (read by another session and stored for later retrieval)
    bool stored;                                                    // Is a message currently stored?
    bool close;                                                     // Should the session be closed?
    ProtocolMessageType type;                                       // Type of last message received
    PackRead *packRead;                                             // Last message received (if any)
};

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientCommandPut(ProtocolClientSession *const this, const ProtocolCommandType type, PackWrite *const param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(PACK_WRITE, param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!protocolClientSessionQueued(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the command
        PackWrite *const commandPack = pckWriteNewIo(this->client->write);
        pckWriteU32P(commandPack, protocolMessageTypeCommand, .defaultWrite = true);
        pckWriteStrIdP(commandPack, this->command);
        pckWriteStrIdP(commandPack, type);
        pckWriteU64P(commandPack, this->sessionId);
        pckWriteBoolP(commandPack, this->pub.open);

        // Write parameters
        if (param != NULL)
        {
            pckWriteEndP(param);
            pckWritePackP(commandPack, pckWriteResult(param));
        }

        pckWriteEndP(commandPack);

        // Flush to send command immediately
        ioWriteFlush(this->client->write);

        this->pub.queued = true;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check protocol state
***********************************************************************************************************************************/
static void
protocolClientStateExpectIdle(const ProtocolClient *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_TEST_END();

    if (this->state != protocolClientStateIdle)
    {
        THROW_FMT(
            ProtocolError, "client state is '%s' but expected '%s'", strZ(strIdToStr(this->state)),
            strZ(strIdToStr(protocolClientStateIdle)));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Find a client session
***********************************************************************************************************************************/
static unsigned int
protocolClientSessionFindIdx(const ProtocolClient *const this, const uint64_t sessionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_TEST_PARAM(UINT64, sessionId);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    for (; result < lstSize(this->sessionList); result++)
    {
        if ((*(ProtocolClientSession **)lstGet(this->sessionList, result))->sessionId == sessionId)
            break;
    }

    CHECK_FMT(FormatError, result < lstSize(this->sessionList), "unable to find protocol client session %" PRIu64, sessionId);

    FUNCTION_TEST_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
// Helper to process errors
static void
protocolClientError(ProtocolClient *const this, const ProtocolMessageType type, PackRead *const error)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(PACK_READ, error);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (type == protocolMessageTypeError)
    {
        ASSERT(error != NULL);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            const ErrorType *const type = errorTypeFromCode(pckReadI32P(error));
            const String *const message = strNewFmt("%s: %s", strZ(this->errorPrefix), strZ(pckReadStrP(error)));
            const String *const stack = pckReadStrP(error);
            pckReadEndP(error);

            CHECK(FormatError, message != NULL && stack != NULL, "invalid error data");

            errorInternalThrow(type, __FILE__, __func__, __LINE__, strZ(message), strZ(stack));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static PackRead *
protocolClientDataGet(ProtocolClientSession *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Expect data-get state before data get
    protocolClientStateExpectIdle(this->client);

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check the session for a stored response
        bool found = false;
        ProtocolMessageType type = protocolMessageTypeData;
        bool close = false;
        PackRead *packRead = NULL;

        if (this->stored)
        {
            found = true;
            type = this->type;
            close = this->close;
            packRead = pckReadMove(this->packRead, memContextCurrent());

            this->stored = false;
        }

        // If stored then read from protocol
        while (!found)
        {
            // Switch state to data get
            this->client->state = protocolClientStateDataGet;

            PackRead *const responsePack = pckReadNewIo(this->client->pub.read);
            const uint64_t sessionId = pckReadU64P(responsePack);
            type = (ProtocolMessageType)pckReadU32P(responsePack);
            close = pckReadBoolP(responsePack);

            if (type == protocolMessageTypeError)
            {
                PackWrite *const packWrite = protocolPackNew();

                pckWriteI32P(packWrite, pckReadI32P(responsePack));
                pckWriteStrP(packWrite, pckReadStrP(responsePack));
                pckWriteStrP(packWrite, pckReadStrP(responsePack));
                pckWriteEnd(packWrite);

                packRead = pckReadNew(pckWriteResult(packWrite));
            }
            else
            {
                ASSERT(type == protocolMessageTypeData);
                ASSERT(sessionId != 0);

                packRead = pckReadPackReadP(responsePack);
            }

            pckReadEndP(responsePack);

            if (sessionId != this->sessionId)
            {
                ProtocolClientSession *const sessionOther = *(ProtocolClientSession **)lstGet(
                    this->client->sessionList, protocolClientSessionFindIdx(this->client, sessionId));

                CHECK_FMT(
                    FormatError, !sessionOther->stored, "protocol client session %" PRIu64 " already has a stored response",
                    sessionOther->sessionId);

                sessionOther->stored = true;
                sessionOther->type = type;
                sessionOther->close = close;
                sessionOther->packRead = objMove(packRead, objMemContext(sessionOther));
            }
            else
                found = true;

            // Switch state back to idle after successful data get
            this->client->state = protocolClientStateIdle;
        }

        // The result is no longer queued -- either it will generate an error or be returned to the user
        this->pub.queued = false;

        // Close the session if requested
        if (close)
            this->pub.open = false;

        // Check if this result is an error and if so throw it
        protocolClientError(this->client, type, packRead);
        CHECK(FormatError, type == protocolMessageTypeData, "expected data message");

        // Return result the the caller
        result = objMove(packRead, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/***********************************************************************************************************************************
Close protocol connection
***********************************************************************************************************************************/
static void
protocolClientFreeResource(THIS_VOID)
{
    THIS(ProtocolClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Switch state to idle so the command is sent no matter the current state
    this->state = protocolClientStateIdle;

    // Stop client sessions from sending cancel commands
    for (unsigned int sessionIdx = 0; sessionIdx < lstSize(this->sessionList); sessionIdx++)
        memContextCallbackClear(objMemContext(*(ProtocolClientSession **)lstGet(this->sessionList, sessionIdx)));

    // Send an exit command but don't wait to see if it succeeds
    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientCommandPut(protocolClientSessionNewP(this, PROTOCOL_COMMAND_EXIT), protocolCommandTypeProcess, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close client session
***********************************************************************************************************************************/
static void
protocolClientSessionFreeResource(THIS_VOID)
{
    THIS(ProtocolClientSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If a result is queued then read it before sending cancel
    if (protocolClientSessionQueued(this))
    {
        if (this->stored) // {uncovered - !!!}
            pckReadFree(this->packRead);
        else
            pckReadFree(protocolClientDataGet(this)); // {uncovered - !!!}
    }

    // If open then cancel
    if (this->pub.open)
    {
        protocolClientCommandPut(this, protocolCommandTypeCancel, NULL);
        protocolClientDataGet(this);
        ASSERT(!this->pub.open);
    }

    // Remove from session list
    lstRemoveIdx(this->client->sessionList, protocolClientSessionFindIdx(this->client, this->sessionId));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolClient *
protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write)
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

    OBJ_NEW_BEGIN(ProtocolClient, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (ProtocolClient)
        {
            .pub =
            {
                .read = read,
            },
            .state = protocolClientStateIdle,
            .write = write,
            .name = strDup(name),
            .errorPrefix = strNewFmt("raised from %s", strZ(name)),
            .keepAliveTime = timeMSec(),
            .sessionList = lstNewP(sizeof(ProtocolClientSession)),
        };

        // Read, parse, and check the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            JsonRead *const greeting = jsonReadNew(ioReadLine(this->pub.read));

            jsonReadObjectBegin(greeting);

            const struct
            {
                const StringId key;
                const char *const value;
            } expected[] =
            {
                {.key = PROTOCOL_GREETING_NAME, .value = PROJECT_NAME},
                {.key = PROTOCOL_GREETING_SERVICE, .value = strZ(service)},
                {.key = PROTOCOL_GREETING_VERSION, .value = PROJECT_VERSION},
            };

            for (unsigned int expectedIdx = 0; expectedIdx < LENGTH_OF(expected); expectedIdx++)
            {
                if (!jsonReadKeyExpectStrId(greeting, expected[expectedIdx].key))
                    THROW_FMT(ProtocolError, "unable to find greeting key '%s'", strZ(strIdToStr(expected[expectedIdx].key)));

                if (jsonReadTypeNext(greeting) != jsonTypeString)
                    THROW_FMT(ProtocolError, "greeting key '%s' must be string type", strZ(strIdToStr(expected[expectedIdx].key)));

                const String *const actualValue = jsonReadStr(greeting);

                if (!strEqZ(actualValue, expected[expectedIdx].value))
                {
                    THROW_FMT(
                        ProtocolError,
                        "expected value '%s' for greeting key '%s' but got '%s'\n"
                        "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?",
                        expected[expectedIdx].value, strZ(strIdToStr(expected[expectedIdx].key)), strZ(actualValue));
                }
            }

            jsonReadObjectEnd(greeting);
        }
        MEM_CONTEXT_TEMP_END();

        // Set a callback to shutdown the protocol
        memContextCallbackSet(objMemContext(this), protocolClientFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolClientSession *
protocolClientSessionNew(ProtocolClient *const client, const StringId command, const ProtocolClientSessionNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING_ID, command);
        FUNCTION_LOG_PARAM(BOOL, param.async);
    FUNCTION_LOG_END();

    ASSERT(client != NULL);

    OBJ_NEW_BEGIN(ProtocolClientSession, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (ProtocolClientSession)
        {
            .client = client,
            .command = command,
            .sessionId = ++client->sessionTotal,
            .async = param.async,
        };
    }
    OBJ_NEW_END();

    // If async then the session will need to be tracked in case another session gets a result for this session
    if (this->async)
    {
        lstAdd(this->client->sessionList, &this);

        // Set a callback to cleanup the session
        memContextCallbackSet(objMemContext(this), protocolClientSessionFreeResource, this);
    }

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT_SESSION, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientSessionOpen(ProtocolClientSession *const this, const ProtocolClientSessionOpenParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.open);

    protocolClientCommandPut(this, protocolCommandTypeOpen, param.param);
    this->pub.open = true;

    // Set a callback to cleanup the session if it was not already done for async
    if (!this->async)
    {
        lstAdd(this->client->sessionList, &this);
        memContextCallbackSet(objMemContext(this), protocolClientSessionFreeResource, this);
    }

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientDataGet(this));
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientSessionRequest(ProtocolClientSession *const this, const ProtocolClientSessionRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientCommandPut(this, protocolCommandTypeProcess, param.param);

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientDataGet(this));
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientSessionRequestAsync(ProtocolClientSession *const this, const ProtocolClientSessionRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->async);

    protocolClientCommandPut(this, protocolCommandTypeProcess, param.param);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientSessionResponse(ProtocolClientSession *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->async);

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientDataGet(this));
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientSessionClose(ProtocolClientSession *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.open);

    protocolClientCommandPut(this, protocolCommandTypeClose, NULL);
    PackRead *const result = protocolClientDataGet(this);
    ASSERT(!this->pub.open);

    memContextCallbackClear(objMemContext(this));
    protocolClientSessionFreeResource(this);

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientSessionCancel(ProtocolClientSession *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    memContextCallbackClear(objMemContext(this));
    protocolClientSessionFreeResource(this);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientRequest(ProtocolClient *const this, const StringId command, const ProtocolClientSessionRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING_ID, command);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolClientSession *const session = protocolClientSessionNewP(this, command);

        protocolClientCommandPut(session, protocolCommandTypeProcess, param.param);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = protocolClientDataGet(session);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientNoOp(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientRequestP(this, PROTOCOL_COMMAND_NOOP);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientToLog(const ProtocolClient *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{name: %s, state: ", strZ(this->name));
    strStcResultSizeInc(debugLog, strIdToLog(this->state, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcCatChr(debugLog, '}');
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientSessionToLog(const ProtocolClientSession *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{client: ");
    protocolClientToLog(this->client, debugLog);
    strStcCat(debugLog, ", command: ");
    strStcResultSizeInc(debugLog, strIdToLog(this->command, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcFmt(
        debugLog, ", sessionId: %" PRIu64 ", queued %s, stored: %s", this->sessionId,
        cvtBoolToConstZ(protocolClientSessionQueued(this)), cvtBoolToConstZ(this->stored));
    strStcCatChr(debugLog, '}');
}
