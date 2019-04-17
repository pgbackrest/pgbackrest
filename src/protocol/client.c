/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
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

STRING_EXTERN(PROTOCOL_COMMAND_NOOP_STR,                            PROTOCOL_COMMAND_NOOP);
STRING_EXTERN(PROTOCOL_COMMAND_EXIT_STR,                            PROTOCOL_COMMAND_EXIT);

STRING_EXTERN(PROTOCOL_ERROR_STR,                                   PROTOCOL_ERROR);

STRING_EXTERN(PROTOCOL_OUTPUT_STR,                                  PROTOCOL_OUTPUT);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolClient
{
    MemContext *memContext;
    const String *name;
    const String *errorPrefix;
    IoRead *read;
    IoWrite *write;
    TimeMSec keepAliveTime;
};

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
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
        this->memContext = memContextCurrent();

        this->name = strDup(name);
        this->errorPrefix = strNewFmt("raised from %s", strPtr(this->name));
        this->read = read;
        this->write = write;
        this->keepAliveTime = timeMSec();

        // Read, parse, and check the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *greeting = ioReadLine(this->read);
            KeyValue *greetingKv = varKv(jsonToVar(greeting));

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
                    THROW_FMT(ProtocolError, "unable to find greeting key '%s'", strPtr(expectedKey));

                if (varType(actualValue) != varTypeString)
                    THROW_FMT(ProtocolError, "greeting key '%s' must be string type", strPtr(expectedKey));

                if (!strEq(varStr(actualValue), expectedValue))
                {
                    THROW_FMT(
                        ProtocolError, "expected value '%s' for greeting key '%s' but got '%s'", strPtr(expectedValue),
                        strPtr(expectedKey), strPtr(varStr(actualValue)));
                }
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Send one noop to catch any errors that might happen after the greeting
        protocolClientNoOp(this);

        // Set a callback to shutdown the protocol
        memContextCallback(this->memContext, (MemContextCallback)protocolClientFree, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, this);
}

/***********************************************************************************************************************************
Read the command output
***********************************************************************************************************************************/
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
        String *response = ioReadLine(this->read);
        KeyValue *responseKv = varKv(jsonToVar(response));

        // Process error if any
        const Variant *error = kvGet(responseKv, VARSTR(PROTOCOL_ERROR_STR));

        if (error != NULL)
        {
            const String *message = varStr(kvGet(responseKv, VARSTR(PROTOCOL_OUTPUT_STR)));

            THROWP_FMT(
                errorTypeFromCode(varIntForce(error)), "%s: %s", strPtr(this->errorPrefix),
                message == NULL ? "no details available" : strPtr(message));
        }

        // Get output
        result = kvGet(responseKv, VARSTR(PROTOCOL_OUTPUT_STR));

        if (outputRequired)
        {
            // Just move the entire response kv since the output is the largest part if it
            kvMove(responseKv, MEM_CONTEXT_OLD());
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

/***********************************************************************************************************************************
Write the protocol command
***********************************************************************************************************************************/
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
    ioWriteLine(this->write, protocolCommandJson(command));
    ioWriteFlush(this->write);

    // Reset the keep alive time
    this->keepAliveTime = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Execute a protocol command and get the output
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
ProtocolClient *
protocolClientMove(ProtocolClient *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Send noop to test connection or keep it alive
***********************************************************************************************************************************/
void
protocolClientNoOp(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientExecute(this, protocolCommandNew(PROTOCOL_COMMAND_NOOP_STR), false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
protocolClientIoRead(const ProtocolClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->read);
}

/***********************************************************************************************************************************
Get write interface
***********************************************************************************************************************************/
IoWrite *
protocolClientIoWrite(const ProtocolClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->write);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
protocolClientToLog(const ProtocolClient *this)
{
    return strNewFmt("{name: %s}", strPtr(this->name));
}

/***********************************************************************************************************************************
Free object
***********************************************************************************************************************************/
void
protocolClientFree(ProtocolClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        memContextCallbackClear(this->memContext);

        // Send an exit command but don't wait to see if it succeeds
        MEM_CONTEXT_TEMP_BEGIN()
        {
            protocolClientWriteCommand(this, protocolCommandNew(PROTOCOL_COMMAND_EXIT_STR));
        }
        MEM_CONTEXT_TEMP_END();

        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}
