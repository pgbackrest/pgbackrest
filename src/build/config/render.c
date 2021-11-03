/***********************************************************************************************************************************
Render Configuration Data
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>

#include "common/log.h"
#include "common/type/convert.h"
#include "storage/posix/storage.h"

#include "build/common/render.h"
#include "build/config/render.h"

/***********************************************************************************************************************************
Option type constants
***********************************************************************************************************************************/
#define CFGDEF_TYPE_STRING                                          "string"

/***********************************************************************************************************************************
Build constant from a string
***********************************************************************************************************************************/
static String *
bldConst(const char *const prefix, const String *const value)
{
    return strUpper(strReplaceChr(strNewFmt("%s_%s", prefix, strZ(value)), '-', '_'));
}

/***********************************************************************************************************************************
Build enum from a string
***********************************************************************************************************************************/
static String *
bldEnum(const char *const prefix, const String *const value)
{
    String *const result = strCatZ(strNew(), prefix);
    const char *const valuePtr = strZ(value);

    bool upper = true;

    for (unsigned int valueIdx = 0; valueIdx < strSize(value); valueIdx++)
    {
        strCatChr(result, upper ? (char)toupper(valuePtr[valueIdx]) : valuePtr[valueIdx]);
        upper = false;

        if (valuePtr[valueIdx + 1] == '-')
        {
            upper = true;
            valueIdx++;
        }
    }

    return result;
}

// Build command enum from a string
static String *
bldEnumCmd(const String *const value)
{
    return bldEnum("cfgCmd", value);
}

// Build option group enum from a string
static String *
bldEnumOptGrp(const String *const value)
{
    return bldEnum("cfgOptGrp", value);
}

// Build option enum from a string
static String *
bldEnumOpt(const String *const value)
{
    return bldEnum("cfgOpt", value);
}

/***********************************************************************************************************************************
Render config.auto.h
***********************************************************************************************************************************/
#define CONFIG_MODULE                                               "config"
#define CONFIG_AUTO_COMMENT                                         "Command and Option Configuration"

static void
bldCfgRenderConfigAutoH(const Storage *const storageRepo, const BldCfg bldCfg)
{
    String *config = strCatFmt(
        strNew(),
        "%s"
        "#ifndef CONFIG_CONFIG_AUTO_H\n"
        "#define CONFIG_CONFIG_AUTO_H\n",
        strZ(bldHeader(CONFIG_MODULE, CONFIG_AUTO_COMMENT)));

    // Command constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command constants\n"
        COMMENT_BLOCK_END "\n");

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);

        strCatFmt(config, "%s\n", strZ(bldDefineRender(bldConst("CFGCMD", cmd->name), strNewFmt("\"%s\"", strZ(cmd->name)))));
    }

    strCatFmt(config, "\n%s\n", strZ(bldDefineRender(STRDEF("CFG_COMMAND_TOTAL"), strNewFmt("%u", lstSize(bldCfg.cmdList)))));

    // Option group constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option group constants\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(config, "%s\n", strZ(bldDefineRender(STRDEF("CFG_OPTION_GROUP_TOTAL"), strNewFmt("%u", lstSize(bldCfg.optGrpList)))));

    // Option constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option constants\n"
        COMMENT_BLOCK_END "\n");

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);

        if (opt->group == NULL)
            strCatFmt(config, "%s\n", strZ(bldDefineRender(bldConst("CFGOPT", opt->name), strNewFmt("\"%s\"", strZ(opt->name)))));
    }

    strCatFmt(config, "\n%s\n", strZ(bldDefineRender(STRDEF("CFG_OPTION_TOTAL"), strNewFmt("%u", lstSize(bldCfg.optList)))));

    // Option value constants
    // -----------------------------------------------------------------------------------------------------------------------------
    bool lf = false;

    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option value constants\n"
        COMMENT_BLOCK_END "\n");

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);

        if (strEq(opt->type, OPT_TYPE_STRING_ID_STR))
        {
            StringList *const allowList = strLstNew();

            if (opt->allowList != NULL)
            {
                for (unsigned int allowListIdx = 0; allowListIdx < strLstSize(opt->allowList); allowListIdx++)
                    strLstAddIfMissing(allowList, strLstGet(opt->allowList, allowListIdx));
            }

            for (unsigned int optCmdListIdx = 0; optCmdListIdx < lstSize(opt->cmdList); optCmdListIdx++)
            {
                BldCfgOptionCommand *optCmd = lstGet(opt->cmdList, optCmdListIdx);

                if (optCmd->allowList != NULL)
                {
                    for (unsigned int allowListIdx = 0; allowListIdx < strLstSize(optCmd->allowList); allowListIdx++)
                        strLstAddIfMissing(allowList, strLstGet(optCmd->allowList, allowListIdx));
                }
            }

            strLstSort(allowList, sortOrderAsc);

            ASSERT(!strLstEmpty(allowList));

            if (lf)
                strCatChr(config, '\n');

            for (unsigned int allowListIdx = 0; allowListIdx < strLstSize(allowList); allowListIdx++)
            {
                const String *const allowListItem = strLstGet(allowList, allowListIdx);
                const String *const constPrefix = strUpper(
                    strReplaceChr(strNewFmt("CFGOPTVAL_%s_%s", strZ(opt->name), strZ(allowListItem)), '-', '_'));

                // Render StringId
                strCatFmt(config, "%s\n", strZ(bldDefineRender(constPrefix, bldStrId(strZ(allowListItem)))));

                // Render Z
                strCatFmt(
                    config, "%s\n",
                    strZ(bldDefineRender(strNewFmt("%s_Z", strZ(constPrefix)), strNewFmt("\"%s\"", strZ(allowListItem)))));
            }

            lf = true;
        }
    }

    // Command enum
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command enum\n"
        COMMENT_BLOCK_END "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);

        strCatFmt(config, "    %s,\n", strZ(bldEnumCmd(cmd->name)));
    }

    strCatFmt(config, "    %s,\n", strZ(bldEnumCmd(STRDEF("none"))));

    strCatZ(
        config,
        "} ConfigCommand;\n");

    // Option group enum
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option group enum\n"
        COMMENT_BLOCK_END "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int optGrpIdx = 0; optGrpIdx < lstSize(bldCfg.optGrpList); optGrpIdx++)
    {
        const BldCfgOptionGroup *const optGrp = lstGet(bldCfg.optGrpList, optGrpIdx);

        strCatFmt(config, "    %s,\n", strZ(bldEnumOptGrp(optGrp->name)));
    }

    strCatZ(
        config,
        "} ConfigOptionGroup;\n");

    // Option enum
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option enum\n"
        COMMENT_BLOCK_END "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOptionGroup *const opt = lstGet(bldCfg.optList, optIdx);

        strCatFmt(config, "    %s,\n", strZ(bldEnumOpt(opt->name)));
    }

    strCatZ(
        config,
        "} ConfigOption;\n");

    // End and save
    strCatZ(
        config,
        "\n"
        "#endif\n");

    bldPut(storageRepo, "src/config/config.auto.h", BUFSTR(config));
}

