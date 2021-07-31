/***********************************************************************************************************************************
Render Configuration Yaml
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
    String *const result = strNewZ(prefix);
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
    String *config = strNewFmt(
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

        if (strEqZ(opt->type, CFGDEF_TYPE_STRING))
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

            if (!strLstEmpty(allowList))
            {
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
    const String *const name;                                       // Deprecated name
    const BldCfgOption *const option;                               // Option name
    const bool indexed;                                             // Can the deprecation be indexed?
    const bool unindexed;                                           // Can the deprecation be unindexed?
} BldCfgRenderOptionDeprecate;

static void
bldCfgRenderLf(String *const config, const bool lf)
{
    if (lf)
        strCatZ(config, "\n");
}

static const String *
bldCfgRenderValue(const String *const type, const String *const value)
{
    // Translate boolean values
    if (strEq(type, OPT_TYPE_BOOLEAN_STR))
    {
        if (strEq(value, FALSE_STR))
            return ZERO_STR;

        CHECK(strEq(value, TRUE_STR));
        return strNewZ("1");
    }
    // Translate time values
    else if (strEq(type, OPT_TYPE_TIME_STR))
        return strNewFmt("%" PRId64, (int64_t)(cvtZToDouble(strZ(value)) * 1000));

    return value;
}

// Helper to render allow list
static bool
bldCfgRenderAllowList(String *const config, const StringList *const allowList, const bool command)
{
    if (allowList != NULL)
    {
        const char *indent = command ? "                " : "            ";

        strCatFmt(
            config,
            "%sPARSE_RULE_OPTION_OPTIONAL_ALLOW_LIST\n"
            "%s(\n",
            indent, indent);

        for (unsigned int allowIdx = 0; allowIdx < strLstSize(allowList); allowIdx++)
        {
            strCatFmt(
                config,
                "%s    \"%s\"%s\n",
                indent, strZ(strLstGet(allowList, allowIdx)),
                allowIdx < strLstSize(allowList) - 1 ? "," : "");
        }

        strCatFmt(config, "%s),\n", indent);

        return true;
    }

    return false;
}

// Helper to render default
static bool
bldCfgRenderDefault(
    String *const config, const String *const type, const String *const defaultValue,
    const bool defaultLiteral, const bool command, bool multi)
{
    if (defaultValue != NULL)
    {
        const char *indent = command ? "                " : "            ";

        bldCfgRenderLf(config, multi);

        strCatFmt(
            config,
            "%sPARSE_RULE_OPTION_OPTIONAL_DEFAULT(%s%s%s),\n",
            indent, defaultLiteral ? "" : "\"", strZ(bldCfgRenderValue(type, defaultValue)), defaultLiteral ? "" : "\"");

        return false;
    }

    return multi;
}

// Helper to render depend
static bool
bldCfgRenderDepend(String *const config, const BldCfgOptionDepend *const depend, const bool command, const bool multi)
{
    if (depend != NULL)
    {
        const char *indent = command ? "                " : "            ";

        bldCfgRenderLf(config, multi);

        if (depend->valueList != NULL)
        {
            strCatFmt(
                config,
                "%sPARSE_RULE_OPTION_OPTIONAL_DEPEND_LIST\n"
                "%s(\n"
                "%s    %s,\n",
                indent, indent, indent, strZ(bldEnum("cfgOpt", depend->option->name)));

            for (unsigned int valueIdx = 0; valueIdx < strLstSize(depend->valueList); valueIdx++)
            {
                strCatFmt(
                    config, "%s    \"%s\"%s\n", indent,
                    strZ(bldCfgRenderValue(depend->option->type, strLstGet(depend->valueList, valueIdx))),
                    valueIdx < strLstSize(depend->valueList) - 1 ? "," : "");
            }

            strCatFmt(config, "%s),\n", indent);

            return true;
        }
        else
        {
            strCatFmt(
                config,
                "%sPARSE_RULE_OPTION_OPTIONAL_DEPEND(%s),\n",
                indent, strZ(bldEnum("cfgOpt", depend->option->name)));

            return false;
        }
    }

    return multi;
}

static void
bldCfgRenderParseAutoC(const Storage *const storageRepo, const BldCfg bldCfg)
{
    String *const config = bldHeader(CONFIG_MODULE, PARSE_AUTO_COMMENT);

    // Command parse rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command parse data\n"
        COMMENT_BLOCK_END "\n"
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
        COMMENT_BLOCK_END "\n"
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

        // Build optional data
        String *const configOptional = strNew();

        if (opt->allowRangeMin != NULL)
        {
            CHECK(opt->allowRangeMax != NULL);

            const String *allowRangeMin = opt->allowRangeMin;
            const String *allowRangeMax = opt->allowRangeMax;

            if (strEq(opt->type, OPT_TYPE_TIME_STR))
            {
                allowRangeMin = strNewFmt("%" PRId64, (int64_t)(cvtZToDouble(strZ(allowRangeMin)) * 1000));
                allowRangeMax = strNewFmt("%" PRId64, (int64_t)(cvtZToDouble(strZ(allowRangeMax)) * 1000));
            }

            strCatFmt(
                configOptional,
                "            PARSE_RULE_OPTION_OPTIONAL_ALLOW_RANGE(%s, %s),\n",
                strZ(allowRangeMin), strZ(allowRangeMax));
        }

        bool multi = bldCfgRenderAllowList(configOptional, opt->allowList, false);
        multi = bldCfgRenderDepend(configOptional, opt->depend, false, multi);
        multi = bldCfgRenderDefault(configOptional, opt->type, opt->defaultValue, opt->defaultLiteral, false, multi);

        // Build optional data for commands
        for (unsigned int optCmdIdx = 0; optCmdIdx < lstSize(opt->cmdList); optCmdIdx++)
        {
            BldCfgOptionCommand *const optCmd = lstGet(opt->cmdList, optCmdIdx);
            String *const configCommand = strNew();

            bool multi = bldCfgRenderAllowList(configCommand, optCmd->allowList, true);
            multi = bldCfgRenderDepend(configCommand, optCmd->depend, true, multi);
            multi = bldCfgRenderDefault(configCommand, opt->type, optCmd->defaultValue, false, true, multi);

            if (optCmd->required != opt->required)
            {
                bldCfgRenderLf(configCommand, multi);

                strCatFmt(
                    configCommand,
                    "                PARSE_RULE_OPTION_OPTIONAL_REQUIRED(%s),\n",
                    cvtBoolToConstZ(optCmd->required));
            }

            if (!strEmpty(configCommand))
            {
                bldCfgRenderLf(configOptional, !strEmpty(configOptional));

                strCatFmt(
                    configOptional,
                    "            PARSE_RULE_OPTION_OPTIONAL_COMMAND_OVERRIDE\n"
                    "            (\n"
                    "                PARSE_RULE_OPTION_OPTIONAL_COMMAND(%s),\n"
                    "\n"
                    "%s"
                    "            )\n",
                    strZ(bldEnum("cfgCmd", optCmd->name)), strZ(configCommand));
            }
        }

        // Add optional data
        if (!strEmpty(configOptional))
        {
            strCatFmt(
                config,
                "\n"
                "        PARSE_RULE_OPTION_OPTIONAL_LIST\n"
                "        (\n"
                "%s"
                "        ),\n",
                strZ(configOptional));
        }

        strCatZ(config, "    ),\n");
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

    // Write to storage
    // -----------------------------------------------------------------------------------------------------------------------------
    bldPut(storageRepo, "src/config/parse.auto.c", BUFSTR(config));
}

/**********************************************************************************************************************************/
void
bldCfgRender(const Storage *const storageRepo, const BldCfg bldCfg)
{
    bldCfgRenderConfigAutoH(storageRepo, bldCfg);
    bldCfgRenderParseAutoC(storageRepo, bldCfg);
}
