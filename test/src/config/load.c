/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/command.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.intern.h"
#include "config/load.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Load log settings
***********************************************************************************************************************************/
static void
cfgLoadLogSetting(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    // Initialize logging
    LogLevel logLevelConsole = logLevelOff;
    bool logTimestamp = true;

    if (cfgOptionValid(cfgOptLogLevel))
        logLevelConsole = logLevelEnum(cfgOptionStrId(cfgOptLogLevel));

    if (cfgOptionValid(cfgOptLogTimestamp))
        logTimestamp = cfgOptionBool(cfgOptLogTimestamp);

    logInit(logLevelConsole, logLevelOff, logLevelOff, logTimestamp, 0, 1, false);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Update options that have complex rules
***********************************************************************************************************************************/
static void
cfgLoadUpdateOption(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    // Get current working dir
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // If repo-path is relative then make it absolute
    if (cfgOptionValid(cfgOptRepoPath))
    {
        const String *const repoPath = cfgOptionStr(cfgOptRepoPath);

        if (!strBeginsWithZ(repoPath, "/"))
        {
            cfgOptionSet(
                cfgOptRepoPath, cfgOptionSource(cfgOptRepoPath), VARSTR(strNewFmt("%s/%s", currentWorkDir, strZ(repoPath))));
        }
    }

    // If test-path is relative then make it absolute
    if (cfgOptionValid(cfgOptTestPath))
    {
        const String *const testPath = cfgOptionStr(cfgOptTestPath);

        if (!strBeginsWithZ(testPath, "/"))
        {
            cfgOptionSet(
                cfgOptTestPath, cfgOptionSource(cfgOptTestPath), VARSTR(strNewFmt("%s/%s", currentWorkDir, strZ(testPath))));
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cfgLoad(unsigned int argListSize, const char *argList[])
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make a copy of the arguments so they can be manipulated
        StringList *const argListNew = strLstNew();

        for (unsigned int argListIdx = 0; argListIdx < argListSize; argListIdx++)
            strLstAddZ(argListNew, argList[argListIdx]);

        // Parse config from command line
        TRY_BEGIN()
        {
            cfgParseP(storagePosixNewP(FSLASH_STR), strLstSize(argListNew), strLstPtr(argListNew), .noConfigLoad = true);
        }
        CATCH(CommandRequiredError)
        {
            strLstAddZ(argListNew, CFGCMD_TEST);
            cfgParseP(storagePosixNewP(FSLASH_STR), strLstSize(argListNew), strLstPtr(argListNew), .noConfigLoad = true);
        }
        TRY_END();

        // Error on noop command. This command is required to hold options that must be declared but are unused by test.
        if (cfgCommand() == cfgCmdNoop)
            THROW(CommandInvalidError, "invalid command '" CFGCMD_NOOP "'");

        // Load the log settings
        cfgLoadLogSetting();

        // Neutralize the umask to make the repository file/path modes more consistent
        if (cfgOptionValid(cfgOptNeutralUmask) && cfgOptionBool(cfgOptNeutralUmask))
            umask(0000);

        // Set IO buffer size
        if (cfgOptionValid(cfgOptBufferSize))
            ioBufferSizeSet(cfgOptionUInt(cfgOptBufferSize));

        // Update options that have complex rules
        cfgLoadUpdateOption();

        // Begin the command
        cmdBegin();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
