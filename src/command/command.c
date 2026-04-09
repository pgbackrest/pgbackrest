/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#include <build.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command/command.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/stat.h"
#include "common/time.h"
#include "common/type/json.h"
#include "config/config.intern.h"
#include "config/parse.h"
#include "version.h"

/***********************************************************************************************************************************
Track time command started
***********************************************************************************************************************************/
static TimeMSec timeBegin;
static String *cmdOptionStr;

/**********************************************************************************************************************************/
FN_EXTERN void
cmdInit(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    timeBegin = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
cmdOption(void)
{
    FUNCTION_TEST_VOID();

    if (cmdOptionStr == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            cmdOptionStr = strNew();

            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Add command parameters if they exist
                const StringList *const commandParamList = cfgCommandParam();

                if (!strLstEmpty(commandParamList))
                {
                    strCatZ(cmdOptionStr, " [");

                    for (unsigned int commandParamIdx = 0; commandParamIdx < strLstSize(commandParamList); commandParamIdx++)
                    {
                        const String *commandParam = strLstGet(commandParamList, commandParamIdx);

                        if (commandParamIdx != 0)
                            strCatZ(cmdOptionStr, ", ");

                        if (strchr(strZ(commandParam), ' ') != NULL)
                            commandParam = strNewFmt("\"%s\"", strZ(commandParam));

                        strCat(cmdOptionStr, commandParam);
                    }

                    strCatZ(cmdOptionStr, "]");
                }

                // Loop though options and add the ones that are interesting
                for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    // Skip the option if not valid for this command. Generally only one command runs at a time, but sometimes
                    // commands are chained together (e.g. backup and expire) and the second command may not use all the options of
                    // the first command. Displaying them is harmless but might cause confusion.
                    if (!cfgOptionValid(optionId) || !cfgParseOptionValid(cfgCommand(), cfgCommandRole(), optionId))
                        continue;

                    // Loop through option indexes
                    const unsigned int optionIdxTotal =
                        cfgOptionGroup(optionId) ? cfgOptionGroupIdxTotal(cfgOptionGroupId(optionId)) : 1;

                    for (unsigned int optionIdx = 0; optionIdx < optionIdxTotal; optionIdx++)
                    {
                        // If option was negated
                        if (cfgOptionIdxNegate(optionId, optionIdx))
                            strCatFmt(cmdOptionStr, " --no-%s", cfgOptionIdxName(optionId, optionIdx));
                        // If option was reset
                        else if (cfgOptionIdxReset(optionId, optionIdx))
                            strCatFmt(cmdOptionStr, " --reset-%s", cfgOptionIdxName(optionId, optionIdx));
                        // Else not default
                        else if (cfgOptionIdxSource(optionId, optionIdx) != cfgSourceDefault)
                        {
                            // Don't show redacted options
                            if (cfgParseOptionSecure(optionId))
                                strCatFmt(cmdOptionStr, " --%s=<redacted>", cfgOptionIdxName(optionId, optionIdx));
                            // Output boolean option
                            else if (cfgParseOptionType(optionId) == cfgOptTypeBoolean)
                                strCatFmt(cmdOptionStr, " --%s", cfgOptionIdxName(optionId, optionIdx));
                            // Output other options
                            else
                            {
                                StringList *valueList = NULL;

                                // Generate the values of hash options
                                if (cfgParseOptionType(optionId) == cfgOptTypeHash)
                                {
                                    valueList = strLstNew();

                                    const KeyValue *const optionKv = cfgOptionIdxKv(optionId, optionIdx);
                                    const VariantList *const keyList = kvKeyList(optionKv);

                                    for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
                                    {
                                        strLstAddFmt(
                                            valueList, "%s=%s", strZ(varStr(varLstGet(keyList, keyIdx))),
                                            strZ(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx)))));
                                    }
                                }
                                // Generate values for list options
                                else if (cfgParseOptionType(optionId) == cfgOptTypeList)
                                {
                                    valueList = strLstNewVarLst(cfgOptionIdxLst(optionId, optionIdx));
                                }
                                // Else only one value
                                else
                                {
                                    valueList = strLstNew();
                                    strLstAdd(valueList, cfgOptionIdxDisplay(optionId, optionIdx));
                                }

                                // Output options and values
                                for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                                {
                                    const String *value = strLstGet(valueList, valueListIdx);

                                    strCatFmt(cmdOptionStr, " --%s", cfgOptionIdxName(optionId, optionIdx));

                                    if (strchr(strZ(value), ' ') != NULL)
                                        value = strNewFmt("\"%s\"", strZ(value));

                                    strCatFmt(cmdOptionStr, "=%s", strZ(value));
                                }
                            }
                        }
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(STRING, cmdOptionStr);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdBegin(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    ASSERT(cfgInited());

    // This is fairly expensive log message to generate so skip it if it won't be output
    if (logAny(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Basic info on command start
            String *const info = strCatFmt(strNew(), "%s command begin", strZ(cfgCommandRoleName()));

            // Free the old option string if it exists. This is needed when more than one command is run in a row so an option
            // string gets created for the new command.
            strFree(cmdOptionStr);
            cmdOptionStr = NULL;

            // Add version and options
            strCatFmt(info, " %s:%s", PROJECT_VERSION, strZ(cmdOption()));

            LOG(cfgLogLevelDefault(), 0, strZ(info));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdEnd(const int code, const String *const errorMessage)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, errorMessage);
    FUNCTION_LOG_END();

    ASSERT(cfgInited());

    // Skip this log message if it won't be output. It's not too expensive but since we skipped cmdBegin(), may as well.
    if (logAny(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Output statistics if there are any
            const String *const statJson = statToJson();

            if (statJson != NULL)
                LOG_DETAIL_FMT("statistics: %s", strZ(statJson));

            // Basic info on command end
            String *const info = strCatFmt(strNew(), "%s command end: ", strZ(cfgCommandRoleName()));

            if (errorMessage == NULL)
            {
                strCatZ(info, "completed successfully");

                if (cfgOptionBool(cfgOptLogTimestamp))
                    strCatFmt(info, " (%" PRIu64 "ms)", timeMSec() - timeBegin);
            }
            else
                strCat(info, errorMessage);

            LOG(cfgLogLevelDefault(), 0, strZ(info));
        }
        MEM_CONTEXT_TEMP_END();
    }

    // Reset timeBegin in case there is another command following this one
    timeBegin = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdAsyncExec(const char *const command, const StringList *const commandExec)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, command);
        FUNCTION_LOG_PARAM(STRING_LIST, commandExec);
    FUNCTION_LOG_END();

    ASSERT(commandExec != NULL);

    // Fork off the async process
    const pid_t pid = forkSafe();

    if (pid == 0)
    {
        // Disable logging and close log file
        logClose();

        // Detach from parent process
        forkDetach();

        // Close any open file descriptors above the standard three (stdin, stdout, stderr). Don't check the return value since we
        // don't know which file descriptors are actually open (might be none). It's possible that there are open files >= 1024 but
        // there is no easy way to detect that and this should give us enough descriptors to do our work.
        for (int fd = 3; fd < 1024; fd++)
            close(fd);

        // Execute the binary. This statement will not return if it is successful. execvp() requires non-const parameters because it
        // modifies them after the fork.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        THROW_ON_SYS_ERROR_FMT(
            execvp(strZ(strLstGet(commandExec, 0)), UNCONSTIFY(char **, strLstPtr(commandExec))) == -1, ExecuteError,
            "unable to execute asynchronous '%s'", command);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG_EXEC_TIME
    // Get the time to measure how long it takes for the forked process to exit
    const TimeMSec timeBegin = timeMSec();
#endif

    // The process that was just forked should return immediately
    int processStatus;

    THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

    // The first fork should exit with success. If not, something went wrong during the second fork.
    CHECK(ExecuteError, WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0, "error on first fork");

#ifdef DEBUG_EXEC_TIME
    // If the process does not exit immediately then something probably went wrong with the double fork. It's possible that this
    // test will fail on very slow systems so it may need to be tuned. The idea is to make sure that the waitpid() above is not
    // waiting on the async process.
    CHECK(AssertError, timeMSec() - timeBegin < 10, "the process does not exit immediately");
#endif

    FUNCTION_LOG_RETURN_VOID();
}
