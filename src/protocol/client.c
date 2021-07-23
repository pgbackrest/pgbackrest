/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
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
Object type
***********************************************************************************************************************************/
struct ProtocolClient
{
    ProtocolClientPub pub;                                          // Publicly accessible variables
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

    // Send an exit command but don't wait to see if it succeeds
    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientCommandPut(this, protocolCommandNew(PROTOCOL_COMMAND_EXIT));
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

    MEM_CONTEXT_NEW_BEGIN("ProtocolClient")
    {
        this = memNew(sizeof(ProtocolClient));

        *this = (ProtocolClient)
        {
            .pub =
            {
                .memContext = memContextCurrent(),
                .read = read,
            },
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
        memContextCallbackSet(this->pub.memContext, protocolClientFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, this);
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

    // End the pack
    if (data != NULL)
        pckWriteEndP(data);

    // Write the data
    PackWrite *dataMessage = pckWriteNew(this->write);
    pckWriteU32P(dataMessage, protocolMessageTypeData, .defaultWrite = true);
    pckWritePackP(dataMessage, data);
    pckWriteEndP(dataMessage);

    // Flush when there is no more data to put
    if (data == NULL)
        ioWriteFlush(this->write);

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

    if (type == protocolMessageTypeError)
    {
        const ErrorType *type = errorTypeFromCode(pckReadI32P(error));
        String *const message = strNewFmt("%s: %s", strZ(this->errorPrefix), strZ(pckReadStrP(error)));
        const String *const stack = pckReadStrP(error);
        pckReadEndP(error);

        CHECK(message != NULL);

        // Add stack trace if the error is an assertion or debug-level logging is enabled
        if (type == &AssertError || logAny(logLevelDebug))
        {
            CHECK(stack != NULL);

            strCat(message, LF_STR);
            strCat(message, stack);
        }

        THROWP(type, strZ(message));
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

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *response = pckReadNew(this->pub.read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(response);

        protocolClientError(this, type, response);

        CHECK(type == protocolMessageTypeData);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = pckReadPackP(response);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(response);
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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *response = pckReadNew(this->pub.read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(response);

        protocolClientError(this, type, response);

        CHECK(type == protocolMessageTypeDataEnd);

        pckReadEndP(response);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolClientCommandPut(ProtocolClient *const this, ProtocolCommand *const command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Put command
    protocolCommandPut(command, this->write);

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
    protocolClientCommandPut(this, command);

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
    return strNewFmt("{name: %s}", strZ(this->name));
}
