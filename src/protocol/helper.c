/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/exec.h"
#include "common/io/client.h"
#include "common/io/socket/client.h"
#include "common/io/socket/server.h"
#include "common/io/tls/client.h"
#include "common/io/tls/server.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/exec.h"
#include "config/load.h"
#include "config/parse.h"
#include "config/protocol.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_SERVICE_LOCAL_STR,                           PROTOCOL_SERVICE_LOCAL);
STRING_EXTERN(PROTOCOL_SERVICE_REMOTE_STR,                          PROTOCOL_SERVICE_REMOTE);

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
typedef struct ProtocolHelperClient
{
    Exec *exec;                                                     // Executed client
    IoClient *ioClient;                                             // Io client, e.g. TlsClient
    IoSession *ioSession;                                           // Io session, e.g. TlsSession
    ProtocolClient *client;                                         // Protocol client
} ProtocolHelperClient;

static struct
{
    MemContext *memContext;                                         // Mem context for protocol helper

    unsigned int clientRemoteSize;                                  // Remote clients
    ProtocolHelperClient *clientRemote;

    unsigned int clientLocalSize;                                   // Local clients
    ProtocolHelperClient *clientLocal;
} protocolHelper;

/***********************************************************************************************************************************
Init local mem context and data structure
***********************************************************************************************************************************/
static void
protocolHelperInit(void)
{
    // In the protocol helper has not been initialized
    if (protocolHelper.memContext == NULL)
    {
        // Create a mem context to store protocol objects
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("ProtocolHelper")
            {
                protocolHelper.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }
}

/**********************************************************************************************************************************/
bool
repoIsLocal(unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, !cfgOptionIdxTest(cfgOptRepoHost, repoIdx));
}

