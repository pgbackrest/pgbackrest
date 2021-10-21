/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Data for the currently loaded configuration
***********************************************************************************************************************************/
Config *configLocal = NULL;

/**********************************************************************************************************************************/
void
cfgInit(Config *config)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, config);
    FUNCTION_TEST_END();

    ASSERT(config != NULL);

    // Free the old context
    if (configLocal != NULL)
        memContextFree(configLocal->memContext);

    // Set config and move context to top so it persists for the life of the program
    configLocal = config;
    memContextMove(configLocal->memContext, memContextTop());

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigCommand
cfgCommand(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configLocal == NULL ? cfgCmdNone : configLocal->command);
}

ConfigCommandRole
cfgCommandRole(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->commandRole);
}

void
cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(commandId <= cfgCmdNone);

    configLocal->command = commandId;
    configLocal->commandRole = commandRoleId;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
cfgCommandHelp(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->help);
}

/**********************************************************************************************************************************/
VariantList *
cfgCommandJobRetry(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);

    // Return NULL if no retries
    unsigned int retryTotal = cfgOptionUInt(cfgOptJobRetry);

    if (retryTotal == 0)
        FUNCTION_TEST_RETURN(NULL);

    // Build retry list
    VariantList *result = varLstNew();

    for (unsigned int retryIdx = 0; retryIdx < cfgOptionUInt(cfgOptJobRetry); retryIdx++)
        varLstAdd(result, varNewUInt64(retryIdx == 0 ? 0 : cfgOptionUInt64(cfgOptJobRetryInterval)));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const char *
cfgCommandName(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command < cfgCmdNone);

    FUNCTION_TEST_RETURN(cfgParseCommandName(configLocal->command));
}

String *
cfgCommandRoleName(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_TEST_RETURN(cfgParseCommandRoleName(cfgCommand(), cfgCommandRole(), COLON_STR));
}

/**********************************************************************************************************************************/
const StringList *
cfgCommandParam(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);

    if (configLocal->paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            configLocal->paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(configLocal->paramList);
}

/**********************************************************************************************************************************/
const String *
cfgExe(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->exe);
}

/**********************************************************************************************************************************/
bool
cfgLockRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    // Local roles never take a lock and the remote role has special logic for locking
    FUNCTION_TEST_RETURN(
        // If a lock is required for the command and the role is main
        (configLocal->lockRequired && cfgCommandRole() == cfgCmdRoleMain) ||
        // Or any command when the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
bool
cfgLockRemoteRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configLocal->lockRemoteRequired);
}

/**********************************************************************************************************************************/
LockType
cfgLockType(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configLocal->lockType);
}

/**********************************************************************************************************************************/
bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(
        // If the command always logs to a file
        configLocal->logFile ||
        // Or log-level-file was explicitly set as a param/env var
        (cfgOptionValid(cfgOptLogLevelFile) && cfgOptionSource(cfgOptLogLevelFile) == cfgSourceParam) ||
        // Or the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
LogLevel
cfgLogLevelDefault(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configLocal->logLevelDefault);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroup(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].group);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupId(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal->option[optionId].group);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].groupId);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxDefault(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(configLocal->optionGroup[groupId].indexDefaultExists);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexDefault);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxToKey(ConfigOptionGroup groupId, unsigned int groupIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, groupIdx);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(groupIdx < configLocal->optionGroup[groupId].indexTotal);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexMap[groupIdx] + 1);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionKeyToIdx(ConfigOption optionId, unsigned int key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, key);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    unsigned int result = 0;

    // If then option is in a group then search for the key, else the index is 0
    if (cfgOptionGroup(optionId))
    {
        unsigned int groupId = cfgOptionGroupId(optionId);

        // Seach the group for the key
        for (; result < cfgOptionGroupIdxTotal(groupId); result++)
        {
            if (configLocal->optionGroup[groupId].indexMap[result] == key - 1)
                break;
        }

        // Error when the key is not found
        if (result == cfgOptionGroupIdxTotal(groupId))
            THROW_FMT(AssertError, "key '%u' is not valid for '%s' option", key, configLocal->option[optionId].name);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxTotal(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexTotal);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroupValid(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].valid);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionIdxDefault(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        !configLocal->option[optionId].group || configLocal->optionGroup[configLocal->option[optionId].groupId].indexDefaultExists);

    FUNCTION_TEST_RETURN(
        configLocal->option[optionId].group ? configLocal->optionGroup[configLocal->option[optionId].groupId].indexDefault : 0);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionIdxTotal(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(
        configLocal->option[optionId].group ? configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal : 1);
}

