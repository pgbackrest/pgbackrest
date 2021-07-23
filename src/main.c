/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/backup/backup.h"
#include "command/check/check.h"
#include "command/command.h"
#include "command/control/start.h"
#include "command/control/stop.h"
#include "command/expire/expire.h"
#include "command/help/help.h"
#include "command/info/info.h"
#include "command/local/local.h"
#include "command/remote/remote.h"
#include "command/repo/create.h"
#include "command/repo/get.h"
#include "command/repo/ls.h"
#include "command/repo/put.h"
#include "command/repo/rm.h"
#include "command/restore/restore.h"
#include "command/stanza/create.h"
#include "command/stanza/delete.h"
#include "command/stanza/upgrade.h"
#include "command/verify/verify.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/exit.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/stat.h"
#include "config/config.h"
#include "config/load.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "storage/helper.h"
#include "version.h"

int
main(int argListSize, const char *argList[])
{
    // Set stack trace and mem context error cleanup handlers
    static const ErrorHandlerFunction errorHandlerList[] = {stackTraceClean, memContextClean};
    errorHandlerSet(errorHandlerList, sizeof(errorHandlerList) / sizeof(ErrorHandlerFunction));

#ifdef WITH_BACKTRACE
    stackTraceInit(argList[0]);
#endif

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
        ConfigCommandRole commandRole = cfgCommandRole();

        // Display help
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandHelp())
        {
            cmdHelp();
        }
        // Local role
        // -------------------------------------------------------------------------------------------------------------------------
        else if (commandRole == cfgCmdRoleLocal)
        {
            String *name = strNewFmt(PROTOCOL_SERVICE_LOCAL "-%s", strZ(cfgOptionDisplay(cfgOptProcess)));

            cmdLocal(
                protocolServerNew(
                    name, PROTOCOL_SERVICE_LOCAL_STR, ioFdReadNewOpen(name, STDIN_FILENO, cfgOptionUInt64(cfgOptProtocolTimeout)),
                    ioFdWriteNewOpen(name, STDOUT_FILENO, cfgOptionUInt64(cfgOptProtocolTimeout))));
        }
        // Remote role
        // -------------------------------------------------------------------------------------------------------------------------
        else if (commandRole == cfgCmdRoleRemote)
        {
            String *name = strNewFmt(PROTOCOL_SERVICE_REMOTE "-%s", strZ(cfgOptionDisplay(cfgOptProcess)));

            cmdRemote(
                protocolServerNew(
                    name, PROTOCOL_SERVICE_REMOTE_STR, ioFdReadNewOpen(name, STDIN_FILENO, cfgOptionUInt64(cfgOptProtocolTimeout)),
                    ioFdWriteNewOpen(name, STDOUT_FILENO, cfgOptionUInt64(cfgOptProtocolTimeout))));
        }
        else
        {
            switch (cfgCommand())
            {
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

                // Help command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdHelp:
                case cfgCmdNone:
                    THROW(AssertError, "'help' and 'none' commands should have been handled already");

                // Info command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdInfo:
                    cmdInfo();
                    break;

                // Repository create command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRepoCreate:
                    cmdRepoCreate();
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

                // Display version
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdVersion:
                    printf(PROJECT_NAME " " PROJECT_VERSION "\n");
                    fflush(stdout);
                    break;
            }
        }
    }
    CATCH_ANY()
    {
        error = true;
        result = exitSafe(result, true, 0);
    }
    TRY_END();

    FUNCTION_LOG_RETURN(INT, error ? result : exitSafe(result, false, 0));
}
