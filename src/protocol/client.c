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
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_GREETING_NAME_STR,                           PROTOCOL_GREETING_NAME);
STRING_EXTERN(PROTOCOL_GREETING_SERVICE_STR,                        PROTOCOL_GREETING_SERVICE);
STRING_EXTERN(PROTOCOL_GREETING_VERSION_STR,                        PROTOCOL_GREETING_VERSION);

/***********************************************************************************************************************************
Client state enum
***********************************************************************************************************************************/
typedef enum
{
    // Client is waiting for a command
    protocolClientStateIdle = STRID5("idle", 0x2b0890),

    // Command put is in progress
    protocolClientStateCommandPut = STRID5("cmd-put", 0x52b0d91a30),

    // Waiting for command data from server. Only used when dataPut is true in protocolClientCommandPut().
    protocolClientStateCommandDataGet = STRID5("cmd-data-get", 0xa14fb0d024d91a30),

    // Putting data to server. Only used when dataPut is true in protocolClientCommandPut().
    protocolClientStateDataPut = STRID5("data-put", 0xa561b0d0240),

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
        protocolClientCommandPut(this, protocolCommandNew(PROTOCOL_COMMAND_EXIT), false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
ProtocolClient *
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

    ProtocolClient *this = NULL;

    OBJ_NEW_BEGIN(ProtocolClient)
    {
        this = OBJ_NEW_ALLOC();

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
            String *greeting = ioReadLine(this->pub.read);
            KeyValue *greetingKv = jsonToKv(greeting);

            const String *expected[] =
            {
                PROTOCOL_GREETING_NAME_STR, STRDEF(PROJECT_NAME),
                PROTOCOL_GREETING_SERVICE_STR, service,
                PROTOCOL_GREETING_VERSION_STR, STRDEF(PROJECT_VERSION),
            };

            for (unsigned int expectedIdx = 0; expectedIdx < sizeof(expected) / sizeof(char *) / 2; expectedIdx++)
            {
                const String *expectedKey = expected[expectedIdx * 2];
                const String *expectedValue = expected[expectedIdx * 2 + 1];

                const Variant *actualValue = kvGet(greetingKv, VARSTR(expectedKey));

                if (actualValue == NULL)
                    THROW_FMT(ProtocolError, "unable to find greeting key '%s'", strZ(expectedKey));

                if (varType(actualValue) != varTypeString)
                    THROW_FMT(ProtocolError, "greeting key '%s' must be string type", strZ(expectedKey));

                if (!strEq(varStr(actualValue), expectedValue))
                {
                    THROW_FMT(
                        ProtocolError,
                        "expected value '%s' for greeting key '%s' but got '%s'\n"
                            "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?",
                        strZ(expectedValue), strZ(expectedKey), strZ(varStr(actualValue)));
                }
            }
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
void
protocolClientDataPut(ProtocolClient *const this, PackWrite *const data)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, data);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Expect data-put state before data put
    protocolClientStateExpect(this, protocolClientStateDataPut);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // End the pack
        if (data != NULL)
            pckWriteEndP(data);

        // Write the data
        PackWrite *dataMessage = pckWriteNewIo(this->write);
        pckWriteU32P(dataMessage, protocolMessageTypeData, .defaultWrite = true);
        pckWritePackP(dataMessage, pckWriteResult(data));
        pckWriteEndP(dataMessage);

        // Flush when there is no more data to put
        if (data == NULL)
        {
            ioWriteFlush(this->write);

            // Switch state to data-get after successful data end put
            this->state = protocolClientStateDataGet;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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

            CHECK(message != NULL && stack != NULL);

            errorInternalThrow(type, __FILE__, __func__, __LINE__, strZ(message), strZ(stack));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackRead *
protocolClientDataGet(ProtocolClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Expect data-get state before data get
    protocolClientStateExpect(
        this, this->state == protocolClientStateCommandDataGet ? protocolClientStateCommandDataGet : protocolClientStateDataGet);

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *response = pckReadNewIo(this->pub.read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(response);

        protocolClientError(this, type, response);

        CHECK(type == protocolMessageTypeData);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = pckReadPackReadP(response);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(response);

        // Switch state to data-put after successful command data get
        if (this->state == protocolClientStateCommandDataGet)
            this->state = protocolClientStateDataPut;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
void
protocolClientDataEndGet(ProtocolClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Expect data-get state before data end get
    protocolClientStateExpect(this, protocolClientStateDataGet);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *response = pckReadNewIo(this->pub.read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(response);

        protocolClientError(this, type, response);

        CHECK(type == protocolMessageTypeDataEnd);

        pckReadEndP(response);

        // Switch state to idle after successful data end get
        this->state = protocolClientStateIdle;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolClientCommandPut(ProtocolClient *const this, ProtocolCommand *const command, const bool dataPut)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
        FUNCTION_LOG_PARAM(BOOL, dataPut);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Expect idle state before command put
    protocolClientStateExpect(this, protocolClientStateIdle);

    // Switch state to cmd-put
    this->state = protocolClientStateDataPut;

    // Put command
    protocolCommandPut(command, this->write);

    // Switch state to data-get/data-put after successful command put
    this->state = dataPut ? protocolClientStateCommandDataGet : protocolClientStateDataGet;

    // Reset the keep alive time
    this->keepAliveTime = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackRead *
protocolClientExecute(ProtocolClient *const this, ProtocolCommand *const command, const bool resultRequired)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
        FUNCTION_LOG_PARAM(BOOL, resultRequired);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Put command
    protocolClientCommandPut(this, command, false);

    // Read result if required
    PackRead *result = NULL;

    if (resultRequired)
        result = protocolClientDataGet(this);

    // Read response
    protocolClientDataEndGet(this);

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
void
protocolClientNoOp(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientExecute(this, protocolCommandNew(PROTOCOL_COMMAND_NOOP), false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
protocolClientToLog(const ProtocolClient *this)
{
    return strNewFmt("{name: %s, state: %s}", strZ(this->name), strZ(strIdToStr(this->state)));
}