/**********************************************************************************************************************************/
const String *
cfgOptionDefault(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    if (configLocal->option[optionId].defaultValue == NULL)
        configLocal->option[optionId].defaultValue = cfgParseOptionDefault(cfgCommand(), optionId);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].defaultValue);
}

void
cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(configLocal->option[optionId].valid);
    ASSERT(cfgParseOptionDataType(optionId) == cfgOptDataTypeString);

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Duplicate into this context
        defaultValue = varDup(defaultValue);

        // Set the default value
        configLocal->option[optionId].defaultValue = varStr(defaultValue);

        // Copy the value to option indexes that are marked as default so the default can be retrieved quickly
        for (unsigned int optionIdx = 0; optionIdx < cfgOptionIdxTotal(optionId); optionIdx++)
        {
            if (configLocal->option[optionId].index[optionIdx].source == cfgSourceDefault)
            {
                configLocal->option[optionId].index[optionIdx].set = true;
                configLocal->option[optionId].index[optionIdx].value.string = varStr(defaultValue);
                configLocal->option[optionId].index[optionIdx].display = NULL;
            }
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}


/**********************************************************************************************************************************/
const String *
cfgOptionDisplayVar(const Variant *const value, const ConfigOptionType optionType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, value);
        FUNCTION_TEST_PARAM(UINT, optionType);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(optionType != cfgOptTypeHash && optionType != cfgOptTypeList);

    if (varType(value) == varTypeString)
    {
        FUNCTION_TEST_RETURN(varStr(value));
    }
    else if (optionType == cfgOptTypeBoolean)
    {
        FUNCTION_TEST_RETURN(varBool(value) ? TRUE_STR : FALSE_STR);
    }
    else if (optionType == cfgOptTypeTime)
    {
        FUNCTION_TEST_RETURN(strNewDbl((double)varInt64(value) / MSEC_PER_SEC));
    }

    FUNCTION_TEST_RETURN(varStrForce(value));
}

const String *
cfgOptionIdxDisplay(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, optionIdx));

    // If there is already a display value set then return that
    ConfigOptionValue *const option = &configLocal->option[optionId].index[optionIdx];

    if (option->display != NULL)
        FUNCTION_TEST_RETURN(option->display);

    // Generate the display value based on the type
    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        option->display = cfgOptionDisplayVar(cfgOptionIdxVar(optionId, optionIdx), cfgParseOptionType(optionId));
    }
    MEM_CONTEXT_END();


    FUNCTION_TEST_RETURN(option->display);
}

const String *
cfgOptionDisplay(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxDisplay(optionId, cfgOptionIdxDefault(optionId)));
}

/**********************************************************************************************************************************/
String *
cfgOptionHostPort(ConfigOption optionId, unsigned int *port)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM_P(UINT, port);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxHostPort(optionId, cfgOptionIdxDefault(optionId), port));
}