/***********************************************************************************************************************************
Render parse.auto.c
***********************************************************************************************************************************/
#define PARSE_AUTO_COMMENT                                          "Config Parse Rules"

typedef struct BldCfgRenderOptionDeprecate
{
    const String *const name;                                       // Deprecated option name
    const BldCfgOption *const option;                               // Option
    const bool indexed;                                             // Can the deprecation be indexed?
    const bool unindexed;                                           // Can the deprecation be unindexed?
} BldCfgRenderOptionDeprecate;

static void
bldCfgRenderLf(String *const config, const bool lf)
{
    if (lf)
        strCatZ(config, "\n");
}

// Helper to get var-128 encoding size
static size_t
bldCfgRenderVar128Size(uint64_t value)
{
    size_t result = 1;

    while (value >= 0x80)
    {
        value >>= 7;
        result++;
    }

    return result;
}

// Helper to render enum string
static String *
bldCfgRenderEnumStr(const String *const source)
{
    String *const result = strNew();
    bool priorSpecial = false;

    for (unsigned int sourceIdx = 0; sourceIdx < strSize(source); sourceIdx++)
    {
        switch (strZ(source)[sourceIdx])
        {
            case '"':
            case '/':
            case ' ':
            case '.':
            {
                if (sourceIdx != 0 && !priorSpecial)
                    strCatChr(result, '_');

                switch (strZ(source)[sourceIdx])
                {
                    case '"':
                        strCatZ(result, "QT");
                        break;

                    case '/':
                        strCatZ(result, "FS");
                        break;

                    case ' ':
                        strCatZ(result, "SP");
                        break;

                    case '.':
                        strCatZ(result, "DT");
                        break;
                }

                if (sourceIdx != strSize(source) - 1)
                    strCatChr(result, '_');

                priorSpecial = true;
                break;
            }

            default:
                strCatChr(result, strZ(source)[sourceIdx]);
                priorSpecial = false;
                break;
        }
    }

    return result;
}

// Helper to render scalars
static String *
bldCfgRenderScalar(const String *const scalar, const String *const optType)
{
    if (strEq(optType, OPT_TYPE_STRING_ID_STR))
        return strNewFmt("PARSE_RULE_VAL_STRID(%s)", strZ(bldEnum("parseRuleValStrId", scalar)));

    if (strEq(optType, OPT_TYPE_STRING_STR))
        return strNewFmt("PARSE_RULE_VAL_STR(parseRuleValStr%s)", strZ(bldCfgRenderEnumStr(scalar)));

    if (strEq(optType, OPT_TYPE_BOOLEAN_STR))
        return strNewFmt("PARSE_RULE_VAL_BOOL_%s", strEqZ(scalar, "true") ? "TRUE" : "FALSE");

    int64_t value = 0;

    if (strEq(optType, OPT_TYPE_TIME_STR))
    {
        value = (int64_t)(cvtZToDouble(strZ(scalar)) * 1000);
    }
    else
    {
        CHECK(strEq(optType, OPT_TYPE_SIZE_STR) || strEq(optType, OPT_TYPE_INTEGER_STR));

        value = cvtZToInt64(strZ(scalar));
    }

    return strNewFmt("PARSE_RULE_VAL_INT(%s)", strZ(bldEnum("parseRuleValInt", strNewFmt("%" PRId64, value))));
}

