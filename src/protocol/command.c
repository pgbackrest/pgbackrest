/***********************************************************************************************************************************
Protocol Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/object.h"
#include "protocol/command.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_KEY_COMMAND_STR,                             PROTOCOL_KEY_COMMAND);
STRING_EXTERN(PROTOCOL_KEY_PARAMETER_STR,                           PROTOCOL_KEY_PARAMETER);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolCommand
{
    MemContext *memContext;
    const String *command;
    PackWrite *pack;
};

OBJECT_DEFINE_MOVE(PROTOCOL_COMMAND);
OBJECT_DEFINE_FREE(PROTOCOL_COMMAND);

/**********************************************************************************************************************************/
ProtocolCommand *
protocolCommandNew(const String *command)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, command);
    FUNCTION_TEST_END();

    ASSERT(command != NULL);

    ProtocolCommand *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ProtocolCommand")
    {
        this = memNew(sizeof(ProtocolCommand));

        *this = (ProtocolCommand)
        {
            .memContext = memContextCurrent(),
            .command = strDup(command),
            .pack = pckWriteNewBuf(bufNew(1024)),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

void
protocolCommandWrite(const ProtocolCommand *this, IoWrite *write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    // Write the command and flush to be sure the command gets sent immediately
    PackWrite *commandPack = pckWriteNew(write);

    pckWriteStrP(commandPack, this->command);

    // Only write params if there were any
    if (!pckWriteEmpty(this->pack))
        pckWritePackP(commandPack, this->pack);

    pckWriteEndP(commandPack);

    // Flush to send command immediately
    ioWriteFlush(write);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackWrite *
protocolCommandParam(ProtocolCommand *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->pack);
}

/**********************************************************************************************************************************/
String *
protocolCommandToLog(const ProtocolCommand *this)
{
    return strNewFmt("{command: %s}", strZ(this->command));
}
