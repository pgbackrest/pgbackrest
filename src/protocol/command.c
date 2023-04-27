/***********************************************************************************************************************************
Protocol Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/keyValue.h"
#include "protocol/client.h"
#include "protocol/command.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolCommand
{
    StringId command;
    PackWrite *pack;
};

/**********************************************************************************************************************************/
FN_EXTERN ProtocolCommand *
protocolCommandNew(const StringId command)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, command);
    FUNCTION_TEST_END();

    ASSERT(command != 0);

    OBJ_NEW_BEGIN(ProtocolCommand, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ProtocolCommand)
        {
            .command = command,
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(PROTOCOL_COMMAND, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolCommandPut(ProtocolCommand *const this, IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the command and flush to be sure the command gets sent immediately
        PackWrite *commandPack = pckWriteNewIo(write);
        pckWriteU32P(commandPack, protocolMessageTypeCommand, .defaultWrite = true);
        pckWriteStrIdP(commandPack, this->command);

        // Only write params if there were any
        if (this->pack != NULL)
        {
            pckWriteEndP(this->pack);
            pckWritePackP(commandPack, pckWriteResult(this->pack));
        }

        pckWriteEndP(commandPack);

        // Flush to send command immediately
        ioWriteFlush(write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
protocolCommandParam(ProtocolCommand *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->pack == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->pack = protocolPackNew();
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN(PACK_WRITE, this->pack);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolCommandToLog(const ProtocolCommand *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{name: ");
    strStcResultSizeInc(debugLog, strIdToLog(this->command, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcCatChr(debugLog, '}');
}
