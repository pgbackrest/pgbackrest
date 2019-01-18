/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/exec.h"
#include "common/memContext.h"
#include "config/config.h"
#include "config/exec.h"
#include "protocol/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_SERVICE_REMOTE_STR,                          PROTOCOL_SERVICE_REMOTE);

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context for protocol helper

    Exec *remoteExec;                                               // Executed remote
    ProtocolClient *remote;                                         // Remote protocol client
} protocolHelper;

/***********************************************************************************************************************************
Is the repository local?
***********************************************************************************************************************************/
bool
repoIsLocal(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RESULT(BOOL, !cfgOptionTest(cfgOptRepoHost));
}

/***********************************************************************************************************************************
Get the command line required for protocol execution
***********************************************************************************************************************************/
static StringList *
protocolParam(RemoteType remoteType, unsigned int remoteId)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(ENUM, remoteType);
        FUNCTION_DEBUG_PARAM(UINT, remoteId);

        FUNCTION_TEST_ASSERT(remoteType == remoteTypeRepo);         // ??? Hard-coded until the function supports db remotes
        FUNCTION_TEST_ASSERT(remoteId == 1);                        // ??? Hard-coded until the function supports db remotes
    FUNCTION_DEBUG_END();

    // Fixed parameters for ssh command
    StringList *result = strLstNew();
    strLstAddZ(result, "-o");
    strLstAddZ(result, "LogLevel=error");
    strLstAddZ(result, "-o");
    strLstAddZ(result, "Compression=no");
    strLstAddZ(result, "-o");
    strLstAddZ(result, "PasswordAuthentication=no");

    // Append port if specified
    if (cfgOptionTest(cfgOptRepoHostPort))
    {
        strLstAddZ(result, "-p");
        strLstAdd(result, strNewFmt("%d", cfgOptionInt(cfgOptRepoHostPort)));
    }

    // Append user/host
    strLstAdd(result, strNewFmt("%s@%s", strPtr(cfgOptionStr(cfgOptRepoHostUser)), strPtr(cfgOptionStr(cfgOptRepoHost))));

    // Append pgbackrest command
    KeyValue *optionReplace = kvNew();

    // Replace config options with the host versions
    if (cfgOptionSource(cfgOptRepoHostConfig) != cfgSourceDefault)
        kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptConfig))), cfgOption(cfgOptRepoHostConfig));

    if (cfgOptionSource(cfgOptRepoHostConfigIncludePath) != cfgSourceDefault)
        kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptConfigIncludePath))), cfgOption(cfgOptRepoHostConfigIncludePath));

    if (cfgOptionSource(cfgOptRepoHostConfigPath) != cfgSourceDefault)
        kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptConfigPath))), cfgOption(cfgOptRepoHostConfigPath));

    // If this is the local or remote command then we need to add the command option
    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptCommand))), varNewStr(strNew(cfgCommandName(cfgCommand()))));

    // Add the process id -- used when more than one process will be called
    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptProcess))), varNewInt(0));

    // Add the type
    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptType))), varNewStr(strNew("backup")));

    StringList *commandExec = cfgExecParam(cfgCmdRemote, optionReplace);
    strLstInsert(commandExec, 0, cfgOptionStr(cfgOptRepoHostCmd));
    strLstAdd(result, strLstJoin(commandExec, " "));

    FUNCTION_DEBUG_RESULT(STRING_LIST, result);
}

/***********************************************************************************************************************************
Get the protocol client
***********************************************************************************************************************************/
ProtocolClient *
protocolGet(RemoteType remoteType, unsigned int remoteId)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(ENUM, remoteType);
        FUNCTION_DEBUG_PARAM(UINT, remoteId);
    FUNCTION_DEBUG_END();

    // Create a mem context to store protocol objects
    if (protocolHelper.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            protocolHelper.memContext = memContextNew("ProtocolHelper");
        }
        MEM_CONTEXT_END();
    }

    // Create protocol object
    if (protocolHelper.remote == NULL)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            // Execute the protocol command
            protocolHelper.remoteExec = execNew(
                cfgOptionStr(cfgOptCmdSsh), protocolParam(remoteType, remoteId),
                strNewFmt("remote-%u process on '%s'", remoteId, strPtr(cfgOptionStr(cfgOptRepoHost))),
                (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * 1000));
            execOpen(protocolHelper.remoteExec);

            // Create protocol object
            protocolHelper.remote = protocolClientNew(
                strNewFmt("remote-%u protocol on '%s'", remoteId, strPtr(cfgOptionStr(cfgOptRepoHost))),
                PROTOCOL_SERVICE_REMOTE_STR, execIoRead(protocolHelper.remoteExec), execIoWrite(protocolHelper.remoteExec));

            protocolClientMove(protocolHelper.remote, execMemContext(protocolHelper.remoteExec));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_DEBUG_RESULT(PROTOCOL_CLIENT, protocolHelper.remote);
}

/***********************************************************************************************************************************
Free the protocol objects and shutdown processes
***********************************************************************************************************************************/
void
protocolFree(void)
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    if (protocolHelper.remote != NULL)
    {
        protocolClientFree(protocolHelper.remote);
        execFree(protocolHelper.remoteExec);

        protocolHelper.remote = NULL;
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