// Helper to render validity
static String *
bldCfgRenderValid(const BldCfgOptionDepend *const depend)
{
    ASSERT(depend != NULL);

    String *const result = strNew();

    strCatFmt(
        result,
        "                PARSE_RULE_OPTIONAL_DEPEND\n"
        "                (\n"
        "                    PARSE_RULE_VAL_OPT(%s),\n",
        strZ(bldEnum("cfgOpt", depend->option->name)));

    if (depend->valueList != NULL)
    {
        for (unsigned int valueIdx = 0; valueIdx < strLstSize(depend->valueList); valueIdx++)
        {
            strCatFmt(
                result,
                "                    %s,\n",
                strZ(
                    bldCfgRenderScalar(
                        strLstGet(depend->valueList, valueIdx),
                        strEq(depend->option->type, OPT_TYPE_STRING_STR) ? OPT_TYPE_STRING_ID_STR : depend->option->type)));
        }
    }

    strCatZ(
        result,
        "                )");

    return result;
}

// Helper to render allow range
static String *
bldCfgRenderAllowRange(const String *const allowRangeMin, const String *const allowRangeMax, const String *const optType)
{
    ASSERT(allowRangeMin != NULL);
    ASSERT(allowRangeMax != NULL);
    ASSERT(optType != NULL);

    return strNewFmt(
        "                PARSE_RULE_OPTIONAL_ALLOW_RANGE\n"
        "                (\n"
        "                    %s,\n"
        "                    %s,\n"
        "                )",
        strZ(bldCfgRenderScalar(allowRangeMin, optType)),
        strZ(bldCfgRenderScalar(allowRangeMax, optType)));
}

// Helper to render allow list
static String *
bldCfgRenderAllowList(const StringList *const allowList, const String *const optType)
{
    ASSERT(allowList != NULL);
    ASSERT(optType != NULL);

    String *const result = strNew();

    strCatZ(
        result,
        "                PARSE_RULE_OPTIONAL_ALLOW_LIST\n"
        "                (\n");

    for (unsigned int allowIdx = 0; allowIdx < strLstSize(allowList); allowIdx++)
    {
        const String *const value = strLstGet(allowList, allowIdx);

        strCatFmt(result, "                    %s,\n", strZ(bldCfgRenderScalar(value, optType)));
    }

    strCatZ(result, "                )");

    return result;
}

// Helper to render default
static String *
bldCfgRenderDefault(
    const String *const defaultValue, const bool defaultLiteral, const String *const optType)
{
    ASSERT(defaultValue != NULL);
    ASSERT(optType != NULL);

    String *const result = strNew();

    strCatZ(
        result,
        "                PARSE_RULE_OPTIONAL_DEFAULT\n"
        "                (\n");

    if (!strEq(optType, OPT_TYPE_STRING_STR) && !strEq(optType, OPT_TYPE_PATH_STR))
        strCatFmt(result, "                    %s,\n", strZ(bldCfgRenderScalar(defaultValue, optType)));

    if (!strEq(optType, OPT_TYPE_BOOLEAN_STR))
    {
        strCatFmt(
            result,
            "                    %s,\n",
            strZ(
                bldCfgRenderScalar(
                    strNewFmt("%s%s%s", defaultLiteral ? "" : "\"", strZ(defaultValue), defaultLiteral ? "" : "\""),
                    OPT_TYPE_STRING_STR)));
    }

    strCatZ(result, "                )");

    return result;
}

// Helper to add values to value lists
static void
bldCfgRenderValueAdd(
    const String *const optType, const String *const value, StringList *const ruleDataList, StringList *const ruleStrList)
{
    if (strEq(optType, OPT_TYPE_TIME_STR))
        strLstAddIfMissing(ruleDataList, strNewFmt("%" PRId64, (int64_t)(cvtZToDouble(strZ(value)) * 1000)));
    else
        strLstAddIfMissing(ruleDataList, value);

    if (ruleStrList != NULL && !strEq(optType, OPT_TYPE_STRING_STR) && !strEq(optType, OPT_TYPE_PATH_STR))
        strLstAddIfMissing(ruleStrList, strNewFmt("\"%s\"", strZ(value)));
}