String *
cfgOptionIdxHostPort(ConfigOption optionId, unsigned int optionIdx, unsigned int *port)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM_P(UINT, port);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));
    ASSERT(port != NULL);

    String *result = NULL;

    // Proceed if option is valid and has a value
    if (cfgOptionIdxTest(optionId, optionIdx))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *host = cfgOptionIdxStr(optionId, optionIdx);

            // If the host contains a colon then it has a port appended
            if (strChr(host, ':') != -1)
            {
                const StringList *hostPart = strLstNewSplitZ(host, ":");

                // More than one colon is invalid
                if (strLstSize(hostPart) > 2)
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "'%s' is not valid for option '%s'"
                            "\nHINT: is more than one port specified?",
                        strZ(host), cfgOptionIdxName(optionId, optionIdx));
                }

                // Set the host
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(strLstGet(hostPart, 0));
                }
                MEM_CONTEXT_PRIOR_END();

                // Set the port and error if it is not a positive integer
                TRY_BEGIN()
                {
                    *port = cvtZToUInt(strZ(strLstGet(hostPart, 1)));
                }
                CATCH(FormatError)
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "'%s' is not valid for option '%s'"
                            "\nHINT: port is not a positive integer.",
                        strZ(host), cfgOptionIdxName(optionId, optionIdx));
                }
                TRY_END();
            }
            // Else there is no port and just copy the host
            else
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(host);
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get option name by id
***********************************************************************************************************************************/
const char *
cfgOptionName(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionIdxName(optionId, cfgOptionIdxDefault(optionId)));
}

const char *
cfgOptionIdxName(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    if (configLocal->option[optionId].group)
    {
        // This is somewhat less than ideal since memory is being allocated with each call, rather than caching prior results. In
        // practice the number of allocations should be quite small so we'll ignore this for now.
        String *name = strNewFmt(
            "%s%u%s", configLocal->optionGroup[configLocal->option[optionId].groupId].name,
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexMap[optionIdx] + 1,
            configLocal->option[optionId].name + strlen(configLocal->optionGroup[configLocal->option[optionId].groupId].name));

        FUNCTION_TEST_RETURN(strZ(name));
    }

    FUNCTION_TEST_RETURN(configLocal->option[optionId].name);
}

/**********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxNegate(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxNegate(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].negate);
}

/**********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxReset(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxReset(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].reset);
}

/**********************************************************************************************************************************/
// Helper to enforce contraints when getting options
static const ConfigOptionValue *
cfgOptionIdxInternal(
    const ConfigOption optionId, const unsigned int optionIdx, const ConfigOptionDataType typeRequested, const bool nullAllowed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM(ENUM, typeRequested);
        FUNCTION_TEST_PARAM(BOOL, nullAllowed);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, optionIdx));

    // If the option is not NULL then check it is the requested type
    const ConfigOptionValue *const result = &configLocal->option[optionId].index[optionIdx];

    if (result->set)
    {
        if (configLocal->option[optionId].dataType != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionIdxName(optionId, optionIdx),
                configLocal->option[optionId].dataType, typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionIdxName(optionId, optionIdx));

    FUNCTION_TEST_RETURN(result);
}

Variant *
cfgOptionIdxVar(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    if (configLocal->option[optionId].index[optionIdx].set)
    {
        switch (configLocal->option[optionId].dataType)
        {
            case cfgOptDataTypeBoolean:
                FUNCTION_TEST_RETURN(varNewBool(configLocal->option[optionId].index[optionIdx].value.boolean));

            case cfgOptDataTypeHash:
                FUNCTION_TEST_RETURN(varNewKv(kvDup(configLocal->option[optionId].index[optionIdx].value.keyValue)));

            case cfgOptDataTypeInteger:
                FUNCTION_TEST_RETURN(varNewInt64(configLocal->option[optionId].index[optionIdx].value.integer));

            case cfgOptDataTypeList:
                FUNCTION_TEST_RETURN(varNewVarLst(configLocal->option[optionId].index[optionIdx].value.list));

            default:
                ASSERT(configLocal->option[optionId].dataType == cfgOptDataTypeString);
                break;
        }

        FUNCTION_TEST_RETURN(varNewStr(configLocal->option[optionId].index[optionIdx].value.string));
    }

    FUNCTION_TEST_RETURN(NULL);
}

bool
cfgOptionIdxBool(const ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeBoolean, false)->value.boolean);
}

