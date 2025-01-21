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
    // Client is waiting for a response / ready to send a request
    protocolClientStateIdle = STRID5("idle", 0x2b0890),

    // Request is in progress
    protocolClientStateRequest = STRID5("request", 0x5265ac4b20),

    // Response is in progress
    protocolClientStateResponse = STRID5("response", 0x2cdcf84cb20),
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

    CHECK_FMT(ProtocolError, result < lstSize(this->sessionList), "unable to find protocol client session %" PRIu64, sessionId);

    FUNCTION_TEST_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
static void
protocolClientRequestInternal(ProtocolClientSession *const this, const ProtocolCommandType type, PackWrite *const param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(PACK_WRITE, param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!protocolClientSessionQueued(this));

    // Expect idle state before request
    protocolClientStateExpectIdle(this->client);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Switch state to request
        this->client->state = protocolClientStateRequest;

        // Write request
        PackWrite *const request = pckWriteNewIo(this->client->write);
        pckWriteU32P(request, protocolMessageTypeRequest, .defaultWrite = true);
        pckWriteStrIdP(request, this->command);
        pckWriteStrIdP(request, type);
        pckWriteU64P(request, this->sessionId);
        pckWriteBoolP(request, this->pub.open);

        // Write request parameters
        if (param != NULL)
        {
            pckWriteEndP(param);
            pckWritePackP(request, pckWriteResult(param));
        }

        pckWriteEndP(request);

        // Flush to send request immediately
        ioWriteFlush(this->client->write);

        this->pub.queued = true;

        // Switch state back to idle after successful request
        this->client->state = protocolClientStateIdle;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Process errors
***********************************************************************************************************************************/
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
protocolClientResponseInternal(ProtocolClientSession *const this)
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
        ProtocolMessageType type = protocolMessageTypeResponse;
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

        // If not stored then read from protocol
        while (!found)
        {
            // Switch state to response
            this->client->state = protocolClientStateResponse;

            // Read response
            PackRead *const response = pckReadNewIo(this->client->pub.read);
            const uint64_t sessionId = pckReadU64P(response);
            type = (ProtocolMessageType)pckReadU32P(response);
            close = pckReadBoolP(response);
            packRead = pckReadPackReadP(response);
            pckReadEndP(response);

            // If this response is for another session then store it with that session. Session id 0 indicates a fatal error on the
            // server that should be reported by the first session that sees it.
            ASSERT(sessionId != 0 || type == protocolMessageTypeError);

            if (sessionId != 0 && sessionId != this->sessionId)
            {
                ProtocolClientSession *const sessionOther = *(ProtocolClientSession **)lstGet(
                    this->client->sessionList, protocolClientSessionFindIdx(this->client, sessionId));

                CHECK_FMT(
                    ProtocolError, !sessionOther->stored, "protocol client session %" PRIu64 " already has a stored response",
                    sessionOther->sessionId);

                sessionOther->stored = true;
                sessionOther->type = type;
                sessionOther->close = close;
                sessionOther->packRead = objMove(packRead, objMemContext(sessionOther));
            }
            else
                found = true;

            // Switch state back to idle after successful response
            this->client->state = protocolClientStateIdle;
        }

        // The result is no longer queued -- either it will generate an error or be returned to the user
        this->pub.queued = false;

        // Close the session if requested
        if (close)
            this->pub.open = false;

        // Check if this result is an error and if so throw it
        protocolClientError(this->client, type, packRead);
        CHECK(FormatError, type == protocolMessageTypeResponse, "expected response message");

        // Return result to the caller
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

    // Stop client sessions from sending cancel requests
    for (unsigned int sessionIdx = 0; sessionIdx < lstSize(this->sessionList); sessionIdx++)
        memContextCallbackClear(objMemContext(*(ProtocolClientSession **)lstGet(this->sessionList, sessionIdx)));

    // Send an exit request but don't wait to see if it succeeds
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Switch state to idle so the request is sent no matter the current state
        this->state = protocolClientStateIdle;

        // Send exit request
        protocolClientRequestInternal(protocolClientSessionNewP(this, PROTOCOL_COMMAND_EXIT), protocolCommandTypeProcess, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolClient *
protocolClientNew(const String *const name, const String *const service, IoRead *const read, IoWrite *const write)
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
FN_EXTERN PackRead *
protocolClientRequest(ProtocolClient *const this, const StringId command, const ProtocolClientRequestParam param)
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

        protocolClientRequestInternal(session, protocolCommandTypeProcess, param.param);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = protocolClientResponseInternal(session);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientNoOp(ProtocolClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientRequestP(this, PROTOCOL_COMMAND_NOOP);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Free client session
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
        // Free stored result
        if (this->stored)
        {
            pckReadFree(this->packRead);
        }
        // Else read and free result
        else
            pckReadFree(protocolClientResponseInternal(this));
    }

    // If open then cancel
    if (this->pub.open)
    {
        protocolClientRequestInternal(this, protocolCommandTypeCancel, NULL);
        protocolClientResponseInternal(this);
        ASSERT(!this->pub.open);
    }

    // Remove from session list
    lstRemoveIdx(this->client->sessionList, protocolClientSessionFindIdx(this->client, this->sessionId));

    FUNCTION_LOG_RETURN_VOID();
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

    protocolClientRequestInternal(this, protocolCommandTypeOpen, param.param);
    this->pub.open = true;

    // Set a callback to cleanup the session if it was not already done for async
    if (!this->async)
    {
        lstAdd(this->client->sessionList, &this);
        memContextCallbackSet(objMemContext(this), protocolClientSessionFreeResource, this);
    }

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientResponseInternal(this));
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientSessionRequest(ProtocolClientSession *const this, const ProtocolClientRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientRequestInternal(this, protocolCommandTypeProcess, param.param);

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientResponseInternal(this));
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientSessionRequestAsync(ProtocolClientSession *const this, const ProtocolClientRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT_SESSION, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, param.param);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->async);

    protocolClientRequestInternal(this, protocolCommandTypeProcess, param.param);

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

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientResponseInternal(this));
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

    protocolClientRequestInternal(this, protocolCommandTypeClose, NULL);
    PackRead *const result = protocolClientResponseInternal(this);
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