/**********************************************************************************************************************************/
void
repoIsLocalVerify(void)
{
    FUNCTION_TEST_VOID();

    repoIsLocalVerifyIdx(cfgOptionGroupIdxDefault(cfgOptGrpRepo));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
repoIsLocalVerifyIdx(unsigned int repoIdx)
{
    FUNCTION_TEST_VOID();

    if (!repoIsLocal(repoIdx))
        THROW_FMT(HostInvalidError, "%s command must be run on the repository host", cfgCommandName());

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
pgIsLocal(unsigned int pgIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, !cfgOptionIdxTest(cfgOptPgHost, pgIdx));
}

/**********************************************************************************************************************************/
void
pgIsLocalVerify(void)
{
    FUNCTION_TEST_VOID();

    if (!pgIsLocal(cfgOptionGroupIdxDefault(cfgOptGrpPg)))
        THROW_FMT(HostInvalidError, "%s command must be run on the " PG_NAME " host", cfgCommandName());

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the command line required for local protocol execution
***********************************************************************************************************************************/
static StringList *
protocolLocalParam(ProtocolStorageType protocolStorageType, unsigned int hostIdx, unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Option replacements
        KeyValue *optionReplace = kvNew();

        // Add the process id -- used when more than one process will be called
        kvPut(optionReplace, VARSTRDEF(CFGOPT_PROCESS), VARUINT(processId));

        // Add the pg default. Don't do this for repos because the repo default should come from the user or the local should
        // handle all the repos equally. Repos don't get special handling like pg primaries or standbys.
        if (protocolStorageType == protocolStorageTypePg)
            kvPut(optionReplace, VARSTRDEF(CFGOPT_PG), VARUINT(cfgOptionGroupIdxToKey(cfgOptGrpPg, hostIdx)));

        // Add the remote type
        kvPut(optionReplace, VARSTRDEF(CFGOPT_REMOTE_TYPE), VARSTR(strIdToStr(protocolStorageType)));

        // Only enable file logging on the local when requested
        kvPut(
            optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_FILE),
            cfgOptionBool(cfgOptLogSubprocess) ? VARUINT64(cfgOptionStrId(cfgOptLogLevelFile)) : VARSTRDEF("off"));

        // Always output errors on stderr for debugging purposes
        kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_STDERR), VARSTRDEF("error"));

        // Disable output to stdout since it is used by the protocol
        kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_CONSOLE), VARSTRDEF("off"));

        result = strLstMove(cfgExecParam(cfgCommand(), cfgCmdRoleLocal, optionReplace, true, false), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
// Helper to execute the local process. This is a separate function solely so that it can be shimmed during testing.
static void
protocolLocalExec(
    ProtocolHelperClient *helper, ProtocolStorageType protocolStorageType, unsigned int hostIdx, unsigned int processId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, helper);
        FUNCTION_TEST_PARAM(ENUM, protocolStorageType);
        FUNCTION_TEST_PARAM(UINT, hostIdx);
        FUNCTION_TEST_PARAM(UINT, processId);
    FUNCTION_TEST_END();

    ASSERT(helper != NULL);

    // Execute the protocol command
    helper->exec = execNew(
        cfgExe(), protocolLocalParam(protocolStorageType, hostIdx, processId),
        strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u process", processId), cfgOptionUInt64(cfgOptProtocolTimeout));
    execOpen(helper->exec);

    // Create protocol object
    helper->client = protocolClientNew(
        strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u protocol", processId),
        PROTOCOL_SERVICE_LOCAL_STR, execIoRead(helper->exec), execIoWrite(helper->exec));

    // Move client to exec context so they are freed together
    protocolClientMove(helper->client, execMemContext(helper->exec));

    FUNCTION_TEST_RETURN_VOID();
}

ProtocolClient *
protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostIdx, unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    protocolHelperInit();

    // Allocate the client cache
    if (protocolHelper.clientLocalSize == 0)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            protocolHelper.clientLocalSize = cfgOptionUInt(cfgOptProcessMax) + 1;
            protocolHelper.clientLocal = memNew(protocolHelper.clientLocalSize * sizeof(ProtocolHelperClient));

            for (unsigned int clientIdx = 0; clientIdx < protocolHelper.clientLocalSize; clientIdx++)
                protocolHelper.clientLocal[clientIdx] = (ProtocolHelperClient){.exec = NULL};
        }
        MEM_CONTEXT_END();
    }

    ASSERT(processId <= protocolHelper.clientLocalSize);

    // Create protocol object
    ProtocolHelperClient *protocolHelperClient = &protocolHelper.clientLocal[processId - 1];

    if (protocolHelperClient->client == NULL)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            protocolLocalExec(protocolHelperClient, protocolStorageType, hostIdx, processId);
        }
        MEM_CONTEXT_END();

        // Send noop to catch initialization errors
        protocolClientNoOp(protocolHelperClient->client);
    }

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, protocolHelperClient->client);
}

