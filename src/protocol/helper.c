/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/exec.h"
#include "common/memContext.h"
#include "crypto/crypto.h"
#include "config/config.h"
#include "config/exec.h"
#include "config/protocol.h"
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
    FUNCTION_TEST_RETURN(!cfgOptionTest(cfgOptRepoHost));
}

/***********************************************************************************************************************************
Get the command line required for protocol execution
***********************************************************************************************************************************/
static StringList *
protocolParam(RemoteType remoteType, unsigned int remoteId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, remoteType);
        FUNCTION_LOG_PARAM(UINT, remoteId);
    FUNCTION_LOG_END();

    ASSERT(remoteType == remoteTypeRepo);                           // ??? Hard-coded until the function supports db remotes
    ASSERT(remoteId == 1);                                          // ??? Hard-coded until the function supports db remotes

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

    // Don't pass the stanza if it is set.  It is better if the remote is stanza-agnostic so the client can operate on multiple
    // stanzas without starting a new remote.  Once the Perl code is removed the stanza option can be removed from the remote
    // command.
    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptStanza))), NULL);

    // Add the type
    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptType))), varNewStr(strNew("backup")));

    StringList *commandExec = cfgExecParam(cfgCmdRemote, optionReplace);
    strLstInsert(commandExec, 0, cfgOptionStr(cfgOptRepoHostCmd));
    strLstAdd(result, strLstJoin(commandExec, " "));

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Get the protocol client
***********************************************************************************************************************************/
ProtocolClient *
protocolGet(RemoteType remoteType, unsigned int remoteId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, remoteType);
        FUNCTION_LOG_PARAM(UINT, remoteId);
    FUNCTION_LOG_END();

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

            // Get cipher options from the remote if none are locally configured
            if (strEq(cfgOptionStr(cfgOptRepoCipherType), CIPHER_TYPE_NONE_STR))
            {
                // Options to query
                VariantList *param = varLstNew();
                varLstAdd(param, varNewStr(strNew(cfgOptionName(cfgOptRepoCipherType))));
                varLstAdd(param, varNewStr(strNew(cfgOptionName(cfgOptRepoCipherPass))));

                VariantList *optionList = configProtocolOption(protocolHelper.remote, param);

                if (!strEq(varStr(varLstGet(optionList, 0)), CIPHER_TYPE_NONE_STR))
                {
                    cfgOptionSet(cfgOptRepoCipherType, cfgSourceConfig, varLstGet(optionList, 0));
                    cfgOptionSet(cfgOptRepoCipherPass, cfgSourceConfig, varLstGet(optionList, 1));
                }
            }

            protocolClientMove(protocolHelper.remote, execMemContext(protocolHelper.remoteExec));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, protocolHelper.remote);
}

/***********************************************************************************************************************************
Free the protocol objects and shutdown processes
***********************************************************************************************************************************/
void
protocolFree(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (protocolHelper.remote != NULL)
    {
        protocolClientFree(protocolHelper.remote);
        execFree(protocolHelper.remoteExec);

        protocolHelper.remote = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}
