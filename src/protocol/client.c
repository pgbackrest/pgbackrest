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
};

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

    // Send an exit command but don't wait to see if it succeeds
    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientCommandPut(this, protocolCommandNewP(PROTOCOL_COMMAND_EXIT));
    }
    MEM_CONTEXT_TEMP_END();

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

/***********************************************************************************************************************************
Check protocol state
***********************************************************************************************************************************/
static void
protocolClientStateExpect(const ProtocolClient *const this, const ProtocolClientState expect)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_TEST_PARAM(STRING_ID, expect);
    FUNCTION_TEST_END();

    if (this->state != expect)
        THROW_FMT(ProtocolError, "client state is '%s' but expected '%s'", strZ(strIdToStr(this->state)), strZ(strIdToStr(expect)));

    FUNCTION_TEST_RETURN_VOID();
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
    ASSERT(error != NULL);

    if (type == protocolMessageTypeError)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const ErrorType *const type = errorTypeFromCode(pckReadI32P(error));
            const String *const message = strNewFmt("%s: %s", strZ(this->errorPrefix), strZ(pckReadStrP(error)));
            const String *const stack = pckReadStrP(error);
            pckReadEndP(error);

            // Switch state to idle after error (server will do the same)
            this->state = protocolClientStateIdle;

            CHECK(FormatError, message != NULL && stack != NULL, "invalid error data");

            errorInternalThrow(type, __FILE__, __func__, __LINE__, strZ(message), strZ(stack));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientDataGet(ProtocolClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Expect data-get state before data get
    protocolClientStateExpect(this, protocolClientStateDataGet);

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *response = pckReadNewIo(this->pub.read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(response);

        protocolClientError(this, type, response);

        CHECK(FormatError, type == protocolMessageTypeData, "expected data message");

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = pckReadPackReadP(response);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(response);
    }
    MEM_CONTEXT_TEMP_END();

    // Switch state to idle after successful data get
    this->state = protocolClientStateIdle;

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
protocolClientCommandPut(ProtocolClient *const this, ProtocolCommand *const command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Most command types do not return a session id
    uint64_t result = 0;

    // Expect idle state before command put
    protocolClientStateExpect(this, protocolClientStateIdle);

    // Switch state to cmd-put
    this->state = protocolClientStateCommandPut;

    // Put command
    protocolCommandPut(command, this->write);

    // Switch state to data-get after successful command put
    this->state = protocolClientStateDataGet;

    // If command type is open then get the session id
    if (protocolCommandType(command) == protocolCommandTypeOpen)
    {
        PackRead *const read = protocolClientDataGet(this);

        result = pckReadU64P(read);

        pckReadFree(read);

        // Switch state to data-get after getting the session id so the command can return its own data
        this->state = protocolClientStateDataGet;
    }

    // Reset the keep alive time
    this->keepAliveTime = timeMSec();

    FUNCTION_LOG_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
protocolClientExecute(ProtocolClient *const this, ProtocolCommand *const command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Put command
    protocolClientCommandPut(this, command);

    FUNCTION_LOG_RETURN(PACK_READ, protocolClientDataGet(this));
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolClientNoOp(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientExecute(this, protocolCommandNewP(PROTOCOL_COMMAND_NOOP));
    }
    MEM_CONTEXT_TEMP_END();

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
