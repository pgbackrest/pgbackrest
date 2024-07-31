/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command/annotate/annotate.h"
#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/backup/backup.h"
#include "command/check/check.h"
#include "command/command.h"
#include "command/control/start.h"
#include "command/control/stop.h"
#include "command/exit.h"
#include "command/expire/expire.h"
#include "command/help/help.h"
#include "command/info/info.h"
#include "command/local/local.h"
#include "command/lock.h"
#include "command/manifest/manifest.h"
#include "command/remote/remote.h"
#include "command/repo/get.h"
#include "command/repo/ls.h"
#include "command/repo/put.h"
#include "command/repo/rm.h"
#include "command/restore/restore.h"
#include "command/server/ping.h"
#include "command/server/server.h"
#include "command/stanza/create.h"
#include "command/stanza/delete.h"
#include "command/stanza/upgrade.h"
#include "command/verify/verify.h"
#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/stat.h"
#include "config/config.h"
#include "config/load.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "storage/azure/helper.h"
#include "storage/cifs/helper.h"
#include "storage/gcs/helper.h"
#include "storage/helper.h"
#include "storage/s3/helper.h"
#include "storage/sftp/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Include automatically generated help data
***********************************************************************************************************************************/
#include "command/help/help.auto.c.inc"

int
main(int argListSize, const char *argList[])
{
    // Set stack trace and mem context error cleanup handlers
    static const ErrorHandlerFunction errorHandlerList[] = {stackTraceClean, memContextClean};
    errorHandlerSet(errorHandlerList, LENGTH_OF(errorHandlerList));

    // Set storage helpers
    static const StorageHelper storageHelperList[] =
    {
        STORAGE_AZURE_HELPER,
        STORAGE_CIFS_HELPER,
        STORAGE_GCS_HELPER,
        STORAGE_S3_HELPER,
#ifdef HAVE_LIBSSH2
        STORAGE_SFTP_HELPER,
#endif
        STORAGE_END_HELPER
    };

    storageHelperInit(storageHelperList);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    // Initialize command with the start time
    cmdInit();

    // Initialize statistics collector
    statInit();

    // Initialize exit handler
    exitInit();

    // Process commands
    volatile int result = 0;
    volatile bool error = false;

    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad((unsigned int)argListSize, argList);
        const ConfigCommandRole commandRole = cfgCommandRole();

        // Main/async commands
        // -------------------------------------------------------------------------------------------------------------------------
        if (commandRole == cfgCmdRoleMain || commandRole == cfgCmdRoleAsync)
        {
            switch (cfgCommandHelp() ? cfgCmdHelp : cfgCommand())
            {
                // Annotate command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdAnnotate:
                    cmdAnnotate();
                    break;

                // Archive get command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchiveGet:
                {
                    if (commandRole == cfgCmdRoleAsync)
                        cmdArchiveGetAsync();
                    else
                        result = cmdArchiveGet();

                    break;
                }

                // Archive push command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchivePush:
                {
                    if (commandRole == cfgCmdRoleAsync)
                        cmdArchivePushAsync();
                    else
                        cmdArchivePush();

                    break;
                }

                // Backup command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdBackup:
                {
                    // Run backup
                    cmdBackup();

                    if (cfgOptionBool(cfgOptExpireAuto))
                    {
                        // Switch to expire command
                        cmdEnd(0, NULL);
                        cfgCommandSet(cfgCmdExpire, cfgCmdRoleMain);
                        cfgLoadLogFile();
                        cmdBegin();

                        // Null out any backup percent complete value in the backup lock file
                        cmdLockWriteP();

                        // Run expire
                        cmdExpire();
                    }

                    break;
                }

                // Check command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdCheck:
                    cmdCheck();
                    break;

                // Expire command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdExpire:
                    cmdExpire();
                    break;

                // Info command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdInfo:
                    cmdInfo();
                    break;

                // Manifest command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdManifest:
                    cmdManifest();
                    break;

                // Repository get file command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRepoGet:
                    result = cmdStorageGet();
                    break;

                // Repository list paths/files command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRepoLs:
                    cmdStorageList();
                    break;

                // Repository put file command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRepoPut:
                    cmdStoragePut();
                    break;

                // Repository remove paths/files command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRepoRm:
                    cmdStorageRemove();
                    break;

                // Server command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdServer:
                    cmdServer((unsigned int)argListSize, argList);
                    break;

                // Server ping command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdServerPing:
                    cmdServerPing();
                    break;

                // Restore command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRestore:
                    cmdRestore();
                    break;

                // Stanza create command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaCreate:
                    cmdStanzaCreate();
                    break;

                // Stanza delete command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaDelete:
                    cmdStanzaDelete();
                    break;

                // Stanza upgrade command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaUpgrade:
                    cmdStanzaUpgrade();
                    break;

                // Start command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStart:
                    cmdStart();
                    break;

                // Stop command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStop:
                    cmdStop();
                    break;

                // Verify command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdVerify:
                    cmdVerify();
                    break;

                // Help/version commands
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdHelp:
                case cfgCmdVersion:
                    cmdHelp(BUF(helpData, sizeof(helpData)));
                    break;
            }
        }
        // Local/remote commands
        // -------------------------------------------------------------------------------------------------------------------------
        else
        {
            ASSERT(commandRole == cfgCmdRoleLocal || commandRole == cfgCmdRoleRemote);

            const String *const service = commandRole == cfgCmdRoleLocal ? PROTOCOL_SERVICE_LOCAL_STR : PROTOCOL_SERVICE_REMOTE_STR;
            const String *const name = strNewFmt("%s-%s", strZ(service), strZ(cfgOptionDisplay(cfgOptProcess)));
            const TimeMSec timeout = cfgOptionUInt64(cfgOptProtocolTimeout);
            ProtocolServer *const server = protocolServerNew(
                name, service, ioFdReadNewOpen(name, STDIN_FILENO, timeout), ioFdWriteNewOpen(name, STDOUT_FILENO, timeout));

            if (commandRole == cfgCmdRoleLocal)
                cmdLocal(server);
            else
                cmdRemote(server);
        }
    }
    CATCH_FATAL()
    {
        error = true;
        result = exitSafe(result, true, 0);
    }
    TRY_END();

    // Free protocol objects
    protocolFree();

    FUNCTION_LOG_RETURN(INT, error ? result : exitSafe(result, false, 0));
}
