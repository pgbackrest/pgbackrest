/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error/retry.h"
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
    List *sessionList;                                              // List of active sessions
    uint64_t sessionId;                                             // Current session being processed
};

struct ProtocolServerResult
{
    void *sessionData;                                              // Session data
    PackWrite *data;                                                // Result data
    size_t extra;                                                   // Extra bytes for initializing data
    bool close;                                                     // Close session?
};

// Track server sessions
typedef struct ProtocolServerSession
{
    uint64_t id;                                                    // Session id
    void *data;                                                     // Data for the session
} ProtocolServerSession;

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServer *
protocolServerNew(const String *const name, const String *const service, IoRead *const read, IoWrite *const write)
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

    OBJ_NEW_BEGIN(ProtocolServer, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ProtocolServer)
        {
            .read = read,
            .write = write,
            .name = strDup(name),
            .sessionList = lstNewP(sizeof(ProtocolServerSession)),
        };

        // Send the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

            jsonWriteZ(jsonWriteKeyStrId(json, PROTOCOL_GREETING_NAME), PROJECT_NAME);
            jsonWriteStr(jsonWriteKeyStrId(json, PROTOCOL_GREETING_SERVICE), service);
            jsonWriteZ(jsonWriteKeyStrId(json, PROTOCOL_GREETING_VERSION), PROJECT_VERSION);

            ioWriteStrLine(this->write, jsonWriteResult(jsonWriteObjectEnd(json)));
            ioWriteFlush(this->write);
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerResponse(ProtocolServer *const this, const ProtocolServerResponseParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_TEST_PARAM(ENUM, param.type);
        FUNCTION_TEST_PARAM(BOOL, param.close);
        FUNCTION_TEST_PARAM(PACK_WRITE, param.data);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // End the pack
        if (param.data != NULL)
            pckWriteEndP(param.data);

        // Write the result
        PackWrite *const resultMessage = pckWriteNewIo(this->write);

        pckWriteU64P(resultMessage, this->sessionId);
        pckWriteU32P(resultMessage, param.type, .defaultWrite = true);
        pckWriteBoolP(resultMessage, param.close);
        pckWritePackP(resultMessage, pckWriteResult(param.data));
        pckWriteEndP(resultMessage);
        ioWriteFlush(this->write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerError(ProtocolServer *this, const int code, const String *const message, const String *const stack)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
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
        PackWrite *const packWrite = protocolPackNew();

        pckWriteI32P(packWrite, code);
        pckWriteStrP(packWrite, message);
        pckWriteStrP(packWrite, stack);

        protocolServerResponseP(this, .type = protocolMessageTypeError, .data = packWrite);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerRequestResult
protocolServerRequest(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ProtocolServerRequestResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const request = pckReadNewIo(this->read);
        const ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(request);

        CHECK(FormatError, type == protocolMessageTypeRequest, "expected request message");

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.id = pckReadStrIdP(request);
            result.type = (ProtocolCommandType)pckReadStrIdP(request);
            this->sessionId = pckReadU64P(request);
            result.sessionRequired = pckReadBoolP(request);
            result.param = pckReadPackP(request);

            ASSERT(this->sessionId != 0);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(request);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerProcess(
    ProtocolServer *const this, const VariantList *const retryInterval, const ProtocolServerHandler *const handlerList,
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

    // Loop until exit request is received
    bool exit = false;

    do
    {
        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Get request
                ProtocolServerRequestResult request = protocolServerRequest(this);

                // Find the handler
                const ProtocolServerHandler *handler = NULL;

                for (unsigned int handlerIdx = 0; handlerIdx < handlerListSize; handlerIdx++)
                {
                    if (request.id == handlerList[handlerIdx].command)
                    {
                        handler = &handlerList[handlerIdx];
                        break;
                    }
                }

                // If handler was found then process
                if (handler != NULL)
                {
                    // Send the request to the handler
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        // Variables to store first error message and retry messages
                        ErrorRetry *const errRetry = errRetryNew();
                        const String *errStackTrace = NULL;

                        // Initialize retries in case of request failure
                        bool retry = false;
                        unsigned int retryRemaining = retryInterval != NULL ? varLstSize(retryInterval) : 0;
                        TimeMSec retrySleepMs = 0;

                        // Handler retry loop
                        do
                        {
                            retry = false;

                            TRY_BEGIN()
                            {
                                // Process request type
                                switch (request.type)
                                {
                                    // Open a protocol session
                                    case protocolCommandTypeOpen:
                                    {
                                        ASSERT(handler->open != NULL);

                                        // Call open handler
                                        ProtocolServerResult *const openResult = handler->open(pckReadNew(request.param));
                                        ASSERT(openResult != NULL);
                                        ASSERT(!openResult->close);

                                        ProtocolServerSession session = {.id = this->sessionId};

                                        // Send data
                                        protocolServerResponseP(
                                            this, .data = openResult->data, .close = openResult->sessionData == NULL);

                                        // Create session if open returned session data
                                        if (openResult->sessionData != NULL)
                                        {
                                            session.data = objMove(openResult->sessionData, objMemContext(this->sessionList));
                                            lstAdd(this->sessionList, &session);
                                        }

                                        break;
                                    }

                                    // Process or close/cancel protocol session
                                    default:
                                    {
                                        // Find session data
                                        void *sessionData = NULL;
                                        unsigned int sessionListIdx = 0;

                                        if (request.sessionRequired)
                                        {
                                            ASSERT(handler->processSession != NULL);
                                            ASSERT(handler->process == NULL);

                                            for (; sessionListIdx < lstSize(this->sessionList); sessionListIdx++)
                                            {
                                                ProtocolServerSession *const session = lstGet(this->sessionList, sessionListIdx);

                                                if (session->id == this->sessionId)
                                                {
                                                    sessionData = session->data;
                                                    break;
                                                }
                                            }

                                            // Error when session not found
                                            if (sessionData == NULL && request.type != protocolCommandTypeCancel)
                                            {
                                                THROW_FMT(
                                                    ProtocolError, "unable to find session id %" PRIu64 " for request %s:%s",
                                                    this->sessionId, strZ(strIdToStr(request.id)),
                                                    strZ(strIdToStr(request.type)));
                                            }
                                        }

                                        // Process request type
                                        switch (request.type)
                                        {
                                            // Process request
                                            case protocolCommandTypeProcess:
                                            {
                                                // Process session
                                                ProtocolServerResult *processResult;

                                                if (handler->processSession != NULL)
                                                {
                                                    ASSERT(handler->process == NULL);
                                                    CHECK_FMT(
                                                        ProtocolError, this->sessionId != 0, "no session id for request %s:%s",
                                                        strZ(strIdToStr(request.id)), strZ(strIdToStr(request.type)));

                                                    processResult = handler->processSession(pckReadNew(request.param), sessionData);
                                                }
                                                // Standalone process
                                                else
                                                {
                                                    ASSERT(handler->process != NULL);
                                                    ASSERT(!request.sessionRequired);

                                                    processResult = handler->process(pckReadNew(request.param));
                                                }

                                                if (processResult == NULL)
                                                    protocolServerResponseP(this);
                                                else
                                                {
                                                    ASSERT(processResult->sessionData == NULL);
                                                    ASSERT(handler->processSession != NULL || !processResult->close);

                                                    protocolServerResponseP(
                                                        this, .data = processResult->data, .close = processResult->close);

                                                    // Free session when close is true. This optimization allows an explicit
                                                    // close to be skipped.
                                                    if (processResult->close)
                                                    {
                                                        objFree(sessionData);
                                                        lstRemoveIdx(this->sessionList, sessionListIdx);
                                                    }
                                                }

                                                break;
                                            }

                                            // Close protocol session
                                            case protocolCommandTypeClose:
                                            {
                                                // If there is a close handler then call it
                                                PackWrite *data = NULL;

                                                if (handler->close != NULL)
                                                {
                                                    ProtocolServerResult *const closeResult = handler->close(
                                                        pckReadNew(request.param), sessionData);
                                                    ASSERT(closeResult != NULL);
                                                    ASSERT(closeResult->sessionData == NULL);
                                                    ASSERT(!closeResult->close);

                                                    data = closeResult->data;
                                                }

                                                // Send data
                                                protocolServerResponseP(this, .data = data, .close = true);

                                                // Free the session
                                                objFree(sessionData);
                                                lstRemoveIdx(this->sessionList, sessionListIdx);

                                                break;
                                            }

                                            // Cancel protocol session
                                            default:
                                            {
                                                CHECK_FMT(
                                                    ProtocolError, request.type == protocolCommandTypeCancel,
                                                    "unknown request type '%s'", strZ(strIdToStr(request.type)));

                                                // Send NULL data
                                                protocolServerResponseP(this, .close = true);

                                                // Free the session
                                                if (sessionData != NULL)
                                                {
                                                    objFree(sessionData);
                                                    lstRemoveIdx(this->sessionList, sessionListIdx);
                                                }

                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            CATCH_ANY()
                            {
                                // Add the error retry info
                                errRetryAddP(errRetry);

                                // On first error record the stack trace. Only the first error will contain a stack trace since
                                // the first error is most likely to contain valuable information.
                                if (errStackTrace == NULL)
                                    errStackTrace = strNewZ(errorStackTrace());

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

                                    // Send keep-alive to remotes. A retry means the request is taking longer than usual so make
                                    // sure the remote does not timeout.
                                    protocolKeepAlive();
                                }
                                // Else report error to the client
                                else
                                {
                                    protocolServerError(
                                        this, errorTypeCode(errRetryType(errRetry)), errRetryMessage(errRetry), errStackTrace);
                                }
                            }
                            TRY_END();
                        }
                        while (retry);
                    }
                    MEM_CONTEXT_TEMP_END();
                }
                // Else check built-in requests
                else
                {
                    switch (request.id)
                    {
                        case PROTOCOL_COMMAND_EXIT:
                            exit = true;
                            break;

                        case PROTOCOL_COMMAND_NOOP:
                            protocolServerResponseP(this);
                            break;

                        default:
                            THROW_FMT(
                                ProtocolError, "invalid request '%s' (0x%" PRIx64 ")", strZ(strIdToStr(request.id)), request.id);
                    }
                }

                // Send keep-alive to remotes. When a local process is doing work that does not involve the remote it is important
                // that the remote does not timeout. This will send a keep alive once per unit of work that is performed by the
                // local process.
                protocolKeepAlive();
            }
            MEM_CONTEXT_TEMP_END();
        }
        CATCH_FATAL()
        {
            // Zero session id so a fatal error will be handled by the first client that sees it
            this->sessionId = 0;

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
FN_EXTERN ProtocolServerResult *
protocolServerResultNew(const ProtocolServerResultNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, param.extra);
    FUNCTION_TEST_END();

    OBJ_NEW_BEGIN(ProtocolServerResult, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ProtocolServerResult)
        {
            .extra = param.extra,
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(PROTOCOL_SERVER_RESULT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
protocolServerResultData(ProtocolServerResult *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER_RESULT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->data == NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->data = pckWriteNewP(.size = PROTOCOL_PACK_DEFAULT_SIZE + this->extra);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(PACK_WRITE, this->data);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerResultSessionDataSet(ProtocolServerResult *const this, void *const sessionData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER_RESULT, this);
        FUNCTION_TEST_PARAM_P(VOID, sessionData);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(sessionData != NULL);

    this->sessionData = objMove(sessionData, objMemContext(this));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerResultCloseSet(ProtocolServerResult *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER_RESULT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->close = true;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerToLog(const ProtocolServer *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{name: %s}", strZ(this->name));
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerResultToLog(const ProtocolServerResult *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{data: %s, sessionData: %s, close: %s}", cvtBoolToConstZ(this->data != NULL),
        cvtBoolToConstZ(this->sessionData != NULL), cvtBoolToConstZ(this->close));
}
