/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/lock.h"
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
typedef enum
{
    protocolClientLocal = STRID5("local", 0xc08dec0),
    protocolClientRemote = STRID5("remote", 0xb47b4b20),
} ProtocolClientType;

typedef struct ProtocolHelperClient
{
    ProtocolClientType type;                                        // Client type
    ProtocolStorageType storageType;                                // Storage type
    unsigned int hostIdx;                                           // Host index
    unsigned int processId;                                         // Process id displayed in logs
    Exec *exec;                                                     // Executed client
    IoClient *ioClient;                                             // Io client, e.g. TlsClient
    IoSession *ioSession;                                           // Io session, e.g. TlsSession
    ProtocolClient *client;                                         // Protocol client
} ProtocolHelperClient;

static struct
{
    MemContext *memContext;                                         // Mem context for protocol helper
    List *clientList;                                               // Client List
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
            MEM_CONTEXT_NEW_BEGIN("ProtocolHelper", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
            {
                protocolHelper.memContext = MEM_CONTEXT_NEW();
                protocolHelper.clientList = lstNewP(sizeof(ProtocolHelperClient));
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }
}

/***********************************************************************************************************************************
Init local mem context and data structure
***********************************************************************************************************************************/
static ProtocolHelperClient *
protocolHelperClientGet(ProtocolClientType type, ProtocolStorageType storageType, unsigned int hostIdx, unsigned int processId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(STRING_ID, storageType);
        FUNCTION_TEST_PARAM(UINT, hostIdx);
        FUNCTION_TEST_PARAM(UINT, processId);
    FUNCTION_TEST_END();

    ASSERT(protocolHelper.clientList != NULL);

    for (unsigned int clientIdx = 0; clientIdx < lstSize(protocolHelper.clientList); clientIdx++)
    {
        ProtocolHelperClient *const match = lstGet(protocolHelper.clientList, clientIdx);

        if (match->type == type && match->storageType == storageType && match->hostIdx == hostIdx && match->processId == processId)
            FUNCTION_TEST_RETURN_P(VOID, match);
    }

    FUNCTION_TEST_RETURN_P(VOID, NULL);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
repoIsLocal(const unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, !cfgOptionIdxTest(cfgOptRepoHost, repoIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pgIsLocal(const unsigned int pgIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, !cfgOptionIdxTest(cfgOptPgHost, pgIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN void
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
protocolLocalParam(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx, const unsigned int processId)
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
        KeyValue *const optionReplace = kvNew();

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

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = cfgExecParam(cfgCommand(), cfgCmdRoleLocal, optionReplace, true, false);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
// Helper to execute the local process. This is a separate function solely so that it can be shimmed during testing.
static void
protocolLocalExec(
    ProtocolHelperClient *const helper, const ProtocolStorageType protocolStorageType, const unsigned int hostIdx,
    const unsigned int processId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, helper);
        FUNCTION_TEST_PARAM(ENUM, protocolStorageType);
        FUNCTION_TEST_PARAM(UINT, hostIdx);
        FUNCTION_TEST_PARAM(UINT, processId);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(helper != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Execute the protocol command
        const String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u process", processId);
        const StringList *const param = protocolLocalParam(protocolStorageType, hostIdx, processId);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            helper->exec = execNew(cfgExe(), param, name, cfgOptionUInt64(cfgOptProtocolTimeout));
        }
        MEM_CONTEXT_PRIOR_END();

        execOpen(helper->exec);

        // Create protocol object
        name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-%u protocol", processId);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            helper->client = protocolClientNew(
                name, PROTOCOL_SERVICE_LOCAL_STR, execIoRead(helper->exec), execIoWrite(helper->exec));
        }
        MEM_CONTEXT_PRIOR_END();

        // Move client to exec context so they are freed together
        protocolClientMove(helper->client, execMemContext(helper->exec));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN ProtocolClient *
protocolLocalGet(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx, const unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    protocolHelperInit();

    // Create protocol object
    ProtocolHelperClient *protocolHelperClient = protocolHelperClientGet(
        protocolClientLocal, protocolStorageType, hostIdx, processId);

    if (protocolHelperClient == NULL)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            ProtocolHelperClient protocolHelperClientAdd =
            {
                .type = protocolClientLocal,
                .storageType = protocolStorageType,
                .hostIdx = hostIdx,
                .processId = processId,
            };

            protocolLocalExec(&protocolHelperClientAdd, protocolStorageType, hostIdx, processId);
            protocolHelperClient = lstAdd(protocolHelper.clientList, &protocolHelperClientAdd);
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
protocolHelperClientFree(ProtocolHelperClient *const protocolHelperClient)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, protocolHelperClient);
    FUNCTION_LOG_END();

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

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to check if client is authorized for a stanza
static bool
protocolServerAuthorize(const String *authListStr, const String *const stanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, authListStr);
        FUNCTION_TEST_PARAM(STRING, stanza);
    FUNCTION_TEST_END();

    ASSERT(authListStr != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Empty list is not valid. ??? It would be better if this were done during config parsing.
        authListStr = strTrim(strDup(authListStr));

        if (strEmpty(authListStr))
            THROW(OptionInvalidValueError, "'" CFGOPT_TLS_SERVER_AUTH "' option must have a value");

        // If * then all stanzas are authorized
        if (strEqZ(authListStr, "*"))
        {
            result = true;
        }
        // Else check the stanza list for a match with the specified stanza. Each entry will need to be trimmed before comparing.
        else if (stanza != NULL)
        {
            StringList *authList = strLstNewSplitZ(authListStr, ",");

            for (unsigned int authListIdx = 0; authListIdx < strLstSize(authList); authListIdx++)
            {
                if (strEq(strTrim(strLstGet(authList, authListIdx)), stanza))
                {
                    result = true;
                    break;
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

FN_EXTERN ProtocolServer *
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
            PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoReadP(tlsSession),
            ioSessionIoWrite(tlsSession));

        // If session is authenticated
        if (ioSessionAuthenticated(tlsSession))
        {
            TRY_BEGIN()
            {
                // Get parameter list from the client. This needs to happen even if we cannot authorize the client so that the
                // session id gets set for the error.
                const ProtocolServerRequestResult command = protocolServerRequest(result);
                CHECK(FormatError, command.id == PROTOCOL_COMMAND_CONFIG, "expected config command");

                // Get list of authorized stanzas for this client
                CHECK(AssertError, cfgOptionTest(cfgOptTlsServerAuth), "missing auth data");

                const String *const clientAuthList = strDup(
                    varStr(kvGet(cfgOptionKv(cfgOptTlsServerAuth), VARSTR(ioSessionPeerName(tlsSession)))));

                // Error if the client is not authorized for anything
                if (clientAuthList == NULL)
                    THROW(AccessError, "access denied");

                // Load parameter list from the client
                StringList *const paramList = pckReadStrLstP(pckReadNew(command.param));
                strLstInsert(paramList, 0, cfgExe());
                cfgLoad(strLstSize(paramList), strLstPtr(paramList));

                // Error if the client is not authorized for the requested stanza
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
            protocolServerResponseP(result);

            // Move result to prior context and move session into result so there is only one return value
            protocolServerMove(result, memContextPrior());
            ioSessionMove(tlsSession, objMemContext(result));
        }
        // Else the client can only detect that the server is alive
        else
        {
            // A noop command should have been received
            const ProtocolServerRequestResult command = protocolServerRequest(result);
            CHECK(FormatError, command.id == PROTOCOL_COMMAND_NOOP, "expected config command");

            // Send a data end message and return a NULL server
            protocolServerResponseP(result);

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
protocolRemoteParam(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a repo remote?
        const bool isRepo = protocolStorageType == protocolStorageTypeRepo;

        // Option replacements
        KeyValue *const optionReplace = kvNew();

        // Replace config options with the host versions
        const unsigned int optConfig = isRepo ? cfgOptRepoHostConfig : cfgOptPgHostConfig;

        kvPut(
            optionReplace, VARSTRDEF(CFGOPT_CONFIG),
            cfgOptionIdxSource(optConfig, hostIdx) != cfgSourceDefault ? VARSTR(cfgOptionIdxStr(optConfig, hostIdx)) : NULL);

        const unsigned int optConfigIncludePath = isRepo ? cfgOptRepoHostConfigIncludePath : cfgOptPgHostConfigIncludePath;

        kvPut(
            optionReplace, VARSTRDEF(CFGOPT_CONFIG_INCLUDE_PATH),
            cfgOptionIdxSource(optConfigIncludePath, hostIdx) != cfgSourceDefault ?
                VARSTR(cfgOptionIdxStr(optConfigIncludePath, hostIdx)) : NULL);

        const unsigned int optConfigPath = isRepo ? cfgOptRepoHostConfigPath : cfgOptPgHostConfigPath;

        kvPut(
            optionReplace, VARSTRDEF(CFGOPT_CONFIG_PATH),
            cfgOptionIdxSource(optConfigPath, hostIdx) != cfgSourceDefault ?
                VARSTR(cfgOptionIdxStr(optConfigPath, hostIdx)) : NULL);

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
                    remove =
                        !cfgParseOptionRequired(cfgCommand(), optionId) ||
                        cfgParseOptionDefault(cfgCommand(), optionId) != NULL;
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

            // Remove options that have been marked for removal if they are not already null or invalid. This is more efficient
            // because cfgExecParam() won't have to search through as large a list looking for overrides.
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
        {
            kvPut(optionReplace, VARSTRDEF(CFGOPT_REPO), VARUINT(cfgOptionGroupIdxToKey(cfgOptGrpRepo, hostIdx)));
        }
        // Else remove repo selection when the remote is pg
        else
            kvPut(optionReplace, VARSTRDEF(CFGOPT_REPO), NULL);

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

        // Add lock required on the remote
        if (cfgLockRemoteRequired())
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOCK), varNewVarLst(varLstNewStrLst(cmdLockList())));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = cfgExecParam(cfgCommand(), cfgCmdRoleRemote, optionReplace, false, true);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

// Helper to add SSH parameters when executing the remote via SSH
static StringList *
protocolRemoteParamSsh(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
    FUNCTION_LOG_END();

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a repo remote?
        const bool isRepo = protocolStorageType == protocolStorageTypeRepo;

        // Fixed parameters for ssh command
        strLstAddZ(result, "-o");
        strLstAddZ(result, "LogLevel=error");
        strLstAddZ(result, "-o");
        strLstAddZ(result, "Compression=no");
        strLstAddZ(result, "-o");
        strLstAddZ(result, "PasswordAuthentication=no");

        // Append port if specified
        const ConfigOption optHostPort = isRepo ? cfgOptRepoHostPort : cfgOptPgHostPort;

        if (cfgOptionIdxTest(optHostPort, hostIdx))
        {
            strLstAddZ(result, "-p");
            strLstAddFmt(result, "%u", cfgOptionIdxUInt(optHostPort, hostIdx));
        }

        // Append user/host
        strLstAddFmt(
            result, "%s@%s", strZ(cfgOptionIdxStr(isRepo ? cfgOptRepoHostUser : cfgOptPgHostUser, hostIdx)),
            strZ(cfgOptionIdxStr(isRepo ? cfgOptRepoHost : cfgOptPgHost, hostIdx)));

        // Add remote command and parameters
        StringList *const paramList = protocolRemoteParam(protocolStorageType, hostIdx);

        strLstInsert(paramList, 0, cfgOptionIdxStr(isRepo ? cfgOptRepoHostCmd : cfgOptPgHostCmd, hostIdx));
        strLstAdd(result, strLstJoin(paramList, " "));
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

    FUNCTION_AUDIT_HELPER();

    ASSERT(helper != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
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
                const String *const name = strNewFmt(PROTOCOL_SERVICE_REMOTE "-%u process on '%s'", processId, strZ(host));
                const StringList *const param = protocolRemoteParamSsh(protocolStorageType, hostIdx);

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    helper->exec = execNew(cfgOptionStr(cfgOptCmdSsh), param, name, cfgOptionUInt64(cfgOptProtocolTimeout));
                }
                MEM_CONTEXT_PRIOR_END();

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
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    helper->ioClient = tlsClientNewP(
                        sckClientNew(
                            host, cfgOptionIdxUInt(isRepo ? cfgOptRepoHostPort : cfgOptPgHostPort, hostIdx),
                            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionUInt64(cfgOptProtocolTimeout)),
                        host, cfgOptionUInt64(cfgOptIoTimeout), cfgOptionUInt64(cfgOptProtocolTimeout), true,
                        .caFile = cfgOptionIdxStrNull(isRepo ? cfgOptRepoHostCaFile : cfgOptPgHostCaFile, hostIdx),
                        .caPath = cfgOptionIdxStrNull(isRepo ? cfgOptRepoHostCaPath : cfgOptPgHostCaPath, hostIdx),
                        .certFile = cfgOptionIdxStr(isRepo ? cfgOptRepoHostCertFile : cfgOptPgHostCertFile, hostIdx),
                        .keyFile = cfgOptionIdxStr(isRepo ? cfgOptRepoHostKeyFile : cfgOptPgHostKeyFile, hostIdx));

                    helper->ioSession = ioClientOpen(helper->ioClient);
                }
                MEM_CONTEXT_PRIOR_END();

                read = ioSessionIoReadP(helper->ioSession);
                write = ioSessionIoWrite(helper->ioSession);

                break;
            }
        }

        // Create protocol object
        const String *const name = strNewFmt(
            PROTOCOL_SERVICE_REMOTE "-%u %s protocol on '%s'", processId, strZ(strIdToStr(remoteType)), strZ(host));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            helper->client = protocolClientNew(name, PROTOCOL_SERVICE_REMOTE_STR, read, write);
        }
        MEM_CONTEXT_PRIOR_END();

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

                TRY_BEGIN()
                {
                    // Pass parameters to server
                    PackWrite *const param = protocolPackNew();

                    pckWriteStrLstP(param, protocolRemoteParam(protocolStorageType, hostIdx));
                    protocolClientRequestP(helper->client, PROTOCOL_COMMAND_CONFIG, .param = param);
                }
                CATCH_ANY()
                {
                    // Clear the callback so the client does not try to shut down the connection. Attempting to shut down the
                    // connection will fail since the server has already disconnected and a new error will be thrown, masking the
                    // original error.
                    memContextCallbackClear(objMemContext(helper->client));
                    RETHROW();
                }
                TRY_END();

                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN ProtocolClient *
protocolRemoteGet(const ProtocolStorageType protocolStorageType, const unsigned int hostIdx, const bool create)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, protocolStorageType);
        FUNCTION_LOG_PARAM(UINT, hostIdx);
        FUNCTION_LOG_PARAM(BOOL, create);
    FUNCTION_LOG_END();

    // Is this a repo remote?
    const bool isRepo = protocolStorageType == protocolStorageTypeRepo;

    protocolHelperInit();

    // Determine protocol id for the remote. If the process option is set then use that since we want the remote protocol id to
    // match the local protocol id. Otherwise set to 0 since the remote is being started from a main process and there should only
    // be one remote per host.
    const unsigned int processId = cfgOptionTest(cfgOptProcess) ? cfgOptionUInt(cfgOptProcess) : 0;

    // Create protocol object
    ProtocolHelperClient *protocolHelperClient = protocolHelperClientGet(
        protocolClientRemote, protocolStorageType, hostIdx, processId);

    if (protocolHelperClient == NULL && create)
    {
        MEM_CONTEXT_BEGIN(protocolHelper.memContext)
        {
            ProtocolHelperClient protocolHelperClientAdd =
            {
                .type = protocolClientRemote,
                .storageType = protocolStorageType,
                .hostIdx = hostIdx,
                .processId = processId,
            };

            protocolRemoteExec(&protocolHelperClientAdd, protocolStorageType, hostIdx, processId);
            protocolHelperClient = lstAdd(protocolHelper.clientList, &protocolHelperClientAdd);

            // Send noop to catch initialization errors
            protocolClientNoOp(protocolHelperClient->client);

            // Get cipher options from the remote if none are locally configured
            if (isRepo && cfgOptionIdxStrId(cfgOptRepoCipherType, hostIdx) == cipherTypeNone)
            {
                // Options to query
                VariantList *const param = varLstNew();
                varLstAdd(param, varNewStrZ(cfgOptionIdxName(cfgOptRepoCipherType, hostIdx)));
                varLstAdd(param, varNewStrZ(cfgOptionIdxName(cfgOptRepoCipherPass, hostIdx)));

                const VariantList *const optionList = configOptionRemote(protocolHelperClient->client, param);

                if (varUInt64(varLstGet(optionList, 0)) != cipherTypeNone)
                {
                    cfgOptionIdxSet(cfgOptRepoCipherType, hostIdx, cfgSourceConfig, varLstGet(optionList, 0));
                    cfgOptionIdxSet(cfgOptRepoCipherPass, hostIdx, cfgSourceConfig, varLstGet(optionList, 1));
                }
            }
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(PROTOCOL_CLIENT, protocolHelperClient != NULL ? protocolHelperClient->client : NULL);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolHelperFree(ProtocolClient *const client)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
    FUNCTION_LOG_END();

    if (client != NULL && protocolHelper.clientList != NULL)
    {
        for (unsigned int clientIdx = 0; clientIdx < lstSize(protocolHelper.clientList); clientIdx++)
        {
            ProtocolHelperClient *const match = lstGet(protocolHelper.clientList, clientIdx);

            if (match->client == client)
            {
                protocolHelperClientFree(match);
                lstRemoveIdx(protocolHelper.clientList, clientIdx);

                break;
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolKeepAlive(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (protocolHelper.clientList)
    {
        for (unsigned int clientIdx = 0; clientIdx < lstSize(protocolHelper.clientList); clientIdx++)
        {
            ProtocolHelperClient *const match = lstGet(protocolHelper.clientList, clientIdx);

            if (match->type == protocolClientRemote)
                protocolClientNoOp(match->client);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolFree(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (protocolHelper.clientList != NULL)
    {
        while (lstSize(protocolHelper.clientList) > 0)
        {
            protocolHelperClientFree(lstGet(protocolHelper.clientList, 0));
            lstRemoveIdx(protocolHelper.clientList, 0);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}