/***********************************************************************************************************************************
Free the protocol client and underlying exec'd process. Log any errors as warnings since it is not worth terminating the process
while closing a local/remote that has already completed its work. The warning will be an indication that something is not right.
***********************************************************************************************************************************/
static void
protocolHelperClientFree(ProtocolHelperClient *protocolHelperClient)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, protocolHelperClient);
    FUNCTION_LOG_END();

    if (protocolHelperClient->client != NULL)
    {
        // Try to shutdown the protocol but only warn on error
        TRY_BEGIN()
        {
            protocolClientFree(protocolHelperClient->client);
        }
        CATCH_ANY()
        {
            LOG_WARN(errorMessage());
        }
        TRY_END();

        // Try to end the child process but only warn on error
        TRY_BEGIN()
        {
            execFree(protocolHelperClient->exec);
        }
        CATCH_ANY()
        {
            LOG_WARN(errorMessage());
        }
        TRY_END();

        // Free the io client/session (there should be no errors)
        ioSessionFree(protocolHelperClient->ioSession);
        ioClientFree(protocolHelperClient->ioClient);

        protocolHelperClient->ioSession = NULL;
        protocolHelperClient->ioClient = NULL;
        protocolHelperClient->client = NULL;
        protocolHelperClient->exec = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolLocalFree(unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    if (protocolHelper.clientLocal != NULL)
    {
        ASSERT(processId <= protocolHelper.clientLocalSize);
        protocolHelperClientFree(&protocolHelper.clientLocal[processId - 1]);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to check if client is authorized for a stanza
static bool
protocolServerAuthorize(const String *authListStr, const String *const stanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, authListStr);
        FUNCTION_LOG_PARAM(STRING, stanza);
    FUNCTION_TEST_END();

    ASSERT(authListStr != NULL);

    // Empty list is not valid. ??? It would be better if this were done during config parsing.
    authListStr = strTrim(strDup(authListStr));

    if (strEmpty(authListStr))
        THROW(OptionInvalidValueError, "'" CFGOPT_TLS_SERVER_AUTH "' option must have a value");

    // If * then all stanzas are authorized
    if (strEqZ(authListStr, "*"))
        FUNCTION_TEST_RETURN(true);

    // Check the list of stanzas for a match with the specified stanza. Each entry will need to be trimmed before comparing.
    if (stanza != NULL)
    {
        StringList *authList = strLstNewSplitZ(authListStr, COMMA_Z);

        for (unsigned int authListIdx = 0; authListIdx < strLstSize(authList); authListIdx++)
        {
            if (strEq(strTrim(strLstGet(authList, authListIdx)), stanza))
                FUNCTION_TEST_RETURN(true);
        }
    }

    FUNCTION_TEST_RETURN(false);
}

ProtocolServer *
protocolServer(IoServer *const tlsServer, IoSession *const socketSession)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_SERVER, tlsServer);
        FUNCTION_LOG_PARAM(IO_SESSION, socketSession);
    FUNCTION_LOG_END();

    ProtocolServer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Start TLS
        IoSession *const tlsSession = ioServerAccept(tlsServer, socketSession);

        result = protocolServerNew(
            PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(tlsSession),
            ioSessionIoWrite(tlsSession));

        // If session is authenticated
        if (ioSessionAuthenticated(tlsSession))
        {
            TRY_BEGIN()
            {
                // Get list of authorized stanzas for this client
                CHECK(AssertError, cfgOptionTest(cfgOptTlsServerAuth), "missing auth data");

                const String *const clientAuthList = strDup(
                    varStr(kvGet(cfgOptionKv(cfgOptTlsServerAuth), VARSTR(ioSessionPeerName(tlsSession)))));

                // Error if the client is not authorized for anything
                if (clientAuthList == NULL)
                    THROW(AccessError, "access denied");

                // Get parameter list from the client and load it
                const ProtocolServerCommandGetResult command = protocolServerCommandGet(result);
                CHECK(FormatError, command.id == PROTOCOL_COMMAND_CONFIG, "expected config command");

                StringList *const paramList = pckReadStrLstP(pckReadNew(command.param));
                strLstInsert(paramList, 0, cfgExe());
                cfgLoad(strLstSize(paramList), strLstPtr(paramList));

                // Error if the client is authorized for the requested stanza
                if (!protocolServerAuthorize(clientAuthList, cfgOptionStrNull(cfgOptStanza)))
                    THROW(AccessError, "access denied");
            }
            CATCH_ANY()
            {
                protocolServerError(result, errorCode(), STR(errorMessage()), STR(errorStackTrace()));
                RETHROW();
            }
            TRY_END();

            // Ack the config command
            protocolServerDataEndPut(result);

            ioSessionMove(tlsSession, memContextPrior());
            protocolServerMove(result, memContextPrior());
        }
        // Else the client can only detect that the server is alive
        else
        {
            // Send a data end message and return a NULL server. Do not waste time looking at what the client wrote.
            protocolServerDataEndPut(result);

            // Set result to NULL so there is no server for the caller to use. The TLS session will be freed when the temp mem
            // context ends.
            result = NULL;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER, result);
}

