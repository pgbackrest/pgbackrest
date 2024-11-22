/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Data for the currently loaded configuration
***********************************************************************************************************************************/
static Config *configLocal = NULL;

/**********************************************************************************************************************************/
FN_EXTERN void
cfgInit(Config *const config)
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

FN_EXTERN bool
cfgInited(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(ENUM, configLocal != NULL);
}

/**********************************************************************************************************************************/
FN_EXTERN ConfigCommand
cfgCommand(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(cfgInited());
    FUNCTION_TEST_RETURN(ENUM, configLocal->command);
}

FN_EXTERN ConfigCommandRole
cfgCommandRole(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(cfgInited());
    FUNCTION_TEST_RETURN(ENUM, configLocal->commandRole);
}

FN_EXTERN void
cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(commandId < CFG_COMMAND_TOTAL);

    configLocal->command = commandId;
    configLocal->commandRole = commandRoleId;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgCommandHelp(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(BOOL, configLocal->help);
}

/**********************************************************************************************************************************/
FN_EXTERN VariantList *
cfgCommandJobRetry(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());

    // Return NULL if no retries
    const unsigned int retryTotal = cfgOptionUInt(cfgOptJobRetry);

    if (retryTotal == 0)
        FUNCTION_TEST_RETURN(VARIANT_LIST, NULL);

    // Build retry list
    VariantList *const result = varLstNew();

    MEM_CONTEXT_BEGIN(lstMemContext((List *)result))
    {
        for (unsigned int retryIdx = 0; retryIdx < cfgOptionUInt(cfgOptJobRetry); retryIdx++)
            varLstAdd(result, varNewUInt64(retryIdx == 0 ? 0 : cfgOptionUInt64(cfgOptJobRetryInterval)));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(VARIANT_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const char *
cfgCommandName(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN_CONST(STRINGZ, cfgParseCommandName(configLocal->command));
}

FN_EXTERN String *
cfgCommandRoleName(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(STRING, cfgParseCommandRoleName(cfgCommand(), cfgCommandRole()));
}

/**********************************************************************************************************************************/
FN_EXTERN const StringList *
cfgCommandParam(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());

    if (configLocal->paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            configLocal->paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(STRING_LIST, configLocal->paramList);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
cfgExe(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(cfgInited());
    FUNCTION_TEST_RETURN(STRING, configLocal->exe);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgLockRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    // Local roles never take a lock and the remote role has special logic for locking
    FUNCTION_TEST_RETURN(
        BOOL,
        // If a lock is required for the command and the role is main
        (configLocal->lockRequired && cfgCommandRole() == cfgCmdRoleMain) ||
        // Or any command when the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgLockRemoteRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN(BOOL, configLocal->lockRemoteRequired);
}

/**********************************************************************************************************************************/
FN_EXTERN LockType
cfgLockType(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN(ENUM, configLocal->lockType);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN(
        BOOL,
        // If the command always logs to a file
        configLocal->logFile ||
        // Or log-level-file was explicitly set as a param/env var
        (cfgOptionValid(cfgOptLogLevelFile) && cfgOptionSource(cfgOptLogLevelFile) == cfgSourceParam) ||
        // Or the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
FN_EXTERN LogLevel
cfgLogLevelDefault(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(cfgInited());
    ASSERT(configLocal->command < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN(ENUM, configLocal->logLevelDefault);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgOptionGroup(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(BOOL, configLocal->option[optionId].group);
}

/**********************************************************************************************************************************/
FN_EXTERN const char *
cfgOptionGroupName(const ConfigOptionGroup groupId, const unsigned int groupIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, groupIdx);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(groupIdx < configLocal->optionGroup[groupId].indexTotal);

    // Generate names for the group the first time one is requested
    ConfigOptionGroupData *const group = &configLocal->optionGroup[groupId];

    if (group->indexName == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            group->indexName = memNew(sizeof(String *) * group->indexTotal);

            for (unsigned int groupIdx = 0; groupIdx < group->indexTotal; groupIdx++)
                group->indexName[groupIdx] = strNewFmt("%s%u", group->name, group->indexMap[groupIdx] + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_CONST(STRINGZ, strZ(group->indexName[groupIdx]));
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionGroupId(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal->option[optionId].group);

    FUNCTION_TEST_RETURN(UINT, configLocal->option[optionId].groupId);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionGroupIdxDefault(const ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(configLocal->optionGroup[groupId].indexDefaultExists);

    FUNCTION_TEST_RETURN(UINT, configLocal->optionGroup[groupId].indexDefault);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionGroupIdxToKey(const ConfigOptionGroup groupId, const unsigned int groupIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, groupIdx);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(groupIdx < configLocal->optionGroup[groupId].indexTotal);

    FUNCTION_TEST_RETURN(UINT, configLocal->optionGroup[groupId].indexMap[groupIdx] + 1);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionKeyToIdx(const ConfigOption optionId, const unsigned int key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, key);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);

    unsigned int result = 0;

    // If then option is in a group then search for the key, else the index is 0
    if (cfgOptionGroup(optionId))
    {
        const unsigned int groupId = cfgOptionGroupId(optionId);

        // Search the group for the key
        for (; result < cfgOptionGroupIdxTotal(groupId); result++)
        {
            if (configLocal->optionGroup[groupId].indexMap[result] == key - 1)
                break;
        }

        // Error when the key is not found
        if (result == cfgOptionGroupIdxTotal(groupId))
            THROW_FMT(AssertError, "key '%u' is not valid for '%s' option", key, configLocal->option[optionId].name);
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionGroupIdxTotal(const ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(UINT, configLocal->optionGroup[groupId].indexTotal);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionIdxDefault(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        !configLocal->option[optionId].group || configLocal->optionGroup[configLocal->option[optionId].groupId].indexDefaultExists);

    const ConfigOptionData *const option = &configLocal->option[optionId];

    FUNCTION_TEST_RETURN(UINT, option->group ? configLocal->optionGroup[option->groupId].indexDefault : 0);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cfgOptionIdxTotal(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);

    const ConfigOptionData *const option = &configLocal->option[optionId];

    FUNCTION_TEST_RETURN(UINT, option->group ? configLocal->optionGroup[option->groupId].indexTotal : 1);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
cfgOptionDefault(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT(optionId < CFG_OPTION_TOTAL);

    ConfigOptionData *const option = &configLocal->option[optionId];

    if (option->defaultValue == NULL)
        option->defaultValue = cfgParseOptionDefault(cfgCommand(), optionId);

    FUNCTION_TEST_RETURN_CONST(STRING, option->defaultValue);
}

FN_EXTERN void
cfgOptionDefaultSet(const ConfigOption optionId, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT(configLocal->option[optionId].valid);
    ASSERT(cfgParseOptionDataType(optionId) == cfgOptDataTypeString);

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Duplicate into this context
        defaultValue = varDup(defaultValue);

        // Set the default value
        ConfigOptionData *const option = &configLocal->option[optionId];
        option->defaultValue = varStr(defaultValue);

        // Copy the value to option indexes that are marked as default so the default can be retrieved quickly
        for (unsigned int optionIdx = 0; optionIdx < cfgOptionIdxTotal(optionId); optionIdx++)
        {
            ConfigOptionValue *const optionValue = &option->index[optionIdx];

            if (optionValue->source == cfgSourceDefault)
            {
                optionValue->set = true;
                optionValue->value.string = varStr(defaultValue);
                optionValue->display = NULL;
            }
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
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
        FUNCTION_TEST_RETURN_CONST(STRING, varStr(value));
    }
    else if (optionType == cfgOptTypeBoolean)
    {
        FUNCTION_TEST_RETURN_CONST(STRING, varBool(value) ? TRUE_STR : FALSE_STR);
    }
    else if (optionType == cfgOptTypeTime)
    {
        FUNCTION_TEST_RETURN_CONST(STRING, strNewDbl((double)varInt64(value) / MSEC_PER_SEC));
    }
    else if (optionType == cfgOptTypeStringId)
    {
        FUNCTION_TEST_RETURN_CONST(STRING, strIdToStr(varUInt64(value)));
    }

    FUNCTION_TEST_RETURN(STRING, varStrForce(value));
}

FN_EXTERN const String *
cfgOptionIdxDisplay(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, optionIdx));

    // If there is already a display value set then return that
    ConfigOptionValue *const option = &configLocal->option[optionId].index[optionIdx];

    if (option->display != NULL)
        FUNCTION_TEST_RETURN_CONST(STRING, option->display);

    // Generate the display value based on the type
    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        option->display = cfgOptionDisplayVar(cfgOptionIdxVar(optionId, optionIdx), cfgParseOptionType(optionId));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_CONST(STRING, option->display);
}

FN_EXTERN const String *
cfgOptionDisplay(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, cfgOptionIdxDisplay(optionId, cfgOptionIdxDefault(optionId)));
}

/***********************************************************************************************************************************
Get option name by id
***********************************************************************************************************************************/
FN_EXTERN const char *
cfgOptionName(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN_CONST(STRINGZ, cfgOptionIdxName(optionId, cfgOptionIdxDefault(optionId)));
}

FN_EXTERN const char *
cfgOptionIdxName(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    // If an indexed option
    ConfigOptionData *const option = &configLocal->option[optionId];

    if (option->group)
    {
        const ConfigOptionGroupData *const group = &configLocal->optionGroup[option->groupId];

        // Generate indexed names for the option the first time one is requested
        if (option->indexName == NULL)
        {
            MEM_CONTEXT_BEGIN(configLocal->memContext)
            {
                option->indexName = memNew(sizeof(String *) * group->indexTotal);

                for (unsigned int optionIdx = 0; optionIdx < group->indexTotal; optionIdx++)
                {
                    option->indexName[optionIdx] = strNewFmt(
                        "%s%u%s", group->name, group->indexMap[optionIdx] + 1, option->name + strlen(group->name));
                }
            }
            MEM_CONTEXT_END();
        }

        FUNCTION_TEST_RETURN_CONST(STRINGZ, strZ(option->indexName[optionIdx]));
    }

    // Else not indexed
    FUNCTION_TEST_RETURN_CONST(STRINGZ, option->name);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgOptionIdxNegate(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    FUNCTION_TEST_RETURN(BOOL, configLocal->option[optionId].index[optionIdx].negate);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgOptionIdxReset(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    FUNCTION_TEST_RETURN(BOOL, configLocal->option[optionId].index[optionIdx].reset);
}

/**********************************************************************************************************************************/
// Helper to enforce constraints when getting options
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
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, optionIdx));

    // If the option is not NULL then check it is the requested type
    const ConfigOptionData *const option = &configLocal->option[optionId];
    const ConfigOptionValue *const result = &option->index[optionIdx];

    if (result->set)
    {
        if (option->dataType != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionIdxName(optionId, optionIdx),
                option->dataType, typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionIdxName(optionId, optionIdx));

    FUNCTION_TEST_RETURN_TYPE_CONST_P(ConfigOptionValue, result);
}

FN_EXTERN Variant *
cfgOptionIdxVar(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    const ConfigOptionData *const option = &configLocal->option[optionId];

    if (option->index[optionIdx].set)
    {
        const ConfigOptionValueType *const optionValueType = &option->index[optionIdx].value;

        switch (option->dataType)
        {
            case cfgOptDataTypeBoolean:
                FUNCTION_TEST_RETURN(VARIANT, varNewBool(optionValueType->boolean));

            case cfgOptDataTypeHash:
                FUNCTION_TEST_RETURN(VARIANT, varNewKv(kvDup(optionValueType->keyValue)));

            case cfgOptDataTypeInteger:
                FUNCTION_TEST_RETURN(VARIANT, varNewInt64(optionValueType->integer));

            case cfgOptDataTypeList:
                FUNCTION_TEST_RETURN(VARIANT, varNewVarLst(optionValueType->list));

            case cfgOptDataTypeStringId:
                FUNCTION_TEST_RETURN(VARIANT, varNewUInt64(optionValueType->stringId));

            default:
                ASSERT(option->dataType == cfgOptDataTypeString);
                break;
        }

        FUNCTION_TEST_RETURN(VARIANT, varNewStr(optionValueType->string));
    }

    FUNCTION_TEST_RETURN(VARIANT, NULL);
}

FN_EXTERN bool
cfgOptionIdxBool(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeBoolean, false)->value.boolean);
}

FN_EXTERN int
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

FN_EXTERN int64_t
cfgOptionIdxInt64(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

FN_EXTERN unsigned int
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

FN_EXTERN uint64_t
cfgOptionIdxUInt64(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer >= 0);

    FUNCTION_LOG_RETURN(UINT64, (uint64_t)cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeInteger, false)->value.integer);
}

FN_EXTERN const KeyValue *
cfgOptionIdxKv(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(KEY_VALUE, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeHash, false)->value.keyValue);
}

FN_EXTERN const KeyValue *
cfgOptionIdxKvNull(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(KEY_VALUE, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeHash, true)->value.keyValue);
}

FN_EXTERN const VariantList *
cfgOptionIdxLst(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(cfgInited());

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

FN_EXTERN const String *
cfgOptionIdxStr(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeString, false)->value.string);
}

FN_EXTERN const String *
cfgOptionIdxStrNull(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeString, true)->value.string);
}

FN_EXTERN StringId
cfgOptionIdxStrId(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(STRING_ID, cfgOptionIdxInternal(optionId, optionIdx, cfgOptDataTypeStringId, false)->value.stringId);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cfgOptionSet(const ConfigOption optionId, const ConfigSource source, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    cfgOptionIdxSet(optionId, cfgOptionIdxDefault(optionId), source, value);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
cfgOptionIdxSet(
    const ConfigOption optionId, const unsigned int optionIdx, const ConfigSource source, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    // Set the source
    const ConfigOptionData *const option = &configLocal->option[optionId];
    ConfigOptionValue *const optionValue = &option->index[optionIdx];
    optionValue->source = source;

    // Only set value if it is not null
    ConfigOptionValueType *const optionValueType = &optionValue->value;

    if (value != NULL)
    {
        switch (option->dataType)
        {
            case cfgOptDataTypeBoolean:
                optionValueType->boolean = varBool(value);
                break;

            case cfgOptDataTypeInteger:
                optionValueType->integer = varInt64(value);
                break;

            case cfgOptDataTypeString:
            {
                if (varType(value) == varTypeString)
                {
                    MEM_CONTEXT_BEGIN(configLocal->memContext)
                    {
                        optionValueType->string = strDup(varStr(value));
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

            case cfgOptDataTypeStringId:
            {
                if (varType(value) == varTypeUInt64)
                    optionValueType->stringId = varUInt64(value);
                else
                    optionValueType->stringId = strIdFromStr(varStr(value));

                break;
            }

            default:
                THROW_FMT(AssertError, "set not available for option data type %u", option->dataType);
        }

        optionValue->set = true;
    }
    else
    {
        optionValue->set = false;

        // Pointer values need to be set to null since they can be accessed when the option is not set, e.g. cfgOptionStrNull().
        // Setting string to NULL suffices to set the other pointers in the union to NULL.
        optionValueType->string = NULL;
    }

    // Clear the display value, which will be generated when needed
    optionValue->display = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN ConfigSource
cfgOptionSource(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(ENUM, cfgOptionIdxSource(optionId, cfgOptionIdxDefault(optionId)));
}

FN_EXTERN ConfigSource
cfgOptionIdxSource(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT((!group && optionIdx == 0) || (group && optionIdx < indexTotal));

    FUNCTION_TEST_RETURN(ENUM, configLocal->option[optionId].index[optionIdx].source);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgOptionTest(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, cfgOptionIdxTest(optionId, cfgOptionIdxDefault(optionId)));
}

FN_EXTERN bool
cfgOptionIdxTest(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());
    ASSERT_DECLARE(const bool group = configLocal->option[optionId].group);
    ASSERT_DECLARE(const unsigned int indexTotal = configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal);
    ASSERT(!cfgOptionValid(optionId) || ((!group && optionIdx == 0) || (group && optionIdx < indexTotal)));

    FUNCTION_TEST_RETURN(BOOL, cfgOptionValid(optionId) && configLocal->option[optionId].index[optionIdx].set);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgOptionValid(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());

    FUNCTION_TEST_RETURN(BOOL, configLocal->option[optionId].valid);
}

FN_EXTERN void
cfgOptionInvalidate(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(cfgInited());

    configLocal->option[optionId].valid = false;

    FUNCTION_TEST_RETURN_VOID();
}
