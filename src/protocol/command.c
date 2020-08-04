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
    Variant *parameterList;
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
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
ProtocolCommand *
protocolCommandParamAdd(ProtocolCommand *this, const Variant *param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
        FUNCTION_TEST_PARAM(VARIANT, param);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Create parameter list if not already created
        if (this->parameterList == NULL)
            this->parameterList = varNewVarLst(varLstNew());

        // Add parameter to the list
        varLstAdd(varVarLst(this->parameterList), varDup(param));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
protocolCommandJson(const ProtocolCommand *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_COMMAND, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        KeyValue *command = kvPut(kvNew(), VARSTR(PROTOCOL_KEY_COMMAND_STR), VARSTR(this->command));

        if (this->parameterList != NULL)
            kvPut(command, VARSTR(PROTOCOL_KEY_PARAMETER_STR), this->parameterList);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = jsonFromKv(command);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
protocolCommandToLog(const ProtocolCommand *this)
{
    return strNewFmt("{command: %s}", strZ(this->command));
}