/***********************************************************************************************************************************
Get the command line required for remote protocol execution
***********************************************************************************************************************************/
static StringList *
protocolRemoteParam(ProtocolStorageType protocolStorageType, unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    // Is this a repo remote?
    bool isRepo = protocolStorageType == protocolStorageTypeRepo;

    // Option replacements
    KeyValue *optionReplace = kvNew();

    // Replace config options with the host versions
    unsigned int optConfig = isRepo ? cfgOptRepoHostConfig : cfgOptPgHostConfig;

    kvPut(
        optionReplace, VARSTRDEF(CFGOPT_CONFIG),
        cfgOptionIdxSource(optConfig, hostIdx) != cfgSourceDefault ? VARSTR(cfgOptionIdxStr(optConfig, hostIdx)) : NULL);

    unsigned int optConfigIncludePath = isRepo ? cfgOptRepoHostConfigIncludePath : cfgOptPgHostConfigIncludePath;

    kvPut(
        optionReplace, VARSTRDEF(CFGOPT_CONFIG_INCLUDE_PATH),
        cfgOptionIdxSource(optConfigIncludePath, hostIdx) != cfgSourceDefault ?
            VARSTR(cfgOptionIdxStr(optConfigIncludePath, hostIdx)) : NULL);

    unsigned int optConfigPath = isRepo ? cfgOptRepoHostConfigPath : cfgOptPgHostConfigPath;

    kvPut(
        optionReplace, VARSTRDEF(CFGOPT_CONFIG_PATH),
        cfgOptionIdxSource(optConfigPath, hostIdx) != cfgSourceDefault ? VARSTR(cfgOptionIdxStr(optConfigPath, hostIdx)) : NULL);

    // Update/remove repo/pg options that are sent to the remote
    for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
    {
        // Skip options that are not part of a group
        if (!cfgOptionGroup(optionId))
            continue;

        bool remove = false;
        bool skipHostZero = false;

        // Remove repo options that are not needed on the remote
        if (cfgOptionGroupId(optionId) == cfgOptGrpRepo)
        {
            // If remote type is pg then remove all repo options since they will not be used
            if (protocolStorageType == protocolStorageTypePg)
            {
                remove = true;
            }
            // Else remove repo options for indexes that are not being passed to the repo. This prevents the remote from getting
            // partial info about a repo, which could cause an error during validation.
            else
            {
                for (unsigned int optionIdx = 0; optionIdx < cfgOptionIdxTotal(optionId); optionIdx++)
                {
                    if (cfgOptionIdxTest(optionId, optionIdx) && optionIdx != hostIdx)
                        kvPut(optionReplace, VARSTRZ(cfgOptionIdxName(optionId, optionIdx)), NULL);
                }
            }
        }
        // Remove pg options that are not needed on the remote
        else
        {
            ASSERT(cfgOptionGroupId(optionId) == cfgOptGrpPg);

            // Remove unrequired/defaulted pg options when the remote type is repo since they won't be used
            if (protocolStorageType == protocolStorageTypeRepo)
            {
                remove = !cfgParseOptionRequired(cfgCommand(), optionId) || cfgParseOptionDefault(cfgCommand(), optionId) != NULL;
            }
            // Move pg options to host index 0 (key 1) so they will be in the default index on the remote host
            else
            {
                if (hostIdx != 0)
                {
                    kvPut(
                        optionReplace, VARSTRZ(cfgOptionIdxName(optionId, 0)),
                        cfgOptionIdxSource(optionId, hostIdx) != cfgSourceDefault ? cfgOptionIdxVar(optionId, hostIdx) : NULL);
                }

                remove = true;
                skipHostZero = true;
            }
        }

        // Remove options that have been marked for removal if they are not already null or invalid. This is more efficient because
        // cfgExecParam() won't have to search through as large a list looking for overrides.
        if (remove)
        {
            // Loop through option indexes
            for (unsigned int optionIdx = 0; optionIdx < cfgOptionIdxTotal(optionId); optionIdx++)
            {
                if (cfgOptionIdxTest(optionId, optionIdx) && !(skipHostZero && optionIdx == 0))
                    kvPut(optionReplace, VARSTRZ(cfgOptionIdxName(optionId, optionIdx)), NULL);
            }
        }
    }

    // Set repo default so the remote only operates on a single repo
    if (protocolStorageType == protocolStorageTypeRepo)
        kvPut(optionReplace, VARSTRDEF(CFGOPT_REPO), VARUINT(cfgOptionGroupIdxToKey(cfgOptGrpRepo, hostIdx)));

    // Add the process id if not set. This means that the remote is being started from the main process and should always get a
    // process id of 0.
    if (!cfgOptionTest(cfgOptProcess))
        kvPut(optionReplace, VARSTRDEF(CFGOPT_PROCESS), VARINT(0));

    // Don't pass log-path or lock-path since these are host specific
    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_PATH), NULL);
    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOCK_PATH), NULL);

    // Only enable file logging on the remote when requested
    kvPut(
        optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_FILE),
        cfgOptionBool(cfgOptLogSubprocess) ? VARUINT64(cfgOptionStrId(cfgOptLogLevelFile)) : VARSTRDEF("off"));

    // Always output errors on stderr for debugging purposes
    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_STDERR), VARSTRDEF("error"));

    // Disable output to stdout since it is used by the protocol
    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_CONSOLE), VARSTRDEF("off"));

    // Add the remote type
    kvPut(optionReplace, VARSTRDEF(CFGOPT_REMOTE_TYPE), VARSTR(strIdToStr(protocolStorageType)));

    FUNCTION_LOG_RETURN(STRING_LIST, cfgExecParam(cfgCommand(), cfgCmdRoleRemote, optionReplace, false, true));
}

