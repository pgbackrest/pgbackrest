/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/time.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolServer
{
    MemContext *memContext;
    const String *name;
    IoRead *read;
    IoWrite *write;

    List *handlerList;
};

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
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

    MEM_CONTEXT_NEW_BEGIN("ProtocolServer")
    {
        this = memNew(sizeof(ProtocolServer));
        this->memContext = memContextCurrent();

        this->name = strDup(name);
        this->read = read;
        this->write = write;

        this->handlerList = lstNew(sizeof(ProtocolServerProcessHandler));

        // Send the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            KeyValue *greetingKv = kvNew();
            kvPut(greetingKv, varNewStr(PROTOCOL_GREETING_NAME_STR), varNewStr(strNew(PROJECT_NAME)));
            kvPut(greetingKv, varNewStr(PROTOCOL_GREETING_SERVICE_STR), varNewStr(service));
            kvPut(greetingKv, varNewStr(PROTOCOL_GREETING_VERSION_STR), varNewStr(strNew(PROJECT_VERSION)));

            ioWriteLine(this->write, kvToJson(greetingKv, 0));
            ioWriteFlush(this->write);
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER, this);
}

/***********************************************************************************************************************************
Add a new handler
***********************************************************************************************************************************/
void
protocolServerHandlerAdd(ProtocolServer *this, ProtocolServerProcessHandler handler)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(FUNCTIONP, handler);
    FUNCTION_LOG_END();

    lstAdd(this->handlerList, &handler);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Process requests
***********************************************************************************************************************************/
void
protocolServerProcess(ProtocolServer *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    // Loop until exit command is received
    bool exit = false;

    do
    {
        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Read command
                KeyValue *commandKv = varKv(jsonToVar(ioReadLine(this->read)));
                String *command = varStr(kvGet(commandKv, varNewStr(PROTOCOL_COMMAND_STR)));
                VariantList *paramList = varVarLst(kvGet(commandKv, varNewStr(PROTOCOL_PARAMETER_STR)));

                // Process command
                bool found = false;

                for (unsigned int handlerIdx = 0; handlerIdx < lstSize(this->handlerList); handlerIdx++)
                {
                    // Get the next handler
                    ProtocolServerProcessHandler handler = *(ProtocolServerProcessHandler *)lstGet(this->handlerList, handlerIdx);

                    // Send the command to the handler
                    found = handler(command, paramList, this);

                    // If the handler processed the command then exit the handler loop
                    if (found)
                        break;
                }

                if (!found)
                {
                    if (strEq(command, PROTOCOL_COMMAND_NOOP_STR))
                        protocolServerResponse(this, NULL);
                    else if (strEq(command, PROTOCOL_COMMAND_EXIT_STR))
                        exit = true;
                    else
                        THROW_FMT(ProtocolError, "invalid command '%s'", strPtr(command));
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        // Asserts are thrown so a stack trace will be output to aid in debugging
        CATCH(AssertError)
        {
            RETHROW();
        }
        CATCH_ANY()
        {
            KeyValue *error = kvNew();
            kvPut(error, varNewStr(PROTOCOL_ERROR_STR), varNewInt(errorCode()));
            kvPut(error, varNewStr(PROTOCOL_OUTPUT_STR), varNewStr(strNew(errorMessage())));

            ioWriteLine(this->write, kvToJson(error, 0));
            ioWriteFlush(this->write);
        }
        TRY_END();
    }
    while (!exit);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Respond to request with output if provided
***********************************************************************************************************************************/
void
protocolServerResponse(ProtocolServer *this, const Variant *output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(VARIANT, output);
    FUNCTION_LOG_END();

    KeyValue *result = kvNew();

    if (output != NULL)
        kvAdd(result, varNewStr(PROTOCOL_OUTPUT_STR), output);

    ioWriteLine(this->write, kvToJson(result, 0));
    ioWriteFlush(this->write);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
ProtocolServer *
protocolServerMove(ProtocolServer *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
protocolServerIoRead(const ProtocolServer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->read);
}

/***********************************************************************************************************************************
Get write interface
***********************************************************************************************************************************/
IoWrite *
protocolServerIoWrite(const ProtocolServer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->write);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
protocolServerToLog(const ProtocolServer *this)
{
    return strNewFmt("{name: %s}", strPtr(this->name));
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
protocolServerFree(ProtocolServer *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
