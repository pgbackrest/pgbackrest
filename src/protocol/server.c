/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include <string.h>

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
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_NAME_STR), VARSTRZ(PROJECT_NAME));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_SERVICE_STR), VARSTR(service));
            kvPut(greetingKv, VARSTR(PROTOCOL_GREETING_VERSION_STR), VARSTRZ(PROJECT_VERSION));

            ioWriteStrLine(this->write, jsonFromKv(greetingKv, 0));
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
Return an error
***********************************************************************************************************************************/
void
protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
        FUNCTION_LOG_PARAM(STRING, stack);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(code != 0);
    ASSERT(message != NULL);
    ASSERT(stack != NULL);

    KeyValue *error = kvNew();
    kvPut(error, VARSTR(PROTOCOL_ERROR_STR), VARINT(code));
    kvPut(error, VARSTR(PROTOCOL_OUTPUT_STR), VARSTR(message));
    kvPut(error, VARSTR(PROTOCOL_ERROR_STACK_STR), VARSTR(stack));

    ioWriteStrLine(this->write, jsonFromKv(error, 0));
    ioWriteFlush(this->write);

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
                KeyValue *commandKv = jsonToKv(ioReadLine(this->read));
                const String *command = varStr(kvGet(commandKv, VARSTR(PROTOCOL_KEY_COMMAND_STR)));
                VariantList *paramList = varVarLst(kvGet(commandKv, VARSTR(PROTOCOL_KEY_PARAMETER_STR)));

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
        CATCH_ANY()
        {
            // Report error to the client
            protocolServerError(this, errorCode(), STR(errorMessage()), STR(errorStackTrace()));
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
        kvAdd(result, VARSTR(PROTOCOL_OUTPUT_STR), output);

    ioWriteStrLine(this->write, jsonFromKv(result, 0));
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
Free object
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
