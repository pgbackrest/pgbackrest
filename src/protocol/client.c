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
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_GREETING_NAME_STR,                           PROTOCOL_GREETING_NAME);
STRING_EXTERN(PROTOCOL_GREETING_SERVICE_STR,                        PROTOCOL_GREETING_SERVICE);
STRING_EXTERN(PROTOCOL_GREETING_VERSION_STR,                        PROTOCOL_GREETING_VERSION);

STRING_EXTERN(PROTOCOL_ERROR_STR,                                   PROTOCOL_ERROR);
STRING_EXTERN(PROTOCOL_ERROR_STACK_STR,                             PROTOCOL_ERROR_STACK);

STRING_EXTERN(PROTOCOL_OUTPUT_STR,                                  PROTOCOL_OUTPUT);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolClient
{
    ProtocolClientPub pub;                                          // Publicly accessible variables
    const String *name;
    const String *errorPrefix;
    TimeMSec keepAliveTime;
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
        protocolClientWriteCommand(this, protocolCommandNew(PROTOCOL_COMMAND_EXIT));
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
                .write = write,
            },
            .name = strDup(name),
            .errorPrefix = strNewFmt("raised from %s", strZ(name)),
            .keepAliveTime = timeMSec(),
        };

        // Read, parse, and check the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *greeting = ioReadLine(protocolClientIoRead(this));
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

        // Send one noop to catch any errors that might happen after the greeting
        protocolClientNoOp(this);

        // Set a callback to shutdown the protocol
        memContextCallbackSet(this->pub.memContext, protocolClientFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, this);
}

/**********************************************************************************************************************************/
// Helper to process errors
static void
protocolClientProcessError(ProtocolClient *this, KeyValue *errorKv)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(KEY_VALUE, errorKv);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(errorKv != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Process error if any
        const Variant *error = kvGet(errorKv, VARSTR(PROTOCOL_ERROR_STR));

        if (error != NULL)
        {
            const ErrorType *type = errorTypeFromCode(varIntForce(error));
            const String *message = varStr(kvGet(errorKv, VARSTR(PROTOCOL_OUTPUT_STR)));

            // Required part of the message
            String *throwMessage = strNewFmt(
                "%s: %s", strZ(this->errorPrefix), message == NULL ? "no details available" : strZ(message));

            // Add stack trace if the error is an assertion or debug-level logging is enabled
            if (type == &AssertError || logAny(logLevelDebug))
            {
                const String *stack = varStr(kvGet(errorKv, VARSTR(PROTOCOL_ERROR_STACK_STR)));

                strCat(throwMessage, LF_STR);
                strCat(throwMessage, stack == NULL ? STRDEF("no stack trace available") : stack);
            }

            THROWP(type, strZ(throwMessage));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

const Variant *
protocolClientReadOutput(ProtocolClient *this, bool outputRequired)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(BOOL, outputRequired);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    const Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read the response
        String *response = ioReadLine(protocolClientIoRead(this));
        KeyValue *responseKv = varKv(jsonToVar(response));

        // Process error if any
        protocolClientProcessError(this, responseKv);

        // Get output
        result = kvGet(responseKv, VARSTR(PROTOCOL_OUTPUT_STR));

        if (outputRequired)
        {
            // Just move the entire response kv since the output is the largest part if it
            kvMove(responseKv, memContextPrior());
        }
        // Else if no output is required then there should not be any
        else if (result != NULL)
            THROW(AssertError, "no output required by command");

        // Reset the keep alive time
        this->keepAliveTime = timeMSec();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_CONST(VARIANT, result);
}

/**********************************************************************************************************************************/
void
protocolClientWriteCommand(ProtocolClient *this, const ProtocolCommand *command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    // Write out the command
    ioWriteStrLine(protocolClientIoWrite(this), protocolCommandJson(command));
    ioWriteFlush(protocolClientIoWrite(this));

    // Reset the keep alive time
    this->keepAliveTime = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const Variant *
protocolClientExecute(ProtocolClient *this, const ProtocolCommand *command, bool outputRequired)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
        FUNCTION_LOG_PARAM(BOOL, outputRequired);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    protocolClientWriteCommand(this, command);

    FUNCTION_LOG_RETURN_CONST(VARIANT, protocolClientReadOutput(this, outputRequired));
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
protocolClientReadLine(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = ioReadLine(protocolClientIoRead(this));

        if (strSize(result) == 0)
        {
            THROW(FormatError, "unexpected empty line");
        }
        else if (strZ(result)[0] == '{')
        {
            KeyValue *responseKv = varKv(jsonToVar(result));

            // Process expected error
            protocolClientProcessError(this, responseKv);

            // If not an error then there is probably a protocol bug
            THROW(FormatError, "expected error but got output");
        }
        else if (strZ(result)[0] != '.')
            THROW_FMT(FormatError, "invalid prefix in '%s'", strZ(result));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strSub(result, 1);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
protocolClientToLog(const ProtocolClient *this)
{
    return strNewFmt("{name: %s}", strZ(this->name));
}
