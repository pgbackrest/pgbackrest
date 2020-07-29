/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#include "build.auto.h"

#include <inttypes.h>
#include <string.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/time.h"
#include "config/config.h"
#include "version.h"

/***********************************************************************************************************************************
Track time command started
***********************************************************************************************************************************/
static TimeMSec timeBegin;
static String *cmdOptionStr;

/**********************************************************************************************************************************/
void
cmdInit(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    timeBegin = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
cmdOption(void)
{
    FUNCTION_TEST_VOID();

    if (cmdOptionStr == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            cmdOptionStr = strNew("");

            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Get command define id used to determine which options are valid for this command
                ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(cfgCommand());

                // Add command parameters if they exist
                const StringList *commandParamList = cfgCommandParam();

                if (strLstSize(commandParamList) != 0)
                {
                    strCatFmt(cmdOptionStr, " [");

                    for (unsigned int commandParamIdx = 0; commandParamIdx < strLstSize(commandParamList); commandParamIdx++)
                    {
                        const String *commandParam = strLstGet(commandParamList, commandParamIdx);

                        if (commandParamIdx != 0)
                            strCatFmt(cmdOptionStr, ", ");

                        if (strchr(strPtr(commandParam), ' ') != NULL)
                            commandParam = strNewFmt("\"%s\"", strPtr(commandParam));

                        strCat(cmdOptionStr, commandParam);
                    }

                    strCatFmt(cmdOptionStr, "]");
                }

                // Loop though options and add the ones that are interesting
                for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    // Skip the option if not valid for this command.  Generally only one command runs at a time, but sometimes
                    // commands are chained together (e.g. backup and expire) and the second command may not use all the options of
                    // the first command.  Displaying them is harmless but might cause confusion.
                    if (!cfgDefOptionValid(commandDefId, cfgOptionDefIdFromId(optionId)))
                        continue;

                    // If option was negated
                    if (cfgOptionNegate(optionId))
                        strCatFmt(cmdOptionStr, " --no-%s", cfgOptionName(optionId));
                    // If option was reset
                    else if (cfgOptionReset(optionId))
                        strCatFmt(cmdOptionStr, " --reset-%s", cfgOptionName(optionId));
                    // Else set and not default
                    else if (cfgOptionSource(optionId) != cfgSourceDefault && cfgOptionTest(optionId))
                    {
                        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                        // Don't show redacted options
                        if (cfgDefOptionSecure(optionDefId))
                            strCatFmt(cmdOptionStr, " --%s=<redacted>", cfgOptionName(optionId));
                        // Output boolean option
                        else if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                            strCatFmt(cmdOptionStr, " --%s", cfgOptionName(optionId));
                        // Output other options
                        else
                        {
                            StringList *valueList = NULL;

                            // Generate the values of hash options
                            if (cfgDefOptionType(optionDefId) == cfgDefOptTypeHash)
                            {
                                valueList = strLstNew();

                                const KeyValue *optionKv = cfgOptionKv(optionId);
                                const VariantList *keyList = kvKeyList(optionKv);

                                for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
                                {
                                    strLstAdd(
                                        valueList,
                                        strNewFmt(
                                            "%s=%s", strPtr(varStr(varLstGet(keyList, keyIdx))),
                                                strPtr(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx))))));
                                }
                            }
                            // Generate values for list options
                            else if (cfgDefOptionType(optionDefId) == cfgDefOptTypeList)
                            {
                                valueList = strLstNewVarLst(cfgOptionLst(optionId));
                            }
                            // Else only one value
                            else
                            {
                                valueList = strLstNew();
                                strLstAdd(valueList, varStrForce(cfgOption(optionId)));
                            }

                            // Output options and values
                            for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                            {
                                const String *value = strLstGet(valueList, valueListIdx);

                                strCatFmt(cmdOptionStr, " --%s", cfgOptionName(optionId));

                                if (strchr(strPtr(value), ' ') != NULL)
                                    value = strNewFmt("\"%s\"", strPtr(value));

                                strCatFmt(cmdOptionStr, "=%s", strPtr(value));
                            }
                        }
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(cmdOptionStr);
}

/**********************************************************************************************************************************/
void
cmdBegin(bool logOption)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, logOption);
    FUNCTION_LOG_END();

    ASSERT(cfgCommand() != cfgCmdNone);

    // This is fairly expensive log message to generate so skip it if it won't be output
    if (logAny(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Basic info on command start
            String *info = strNewFmt("%s command begin", strPtr(cfgCommandRoleName()));

            if (logOption)
            {
                strFree(cmdOptionStr);
                cmdOptionStr = NULL;

                strCatFmt(info, " %s:%s", PROJECT_VERSION, strPtr(cmdOption()));
            }

            LOG(cfgLogLevelDefault(), 0, strPtr(info));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdEnd(int code, const String *errorMessage)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, errorMessage);
    FUNCTION_LOG_END();

    ASSERT(cfgCommand() != cfgCmdNone);

    // Skip this log message if it won't be output.  It's not too expensive but since we skipped cmdBegin(), may as well.
    if (logAny(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Log socket statistics
            String *sckClientStat = sckClientStatStr();

            if (sckClientStat != NULL)
                LOG_DETAIL(strPtr(sckClientStat));

            // Log tls statistics
            String *tlsClientStat = tlsClientStatStr();

            if (tlsClientStat != NULL)
                LOG_DETAIL(strPtr(tlsClientStat));

            // Log http statistics
            String *httpClientStat = httpClientStatStr();

            if (httpClientStat != NULL)
                LOG_INFO(strPtr(httpClientStat));

            // Basic info on command end
            String *info = strNewFmt("%s command end: ", strPtr(cfgCommandRoleName()));

            if (errorMessage == NULL)
            {
                strCatZ(info, "completed successfully");

                if (cfgOptionValid(cfgOptLogTimestamp) && cfgOptionBool(cfgOptLogTimestamp))
                    strCatFmt(info, " (%" PRIu64 "ms)", timeMSec() - timeBegin);
            }
            else
                strCat(info, errorMessage);

            LOG(cfgLogLevelDefault(), 0, strPtr(info));
        }
        MEM_CONTEXT_TEMP_END();
    }

    // Reset timeBegin in case there is another command following this one
    timeBegin = timeMSec();

    FUNCTION_LOG_RETURN_VOID();
}
