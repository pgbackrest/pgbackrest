/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/check/check.h"
#include "command/command.h"
#include "command/control/start.h"
#include "command/control/stop.h"
#include "command/expire/expire.h"
#include "command/help/help.h"
#include "command/info/info.h"
#include "command/local/local.h"
#include "command/remote/remote.h"
#include "command/restore/restore.h"
#include "command/storage/list.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/exit.h"
#include "config/config.h"
#include "config/load.h"
#include "postgres/interface.h"
#include "perl/exec.h"
#include "version.h"

int
main(int argListSize, const char *argList[])
{
#ifdef WITH_BACKTRACE
    stackTraceInit(argList[0]);
#endif

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    // Initialize command with the start time
    cmdInit();

    volatile bool result = 0;
    volatile bool error = false;

    // Initialize exit handler
    exitInit();

    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad((unsigned int)argListSize, argList);

        // Display help
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandHelp())
        {
            cmdHelp();
        }
        else
        {
            switch (cfgCommand())
            {
                // Archive get command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchiveGet:
                {
                    result = cmdArchiveGet();
                    break;
                }

                // Archive get async command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchiveGetAsync:
                {
                    cmdArchiveGetAsync();
                    break;
                }

                // Archive push command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchivePush:
                {
                    cmdArchivePush();
                    break;
                }

                // Archive push async command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdArchivePushAsync:
                {
                    cmdArchivePushAsync();
                    break;
                }

                // Backup command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdBackup:
                {
#ifdef DEBUG
                    // Check pg_control during testing so errors are more obvious.  Otherwise errors only happen in
                    // archive-get/archive-push and end up in the PostgreSQL log which is not output in CI.  This can be removed
                    // once backup is written in C.
                    if (cfgOptionBool(cfgOptOnline) && !cfgOptionBool(cfgOptBackupStandby) && !cfgOptionTest(cfgOptPgHost))
                        pgControlFromFile(cfgOptionStr(cfgOptPgPath));
#endif

                    // Run backup
                    perlExec();

                    // Switch to expire command
                    cmdEnd(0, NULL);
                    cfgCommandSet(cfgCmdExpire);
                    cmdBegin(false);

                    // Run expire
                    perlExec();
                    cmdExpire();

                    break;
                }

                // Check command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdCheck:
                {
                    // Functionality is currently split between Perl and C
                    perlExec();
                    cmdCheck();
                    break;
                }

                // Expire command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdExpire:
                {
                    perlExec();
                    cmdExpire();
                    break;
                }

                // Help command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdHelp:
                case cfgCmdNone:
                {
                    THROW(AssertError, "'help' and 'none' commands should have been handled already");
                }

                // Info command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdInfo:
                {
                    cmdInfo();
                    break;
                }

                // Local command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdLocal:
                {
                    cmdLocal(STDIN_FILENO, STDOUT_FILENO);
                    break;
                }

                // Remote command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRemote:
                {
                    if (cfgOptionBool(cfgOptC))
                    {
                        cmdRemote(STDIN_FILENO, STDOUT_FILENO);
                    }
                    else
                        perlExec();

                    break;
                }

                // Restore command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdRestore:
                {
                    // Functionality is currently split between Perl and C
                    cmdRestore();
                    perlExec();
                    break;
                }

                // Stanza create command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaCreate:
                {
                    perlExec();
                    break;
                }

                // Stanza delete command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaDelete:
                {
                    perlExec();
                    break;
                }

                // Stanza upgrade command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStanzaUpgrade:
                {
                    perlExec();
                    break;
                }

                // Start command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStart:
                {
                    cmdStart();
                    break;
                }

                // Stop command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdStop:
                {
                    cmdStop();
                    break;
                }

                // Storage list command
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdLs:
                {
                    cmdStorageList();
                    break;
                }

                // Display version
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdVersion:
                {
                    printf(PROJECT_NAME " " PROJECT_VERSION "\n");
                    fflush(stdout);
                    break;
                }
            }
        }
    }
    CATCH_ANY()
    {
        error = true;
    }
    TRY_END();

    FUNCTION_LOG_RETURN(INT, exitSafe(result, error, 0));
}