// Helper to add SSH parameters when executing the remote via SSH
static StringList *
protocolRemoteParamSsh(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a repo remote?
        bool isRepo = protocolStorageType == protocolStorageTypeRepo;

        // Fixed parameters for ssh command
        result = strLstNew();
        strLstAddZ(result, "-o");
        strLstAddZ(result, "LogLevel=error");
        strLstAddZ(result, "-o");
        strLstAddZ(result, "Compression=no");
        strLstAddZ(result, "-o");
        strLstAddZ(result, "PasswordAuthentication=no");

        // Append port if specified
        ConfigOption optHostPort = isRepo ? cfgOptRepoHostPort : cfgOptPgHostPort;

        if (cfgOptionIdxTest(optHostPort, hostIdx))
        {
            strLstAddZ(result, "-p");
            strLstAdd(result, strNewFmt("%u", cfgOptionIdxUInt(optHostPort, hostIdx)));
        }

        // Append user/host
        strLstAdd(
            result,
            strNewFmt(
                "%s@%s", strZ(cfgOptionIdxStr(isRepo ? cfgOptRepoHostUser : cfgOptPgHostUser, hostIdx)),
                strZ(cfgOptionIdxStr(isRepo ? cfgOptRepoHost : cfgOptPgHost, hostIdx))));

        // Add remote command and parameters
        StringList *paramList = protocolRemoteParam(protocolStorageType, hostIdx);

        strLstInsert(paramList, 0, cfgOptionIdxStr(isRepo ? cfgOptRepoHostCmd : cfgOptPgHostCmd, hostIdx));
        strLstAdd(result, strLstJoin(paramList, " "));

        // Move to prior context
        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
// Helper to execute the local process. This is a separate function solely so that it can be shimmed during testing.
static void
protocolRemoteExec(
    ProtocolHelperClient *const helper, const ProtocolStorageType protocolStorageType, const unsigned int hostIdx,
    const unsigned int processId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, helper);
        FUNCTION_TEST_PARAM(ENUM, protocolStorageType);
        FUNCTION_TEST_PARAM(UINT, hostIdx);
        FUNCTION_TEST_PARAM(UINT, processId);
    FUNCTION_TEST_END();

    ASSERT(helper != NULL);

    // Get remote info
    const bool isRepo = protocolStorageType == protocolStorageTypeRepo;
    const StringId remoteType = cfgOptionIdxStrId(isRepo ? cfgOptRepoHostType : cfgOptPgHostType, hostIdx);
    const String *const host = cfgOptionIdxStr(isRepo ? cfgOptRepoHost : cfgOptPgHost, hostIdx);

    // Handle remote types
    IoRead *read;
    IoWrite *write;

    switch (remoteType)
    {
        // SSH remote
        case CFGOPTVAL_REPO_HOST_TYPE_SSH:
        {
            // Exec SSH
            helper->exec = execNew(
                cfgOptionStr(cfgOptCmdSsh), protocolRemoteParamSsh(protocolStorageType, hostIdx),
                strNewFmt(PROTOCOL_SERVICE_REMOTE "-%u process on '%s'", processId, strZ(host)),
                cfgOptionUInt64(cfgOptProtocolTimeout));
            execOpen(helper->exec);

            read = execIoRead(helper->exec);
            write = execIoWrite(helper->exec);

            break;
        }

        // TLS remote
        default:
        {
            ASSERT(remoteType == CFGOPTVAL_REPO_HOST_TYPE_TLS);

            // Negotiate TLS
            helper->ioClient = tlsClientNewP(
                sckClientNew(
                    host, cfgOptionIdxUInt(isRepo ? cfgOptRepoHostPort : cfgOptPgHostPort, hostIdx),
                    cfgOptionUInt64(cfgOptIoTimeout), cfgOptionUInt64(cfgOptProtocolTimeout)),
                host, cfgOptionUInt64(cfgOptIoTimeout), cfgOptionUInt64(cfgOptProtocolTimeout), true,
                .caFile = cfgOptionIdxStrNull(isRepo ? cfgOptRepoHostCaFile : cfgOptPgHostCaFile, hostIdx),
                .caPath = cfgOptionIdxStrNull(isRepo ? cfgOptRepoHostCaPath : cfgOptPgHostCaPath, hostIdx),
                .certFile = cfgOptionIdxStr(isRepo ? cfgOptRepoHostCertFile : cfgOptPgHostCertFile, hostIdx),
                .keyFile = cfgOptionIdxStr(isRepo ? cfgOptRepoHostKeyFile : cfgOptPgHostKeyFile, hostIdx),
                .crlFile = cfgOptionIdxStrNull(isRepo ? cfgOptRepoHostCrlFile : cfgOptPgHostCrlFile, hostIdx));
            helper->ioSession = ioClientOpen(helper->ioClient);

            read = ioSessionIoRead(helper->ioSession);
            write = ioSessionIoWrite(helper->ioSession);

            break;
        }
    }

    // Create protocol object
    helper->client = protocolClientNew(
        strNewFmt(PROTOCOL_SERVICE_REMOTE "-%u %s protocol on '%s'", processId, strZ(strIdToStr(remoteType)), strZ(host)),
        PROTOCOL_SERVICE_REMOTE_STR, read, write);

    // Remote initialization
    switch (remoteType)
    {
        // SSH remote
        case CFGOPTVAL_REPO_HOST_TYPE_SSH:
            // Move client to exec context so they are freed together
            protocolClientMove(helper->client, execMemContext(helper->exec));
            break;

        // TLS remote
        default:
        {
            ASSERT(remoteType == CFGOPTVAL_REPO_HOST_TYPE_TLS);

            // Pass parameters to server
            ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_CONFIG);
            pckWriteStrLstP(protocolCommandParam(command), protocolRemoteParam(protocolStorageType, hostIdx));
            protocolClientExecute(helper->client, command, false);

            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

ProtocolClient *
protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    // Is this a repo remote?
    bool isRepo = protocolStorageType == protocolStorageTypeRepo;

    protocolHelperInit();

    // Allocate the client cache
    if (protocolHelper.clientRemoteSize == 0)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            protocolHelper.clientRemoteSize = cfgOptionGroupIdxTotal(isRepo ? cfgOptGrpRepo : cfgOptGrpPg) + 1;
            protocolHelper.clientRemote = memNew(protocolHelper.clientRemoteSize * sizeof(ProtocolHelperClient));

            for (unsigned int clientIdx = 0; clientIdx < protocolHelper.clientRemoteSize; clientIdx++)
                protocolHelper.clientRemote[clientIdx] = (ProtocolHelperClient){.exec = NULL};
        }
        MEM_CONTEXT_END();
    }

    // Determine protocol id for the remote.  If the process option is set then use that since we want the remote protocol id to
    // match the local protocol id. Otherwise set to 0 since the remote is being started from a main process and there should only
    // be one remote per host.
    unsigned int processId = 0;

    if (cfgOptionTest(cfgOptProcess))
        processId = cfgOptionUInt(cfgOptProcess);

    CHECK(AssertError, hostIdx < protocolHelper.clientRemoteSize, "invalid host");

    // Create protocol object
    ProtocolHelperClient *protocolHelperClient = &protocolHelper.clientRemote[hostIdx];

    if (protocolHelperClient->client == NULL)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            protocolRemoteExec(protocolHelperClient, protocolStorageType, hostIdx, processId);

            // Send noop to catch initialization errors
            protocolClientNoOp(protocolHelperClient->client);

            // Get cipher options from the remote if none are locally configured
            if (isRepo && cfgOptionIdxStrId(cfgOptRepoCipherType, hostIdx) == cipherTypeNone)
            {
                // Options to query
                VariantList *param = varLstNew();
                varLstAdd(param, varNewStrZ(cfgOptionIdxName(cfgOptRepoCipherType, hostIdx)));
                varLstAdd(param, varNewStrZ(cfgOptionIdxName(cfgOptRepoCipherPass, hostIdx)));

                VariantList *optionList = configOptionRemote(protocolHelperClient->client, param);

                if (varUInt64(varLstGet(optionList, 0)) != cipherTypeNone)
                {
                    cfgOptionIdxSet(cfgOptRepoCipherType, hostIdx, cfgSourceConfig, varLstGet(optionList, 0));
                    cfgOptionIdxSet(cfgOptRepoCipherPass, hostIdx, cfgSourceConfig, varLstGet(optionList, 1));
                }
            }
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, protocolHelperClient->client);
}

/**********************************************************************************************************************************/
void
protocolRemoteFree(unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    if (protocolHelper.clientRemote != NULL)
        protocolHelperClientFree(&protocolHelper.clientRemote[hostIdx]);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolKeepAlive(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (protocolHelper.memContext != NULL)
    {
        for (unsigned int clientIdx  = 0; clientIdx < protocolHelper.clientRemoteSize; clientIdx++)
        {
            if (protocolHelper.clientRemote[clientIdx].client != NULL)
                protocolClientNoOp(protocolHelper.clientRemote[clientIdx].client);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolFree(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (protocolHelper.memContext != NULL)
    {
        // Free remotes
        for (unsigned int clientIdx = 0; clientIdx < protocolHelper.clientRemoteSize; clientIdx++)
            protocolRemoteFree(clientIdx);

        // Free locals
        for (unsigned int clientIdx = 1; clientIdx <= protocolHelper.clientLocalSize; clientIdx++)
            protocolLocalFree(clientIdx);
    }

    FUNCTION_LOG_RETURN_VOID();
}
