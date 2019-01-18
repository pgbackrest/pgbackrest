/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#include "common/assert.h"
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

STRING_EXTERN(PROTOCOL_COMMAND_STR,                                 PROTOCOL_COMMAND);
STRING_EXTERN(PROTOCOL_COMMAND_NOOP_STR,                            PROTOCOL_COMMAND_NOOP);
STRING_EXTERN(PROTOCOL_COMMAND_EXIT_STR,                            PROTOCOL_COMMAND_EXIT);

STRING_EXTERN(PROTOCOL_ERROR_STR,                                   PROTOCOL_ERROR);

STRING_EXTERN(PROTOCOL_OUTPUT_STR,                                  PROTOCOL_OUTPUT);

STRING_EXTERN(PROTOCOL_PARAMETER_STR,                               PROTOCOL_PARAMETER);

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
Create a new storage file
***********************************************************************************************************************************/
ProtocolClient *
protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(IO_READ, read);
        FUNCTION_DEBUG_PARAM(IO_WRITE, write);

        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(read != NULL);
        FUNCTION_TEST_ASSERT(write != NULL);
    FUNCTION_DEBUG_END();

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
            KeyValue *greetingKv = jsonToKv(greeting);

            const String *expected[] =
            {
                PROTOCOL_GREETING_NAME_STR, strNew(PROJECT_NAME),
                PROTOCOL_GREETING_SERVICE_STR, service,
                PROTOCOL_GREETING_VERSION_STR, strNew(PROJECT_VERSION),
            };

            for (unsigned int expectedIdx = 0; expectedIdx < sizeof(expected) / sizeof(char *) / 2; expectedIdx++)
            {
                const String *expectedKey = expected[expectedIdx * 2];
                const String *expectedValue = expected[expectedIdx * 2 + 1];

                const Variant *actualValue = kvGet(greetingKv, varNewStr(expectedKey));

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

        // Set a callback so the handles will get freed
        memContextCallback(this->memContext, (MemContextCallback)protocolClientFree, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(PROTOCOL_CLIENT, this);
}

/***********************************************************************************************************************************
Read the command output
***********************************************************************************************************************************/
const VariantList *
protocolClientReadOutput(ProtocolClient *this, bool outputRequired)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_DEBUG_PARAM(BOOL, outputRequired);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    const VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read the response
        String *response = ioReadLine(this->read);
        KeyValue *responseKv = jsonToKv(response);

        // Process error if any
        const Variant *error = kvGet(responseKv, varNewStr(PROTOCOL_ERROR_STR));

        if (error != NULL)
        {
            const String *message = varStr(kvGet(responseKv, varNewStr(PROTOCOL_OUTPUT_STR)));

            THROWP_FMT(
                errorTypeFromCode(varIntForce(error)), "%s: %s", strPtr(this->errorPrefix),
                message == NULL ? "no details available" : strPtr(message));
        }

        // Get output
        const Variant *output = kvGet(responseKv, varNewStr(PROTOCOL_OUTPUT_STR));

        ASSERT(output != NULL);
        ASSERT(varType(output) == varTypeVariantList);

        if (outputRequired)
        {
            // Just move the entire response kv since the output is the largest part if it
            kvMove(responseKv, MEM_CONTEXT_OLD());
            result = varVarLst(output);
        }
        // Else if no output is required then there should not be any
        else if (varLstSize(varVarLst(output)) != 0)
            THROW(AssertError, "no output required by command");

        // Reset the keep alive time
        this->keepAliveTime = timeMSec();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(CONST_VARIANT_LIST, result);
}

/***********************************************************************************************************************************
Write the protocol command
***********************************************************************************************************************************/
void
protocolClientWriteCommand(ProtocolClient *this, const KeyValue *command)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_DEBUG_PARAM(KEY_VALUE, command);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(command != NULL);
    FUNCTION_DEBUG_END();

    // Write out the command
    ioWriteLine(this->write, kvToJson(command, 0));
    ioWriteFlush(this->write);

    // Reset the keep alive time
    this->keepAliveTime = timeMSec();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Execute a protocol command and get the output
***********************************************************************************************************************************/
const VariantList *
protocolClientExecute(ProtocolClient *this, const KeyValue *command, bool outputRequired)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(PROTOCOL_CLIENT, this);
        FUNCTION_DEBUG_PARAM(KEY_VALUE, command);
        FUNCTION_DEBUG_PARAM(BOOL, outputRequired);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(command != NULL);
    FUNCTION_DEBUG_END();

    protocolClientWriteCommand(this, command);

    FUNCTION_DEBUG_RESULT(CONST_VARIANT_LIST, protocolClientReadOutput(this, outputRequired));
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

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(PROTOCOL_CLIENT, this);
}

/***********************************************************************************************************************************
Send noop to test connection or keep it alive
***********************************************************************************************************************************/
void
protocolClientNoOp(ProtocolClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(PROTOCOL_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolClientExecute(this, kvPut(kvNew(), varNewStr(PROTOCOL_COMMAND_STR), varNewStr(PROTOCOL_COMMAND_NOOP_STR)), false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
protocolClientIoRead(const ProtocolClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->read);
}

/***********************************************************************************************************************************
Get write interface
***********************************************************************************************************************************/
IoWrite *
protocolClientIoWrite(const ProtocolClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->write);
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
Free the file
***********************************************************************************************************************************/
void
protocolClientFree(ProtocolClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(PROTOCOL_CLIENT, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        memContextCallbackClear(this->memContext);

        // Send an exit command but don't wait to see if it succeeds
        MEM_CONTEXT_TEMP_BEGIN()
        {
            protocolClientWriteCommand(this, kvPut(kvNew(), varNewStr(PROTOCOL_COMMAND_STR), varNewStr(PROTOCOL_COMMAND_EXIT_STR)));
        }
        MEM_CONTEXT_TEMP_END();

        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