int
cfgOptionIdxInt(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer >= INT_MIN);
    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer <= INT_MAX);

    FUNCTION_LOG_RETURN(INT, (int)cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

int64_t
cfgOptionIdxInt64(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

unsigned int
cfgOptionIdxUInt(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer >= 0);
    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer <= UINT_MAX);

    FUNCTION_LOG_RETURN(UINT, (unsigned int)cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

uint64_t
cfgOptionIdxUInt64(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer >= 0);

    FUNCTION_LOG_RETURN(UINT64, (uint64_t)cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

const KeyValue *
cfgOptionIdxKv(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(KEY_VALUE, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeHash, false)->value.keyValue);
}

const KeyValue *
cfgOptionIdxKvNull(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(KEY_VALUE, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeHash, true)->value.keyValue);
}

const VariantList *
cfgOptionIdxLst(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(configLocal != NULL);

    const VariantList *optionValue = cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeList, true)->value.list;

    if (optionValue == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            optionValue = varLstNew();
            configLocal->option[optionId].index[optionIdx].value.list = optionValue;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN_CONST(VARIANT_LIST, optionValue);
}

const String *
cfgOptionIdxStr(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeString, false)->value.string);
}

const String *
cfgOptionIdxStrNull(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeString, true)->value.string);
}

// Helper to convert option String values to StringIds. Some options need 6-bit encoding while most work fine with 5-bit encoding.
// At some point the config parser will work with StringIds directly and this code can be removed, but for now it protects the
// callers from this logic and hopefully means no changes to the callers when the parser is updated.
static StringId
cfgOptionStrIdInternal(
    const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    const String *const value = cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeString, false)->value.string;
    StringId result = 0;

    TRY_BEGIN()
    {
        result = strIdFromStr(stringIdBit5, value);
    }
    CATCH_ANY()
    {
        result = strIdFromStr(stringIdBit6, value);
    }
    TRY_END();

    FUNCTION_TEST_RETURN(result);
}

StringId
cfgOptionIdxStrId(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(STRING_ID, cfgOptionStrIdInternal(optionId, optionIdx));
}

/**********************************************************************************************************************************/
void
cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    cfgOptionIdxSet(optionId, cfgOptionIdxDefault(optionId), source, value);

    FUNCTION_TEST_RETURN_VOID();
}

void
cfgOptionIdxSet(ConfigOption optionId, unsigned int optionIdx, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    // Set the source
    configLocal->option[optionId].index[optionIdx].source = source;

    // Only set value if it is not null
    if (value != NULL)
    {
        switch (configLocal->option[optionId].dataType)
        {
            case cfgOptDataTypeBoolean:
                configLocal->option[optionId].index[optionIdx].value.boolean = varBool(value);
                break;

            case cfgOptDataTypeInteger:
                configLocal->option[optionId].index[optionIdx].value.integer = varInt64(value);
                break;

            case cfgOptDataTypeString:
            {
                if (varType(value) == varTypeString)
                {
                    MEM_CONTEXT_BEGIN(configLocal->memContext)
                    {
                        configLocal->option[optionId].index[optionIdx].value.string = strDup(varStr(value));
                    }
                    MEM_CONTEXT_END();
                }
                else
                {
                    THROW_FMT(
                        AssertError, "option '%s' must be set with String variant", cfgOptionIdxName(optionId, optionIdx));
                }

                break;
            }

            default:
                THROW_FMT(AssertError, "set not available for option data type %u", configLocal->option[optionId].dataType);
        }

        configLocal->option[optionId].index[optionIdx].set = true;
    }
    else
    {
        configLocal->option[optionId].index[optionIdx].set = false;

        // Pointer values need to be set to null since they can be accessed when the option is not set, e.g. cfgOptionStrNull().
        configLocal->option[optionId].index[optionIdx].value.string = NULL;
    }

    // Clear the display value, which will be generated when needed
    configLocal->option[optionId].index[optionIdx].display = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigSource
cfgOptionSource(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxSource(optionId, cfgOptionIdxDefault(optionId)));
}

ConfigSource
cfgOptionIdxSource(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].source);
}

/**********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxTest(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxTest(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        !cfgOptionValid(optionId) ||
        ((!configLocal->option[optionId].group && optionIdx == 0) ||
         (configLocal->option[optionId].group && optionIdx <
          configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal)));

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configLocal->option[optionId].index[optionIdx].set);
}

/**********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].valid);
}

void
cfgOptionInvalidate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);

    configLocal->option[optionId].valid = false;

    FUNCTION_TEST_RETURN_VOID();
}