static void
bldCfgRenderParseAutoC(const Storage *const storageRepo, const BldCfg bldCfg)
{
    String *const config = strNew();

    StringList *const ruleIntList = strLstNew();
    StringList *const ruleStrList = strLstNew();
    StringList *const ruleStrIdList = strLstNew();

    // Command parse rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command parse data\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(
        config, "%s\n",
        strZ(
            bldDefineRender(
                STRDEF("PARSE_RULE_VAL_CMD(value)"),
                strNewFmt("PARSE_RULE_U32_%zu(value)", bldCfgRenderVar128Size(lstSize(bldCfg.cmdList) - 1)))));

    strCatFmt(
        config,
        "\n"
        "static const ParseRuleCommand parseRuleCommand[CFG_COMMAND_TOTAL] =\n"
        "{\n");

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);

        bldCfgRenderLf(config, cmdIdx != 0);

        strCatFmt(
            config,
            COMMENT_SEPARATOR "\n"
            "    PARSE_RULE_COMMAND\n"
            "    (\n"
            "        PARSE_RULE_COMMAND_NAME(\"%s\"),\n",
            strZ(cmd->name));

        if (cmd->lockRequired)
            strCatZ(config, "        PARSE_RULE_COMMAND_LOCK_REQUIRED(true),\n");

        if (cmd->lockRemoteRequired)
            strCatZ(config, "        PARSE_RULE_COMMAND_LOCK_REMOTE_REQUIRED(true),\n");

        strCatFmt(config, "        PARSE_RULE_COMMAND_LOCK_TYPE(%s),\n", strZ(bldEnum("lockType", cmd->lockType)));

        if (cmd->logFile)
            strCatZ(config, "        PARSE_RULE_COMMAND_LOG_FILE(true),\n");

        strCatFmt(config, "        PARSE_RULE_COMMAND_LOG_LEVEL_DEFAULT(%s),\n", strZ(bldEnum("logLevel", cmd->logLevelDefault)));

        if (cmd->parameterAllowed)
            strCatZ(config, "        PARSE_RULE_COMMAND_PARAMETER_ALLOWED(true),\n");

        strCatFmt(
            config,
            "\n"
            "        PARSE_RULE_COMMAND_ROLE_VALID_LIST\n"
            "        (\n");

        for (unsigned int cmdRoleIdx = 0; cmdRoleIdx < strLstSize(cmd->roleList); cmdRoleIdx++)
        {
            strCatFmt(
                config,
                "            PARSE_RULE_COMMAND_ROLE(%s)\n",
                strZ(bldEnum("cfgCmdRole", strLstGet(cmd->roleList, cmdRoleIdx))));
        }

        strCatZ(
            config,
            "        ),\n"
            "    ),\n");
    }

    strCatZ(
        config,
        "};\n");

    // Option group rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option group parse data\n"
        COMMENT_BLOCK_END "\n"
        "static const ParseRuleOptionGroup parseRuleOptionGroup[CFG_OPTION_GROUP_TOTAL] =\n"
        "{\n");

    for (unsigned int optGrpIdx = 0; optGrpIdx < lstSize(bldCfg.optGrpList); optGrpIdx++)
    {
        const BldCfgOptionGroup *const optGrp = lstGet(bldCfg.optGrpList, optGrpIdx);

        bldCfgRenderLf(config, optGrpIdx != 0);

        strCatFmt(
            config,
            COMMENT_SEPARATOR "\n"
            "    PARSE_RULE_OPTION_GROUP\n"
            "    (\n"
            "        PARSE_RULE_OPTION_GROUP_NAME(\"%s\"),\n"
            "    ),\n",
            strZ(optGrp->name));
    }

    strCatZ(
        config,
        "};\n");

    // Option rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option parse data\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(
        config, "%s\n",
        strZ(
            bldDefineRender(
                STRDEF("PARSE_RULE_VAL_OPT(value)"),
                strNewFmt("PARSE_RULE_U32_%zu(value)", bldCfgRenderVar128Size(lstSize(bldCfg.optList) - 1)))));

    strCatFmt(
        config,
        "\n"
        "static const ParseRuleOption parseRuleOption[CFG_OPTION_TOTAL] =\n"
        "{\n");

    StringList *const cmdRoleAllList = strLstNew();
    strLstAdd(cmdRoleAllList, CMD_ROLE_MAIN_STR);
    strLstAdd(cmdRoleAllList, CMD_ROLE_ASYNC_STR);
    strLstAdd(cmdRoleAllList, CMD_ROLE_LOCAL_STR);
    strLstAdd(cmdRoleAllList, CMD_ROLE_REMOTE_STR);

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);
        StringList *const ruleDataList = strLstNew();
        bool ruleInt = false;

        bldCfgRenderLf(config, optIdx != 0);

        strCatFmt(
            config,
            COMMENT_SEPARATOR "\n"
            "    PARSE_RULE_OPTION\n"
            "    (\n"
            "        PARSE_RULE_OPTION_NAME(\"%s\"),\n"
            "        PARSE_RULE_OPTION_TYPE(%s),\n",
            strZ(opt->name), strZ(bldEnum("cfgOptType", opt->type)));

        if (opt->negate)
            strCatZ(config, "        PARSE_RULE_OPTION_NEGATE(true),\n");

        if (opt->reset)
            strCatZ(config, "        PARSE_RULE_OPTION_RESET(true),\n");

        strCatFmt(
            config,
            "        PARSE_RULE_OPTION_REQUIRED(%s),\n"
            "        PARSE_RULE_OPTION_SECTION(%s),\n",
            cvtBoolToConstZ(opt->required), strZ(bldEnum("cfgSection", opt->section)));

        if (opt->secure)
            strCatZ(config, "        PARSE_RULE_OPTION_SECURE(true),\n");

        if (strEq(opt->type, OPT_TYPE_HASH_STR) || strEq(opt->type, OPT_TYPE_LIST_STR))
            strCatZ(config, "        PARSE_RULE_OPTION_MULTI(true),\n");

        if (opt->group != NULL)
        {
            strCatFmt(
                config,
                "        PARSE_RULE_OPTION_GROUP_MEMBER(true),\n"
                "        PARSE_RULE_OPTION_GROUP_ID(%s),\n",
                strZ(bldEnum("cfgOptGrp", opt->group)));
        }

        // If an unindexed deprecation matches the base option name of an indexed option
        if (opt->deprecateList != NULL)
        {
            for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(opt->deprecateList); deprecateIdx++)
            {
                const BldCfgOptionDeprecate *const deprecate = lstGet(opt->deprecateList, deprecateIdx);

                if (strEq(deprecate->name, opt->name) && deprecate->unindexed)
                {
                    strCatZ(config, "        PARSE_RULE_OPTION_DEPRECATE_MATCH(true),\n");
                    break;
                }
            }
        }

        // Build command role valid lists
        for (unsigned int cmdRoleAllIdx = 0; cmdRoleAllIdx < strLstSize(cmdRoleAllList); cmdRoleAllIdx++)
        {
            String *const configRole = strNew();

            const String *const cmdRole = strLstGet(cmdRoleAllList, cmdRoleAllIdx);

            for (unsigned int optCmdIdx = 0; optCmdIdx < lstSize(opt->cmdList); optCmdIdx++)
            {
                BldCfgOptionCommand *const optCmd = lstGet(opt->cmdList, optCmdIdx);

                if (strLstExists(optCmd->roleList, cmdRole))
                {
                    strCatFmt(
                        configRole,
                        "            PARSE_RULE_OPTION_COMMAND(%s)\n", strZ(bldEnum("cfgCmd", optCmd->name)));
                }
            }

            if (!strEmpty(configRole))
            {
                strCatFmt(
                    config,
                    "\n"
                    "        PARSE_RULE_OPTION_COMMAND_ROLE_%s_VALID_LIST\n"
                    "        (\n"
                    "%s"
                    "        ),\n",
                    strZ(strUpper(strDup(cmdRole))), strZ(configRole));
            }
        }

        // Build default optional rules
        KeyValue *const optionalDefaultRule = kvNew();
        const Variant *const ruleDepend = VARSTRDEF("01-depend");
        const Variant *const ruleAllowRange = VARSTRDEF("02-allow-range");
        const Variant *const ruleAllowList = VARSTRDEF("03-allow-list");
        const Variant *const ruleDefault = VARSTRDEF("04-default");
        const Variant *const ruleRequire = VARSTRDEF("05-require");
        const Variant *const ruleList[] = {ruleDepend, ruleAllowRange, ruleAllowList, ruleDefault, ruleRequire};

        if (opt->depend)
            kvAdd(optionalDefaultRule, ruleDepend, VARSTR(bldCfgRenderValid(opt->depend)));

        if (opt->allowRangeMin != NULL)
        {
            kvAdd(
                optionalDefaultRule, ruleAllowRange,
                VARSTR(bldCfgRenderAllowRange(opt->allowRangeMin, opt->allowRangeMax, opt->type)));

            bldCfgRenderValueAdd(opt->type, opt->allowRangeMin, ruleDataList, NULL);
            bldCfgRenderValueAdd(opt->type, opt->allowRangeMax, ruleDataList, NULL);
        }

        if (opt->allowList != NULL)
        {
            kvAdd(optionalDefaultRule, ruleAllowList, VARSTR(bldCfgRenderAllowList(opt->allowList, opt->type)));

            for (unsigned int allowIdx = 0; allowIdx < strLstSize(opt->allowList); allowIdx++)
                bldCfgRenderValueAdd(opt->type, strLstGet(opt->allowList, allowIdx), ruleDataList, NULL);
        }

        if (opt->defaultValue != NULL)
        {
            kvAdd(optionalDefaultRule, ruleDefault, VARSTR(bldCfgRenderDefault(opt->defaultValue, opt->defaultLiteral, opt->type)));

            if (!strEq(opt->type, OPT_TYPE_BOOLEAN_STR))
                bldCfgRenderValueAdd(opt->type, opt->defaultValue, ruleDataList, ruleStrList);
        }

        // Build command optional rules
        KeyValue *const optionalCmdRule = kvNew();

        for (unsigned int optCmdIdx = 0; optCmdIdx < lstSize(opt->cmdList); optCmdIdx++)
        {
            BldCfgOptionCommand *const optCmd = lstGet(opt->cmdList, optCmdIdx);
            KeyValue *const optionalCmdRuleType = kvNew();

            // Depends
            if (optCmd->depend != NULL)
                kvAdd(optionalCmdRuleType, ruleDepend, VARSTR(bldCfgRenderValid(optCmd->depend)));

            // Allow lists
            if (optCmd->allowList != NULL)
            {
                kvAdd(optionalCmdRuleType, ruleAllowList, VARSTR(bldCfgRenderAllowList(optCmd->allowList, opt->type)));

                for (unsigned int allowIdx = 0; allowIdx < strLstSize(optCmd->allowList); allowIdx++)
                    bldCfgRenderValueAdd(opt->type, strLstGet(optCmd->allowList, allowIdx), ruleDataList, NULL);
            }

            // Defaults
            if (optCmd->defaultValue != NULL)
            {
                kvAdd(
                    optionalCmdRuleType, ruleDefault,
                    VARSTR(bldCfgRenderDefault(optCmd->defaultValue, opt->defaultLiteral, opt->type)));

                if (!strEq(opt->type, OPT_TYPE_BOOLEAN_STR))
                    bldCfgRenderValueAdd(opt->type, optCmd->defaultValue, ruleDataList, ruleStrList);
            }

            // Requires
            if (optCmd->required != opt->required)
            {
                kvAdd(
                    optionalCmdRuleType, ruleRequire,
                    VARSTR(strNewFmt("                PARSE_RULE_OPTIONAL_%sREQUIRED()", optCmd->required ? "" : "NOT_")));
            }

            // Add defaults
            if (varLstSize(kvKeyList(optionalCmdRuleType)) > 0)
            {
                for (unsigned int ruleIdx = 0; ruleIdx < sizeof(ruleList) / sizeof(Variant *); ruleIdx++)
                {
                    if (kvKeyExists(optionalCmdRuleType, ruleList[ruleIdx]))
                        kvAdd(optionalCmdRule, VARSTR(optCmd->name), kvGet(optionalCmdRuleType, ruleList[ruleIdx]));
                    else if (kvKeyExists(optionalDefaultRule, ruleList[ruleIdx]))
                        kvAdd(optionalCmdRule, VARSTR(optCmd->name), kvGet(optionalDefaultRule, ruleList[ruleIdx]));
                }
            }
        }

        // Add optional rules
        unsigned int optionalCmdRuleSize = varLstSize(kvKeyList(optionalCmdRule));
        unsigned int optionalDefaultRuleSize = varLstSize(kvKeyList(optionalDefaultRule));

        if (optionalCmdRuleSize != 0 || optionalDefaultRuleSize != 0)
        {
            strCatZ(
                config,
                "\n"
                "        PARSE_RULE_OPTIONAL\n"
                "        (\n");

            if (optionalCmdRuleSize != 0)
            {
                KeyValue *const combine = kvNew();

                for (unsigned int ruleIdx = 0; ruleIdx < optionalCmdRuleSize; ruleIdx++)
                {
                    const Variant *const cmd = varLstGet(kvKeyList(optionalCmdRule), ruleIdx);
                    const VariantList *const groupList = kvGetList(optionalCmdRule, cmd);
                    String *const group = strNew();

                    for (unsigned int groupIdx = 0; groupIdx < varLstSize(groupList); groupIdx++)
                    {
                        strCatFmt(
                            group,
                            "\n"
                            "%s,\n",
                            strZ(varStr(varLstGet(groupList, groupIdx))));
                    }

                    kvAdd(combine, VARSTR(group), cmd);
                }

                unsigned int combineSize = varLstSize(kvKeyList(combine));

                for (unsigned int ruleIdx = 0; ruleIdx < combineSize; ruleIdx++)
                {
                    const Variant *const group = varLstGet(kvKeyList(combine), ruleIdx);
                    const VariantList *const cmdList = kvGetList(combine, group);

                    if (ruleIdx != 0)
                        strCatChr(config, '\n');

                    strCatZ(
                        config,
                        "            PARSE_RULE_OPTIONAL_GROUP\n"
                        "            (\n"
                        "                PARSE_RULE_FILTER_CMD\n"
                        "                (\n");

                    for (unsigned int cmdIdx = 0; cmdIdx < varLstSize(cmdList); cmdIdx++)
                    {
                        strCatFmt(
                            config,
                            "                    PARSE_RULE_VAL_CMD(%s),\n",
                            strZ(bldEnum("cfgCmd", varStr(varLstGet(cmdList, cmdIdx)))));
                    }

                    strCatFmt(
                        config,
                        "                ),\n"
                        "%s"
                        "            ),\n",
                        strZ(varStr(group)));
                }
            }

            if (optionalDefaultRuleSize != 0)
            {
                if (optionalCmdRuleSize != 0)
                    strCatChr(config, '\n');

                strCatZ(
                    config,
                    "            PARSE_RULE_OPTIONAL_GROUP\n"
                    "            (\n");

                for (unsigned int ruleIdx = 0; ruleIdx < optionalDefaultRuleSize; ruleIdx++)
                {
                    const Variant *const key = varLstGet(kvKeyList(optionalDefaultRule), ruleIdx);

                    if (ruleIdx != 0)
                        strCatChr(config, '\n');

                    strCatFmt(
                        config,
                        "%s,\n",
                        strZ(varStr(kvGet(optionalDefaultRule, key))));
                }

                strCatZ(config, "            ),\n");
            }

            strCatZ(config, "        ),\n");
        }

        strCatZ(config, "    ),\n");

        // Build rule values
        if (strEq(opt->type, OPT_TYPE_BOOLEAN_STR))
            continue;

        StringList *ruleAddList = NULL;

        if (strEq(opt->type, OPT_TYPE_STRING_STR) || strEq(opt->type, OPT_TYPE_PATH_STR))
        {
            ruleAddList = ruleStrList;
        }
        else if (strEq(opt->type, OPT_TYPE_STRING_ID_STR))
        {
            ruleAddList = ruleStrIdList;
        }
        else
        {
            ruleAddList = ruleIntList;
            ruleInt = true;
        }

        for (unsigned int ruleDataIdx = 0; ruleDataIdx < strLstSize(ruleDataList); ruleDataIdx++)
        {
            if (ruleInt)
            {
                strLstAddIfMissing(ruleAddList, strNewFmt("%20s", strZ(strLstGet(ruleDataList, ruleDataIdx))));
            }
            else if (strEq(opt->type, OPT_TYPE_STRING_ID_STR))
            {
                strLstAddIfMissing(ruleAddList, strLstGet(ruleDataList, ruleDataIdx));
            }
            else
            {
                strLstAddIfMissing(
                    ruleAddList,
                    strNewFmt(
                        "%s%s%s", opt->defaultLiteral ? "" : "\"", strZ(strLstGet(ruleDataList, ruleDataIdx)),
                        opt->defaultLiteral ? "" : "\""));
            }
        }
    }

    strCatZ(
        config,
        "};\n");

    // Option deprecations
    // -----------------------------------------------------------------------------------------------------------------------------
    List *const deprecateCombineList = lstNewP(sizeof(BldCfgRenderOptionDeprecate), .comparator = lstComparatorStr);

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);

        if (opt->deprecateList != NULL)
        {
            for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(opt->deprecateList); deprecateIdx++)
            {
                const BldCfgOptionDeprecate *const deprecate = lstGet(opt->deprecateList, deprecateIdx);

                lstAdd(
                    deprecateCombineList,
                    &(BldCfgRenderOptionDeprecate)
                    {
                        .name = deprecate->name,
                        .option = opt,
                        .indexed = deprecate->indexed,
                        .unindexed = deprecate->unindexed,
                    });
            }
        }
    }

    lstSort(deprecateCombineList, sortOrderAsc);

    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option deprecations\n"
        COMMENT_BLOCK_END "\n"
        "%s\n"
        "\n"
        "static const ParseRuleOptionDeprecate parseRuleOptionDeprecate[CFG_OPTION_DEPRECATE_TOTAL] =\n"
        "{\n",
        strZ(bldDefineRender(STRDEF("CFG_OPTION_DEPRECATE_TOTAL"), strNewFmt("%u", lstSize(deprecateCombineList)))));

    for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(deprecateCombineList); deprecateIdx++)
    {
        const BldCfgRenderOptionDeprecate *const deprecate = lstGet(deprecateCombineList, deprecateIdx);

        bldCfgRenderLf(config, deprecateIdx != 0);

        strCatFmt(
            config,
            "    // %s deprecation\n"
            "    {\n"
            "        .name = \"%s\",\n"
            "        .id = %s,\n",
            strZ(deprecate->option->name), strZ(deprecate->name), strZ(bldEnum("cfgOpt", deprecate->option->name)));

        if (deprecate->indexed)
            strCatZ(config, "        .indexed = true,\n");

        if (deprecate->unindexed)
            strCatZ(config, "        .unindexed = true,\n");

        strCatZ(config, "    },\n");
    }

    strCatZ(config, "};\n");

    // Order for option parse resolution
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Order for option parse resolution\n"
        COMMENT_BLOCK_END "\n"
        "static const ConfigOption optionResolveOrder[] =\n"
        "{\n");

    // Render resolve order
    for (unsigned int optResolveIdx = 0; optResolveIdx < lstSize(bldCfg.optResolveList); optResolveIdx++)
    {
        strCatFmt(
            config, "    %s,\n", strZ(bldEnum("cfgOpt", (*(BldCfgOption **)lstGet(bldCfg.optResolveList, optResolveIdx))->name)));
    }

    strCatZ(config, "};\n");


    // Rule Strings
    // -----------------------------------------------------------------------------------------------------------------------------
    String *const configVal = strNew();

    strLstSort(ruleStrList, sortOrderAsc);

    strCatFmt(
        configVal,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Rule Strings\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(
        configVal, "%s\n",
        strZ(
            bldDefineRender(
                STRDEF("PARSE_RULE_VAL_STR(value)"),
                strNewFmt("PARSE_RULE_U32_%zu(value)", bldCfgRenderVar128Size(strLstSize(ruleStrList) - 1)))));

    strCatFmt(
        configVal,
        "\n"
        "static const StringPub parseRuleValueStr[] =\n"
        "{\n");

    for (unsigned int ruleStrIdx = 0; ruleStrIdx < strLstSize(ruleStrList); ruleStrIdx++)
        strCatFmt(configVal, "    PARSE_RULE_STRPUB(%s),\n", strZ(strLstGet(ruleStrList, ruleStrIdx)));

    strCatZ(configVal, "};\n");

    strCatZ(
        configVal,
        "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int ruleStrIdx = 0; ruleStrIdx < strLstSize(ruleStrList); ruleStrIdx++)
    {
        strCatFmt(
            configVal, "    %s,\n",
            strZ(strNewFmt("parseRuleValStr%s", strZ(bldCfgRenderEnumStr(strLstGet(ruleStrList, ruleStrIdx))))));
    }

    strCatZ(configVal, "} ParseRuleValueStr;\n");

    // Rule StringIds
    // -----------------------------------------------------------------------------------------------------------------------------
    strLstSort(ruleStrIdList, sortOrderAsc);

    strCatFmt(
        configVal,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Rule StringIds\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(
        configVal, "%s\n",
        strZ(
            bldDefineRender(
                STRDEF("PARSE_RULE_VAL_STRID(value)"),
                strNewFmt("PARSE_RULE_U32_%zu(value)", bldCfgRenderVar128Size(strLstSize(ruleStrIdList) - 1)))));

    strCatFmt(
        configVal,
        "\n"
        "static const StringId parseRuleValueStrId[] =\n"
        "{\n");

    for (unsigned int ruleStrIdIdx = 0; ruleStrIdIdx < strLstSize(ruleStrIdList); ruleStrIdIdx++)
        strCatFmt(configVal, "    %s,\n", strZ(bldStrId(strZ(strLstGet(ruleStrIdList, ruleStrIdIdx)))));

    strCatZ(configVal, "};\n");

    strCatZ(
        configVal,
        "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int ruleStrIdIdx = 0; ruleStrIdIdx < strLstSize(ruleStrIdList); ruleStrIdIdx++)
        strCatFmt(configVal, "    %s,\n", strZ(bldEnum("parseRuleValStrId", strLstGet(ruleStrIdList, ruleStrIdIdx))));

    strCatZ(configVal, "} ParseRuleValueStrId;\n");

    // Rule Ints
    // -----------------------------------------------------------------------------------------------------------------------------
    strLstSort(ruleIntList, sortOrderAsc);

    strCatFmt(
        configVal,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Rule Ints\n"
        COMMENT_BLOCK_END "\n");

    strCatFmt(
        configVal, "%s\n",
        strZ(
            bldDefineRender(
                STRDEF("PARSE_RULE_VAL_INT(value)"),
                strNewFmt("PARSE_RULE_U32_%zu(value)", bldCfgRenderVar128Size(strLstSize(ruleIntList) - 1)))));

    strCatFmt(
        configVal,
        "\n"
        "static const int64_t parseRuleValueInt[] =\n"
        "{\n");

    for (unsigned int ruleIntIdx = 0; ruleIntIdx < strLstSize(ruleIntList); ruleIntIdx++)
        strCatFmt(configVal, "    %s,\n", strZ(strTrim(strLstGet(ruleIntList, ruleIntIdx))));

    strCatZ(configVal, "};\n");

    strCatZ(
        configVal,
        "\n"
        "typedef enum\n"
        "{\n");

    for (unsigned int ruleIntIdx = 0; ruleIntIdx < strLstSize(ruleIntList); ruleIntIdx++)
        strCatFmt(configVal, "    %s,\n", strZ(bldEnum("parseRuleValInt", strLstGet(ruleIntList, ruleIntIdx))));

    strCatZ(configVal, "} ParseRuleValueInt;\n");

    // Write to storage
    // -----------------------------------------------------------------------------------------------------------------------------
    bldPut(
        storageRepo, "src/config/parse.auto.c",
        BUFSTR(strNewFmt("%s%s%s", strZ(bldHeader(CONFIG_MODULE, PARSE_AUTO_COMMENT)), strZ(configVal), strZ(config))));
}

/**********************************************************************************************************************************/
void
bldCfgRender(const Storage *const storageRepo, const BldCfg bldCfg)
{
    bldCfgRenderConfigAutoH(storageRepo, bldCfg);
    bldCfgRenderParseAutoC(storageRepo, bldCfg);
}
