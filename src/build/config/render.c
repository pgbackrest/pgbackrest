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
            "        PARSE_RULE_OPTION_TYPE(%s),\n"
            "        PARSE_RULE_OPTION_REQUIRED(%s),\n"
            "        PARSE_RULE_OPTION_SECTION(%s),\n",
            strZ(opt->name), strZ(bldEnum("cfgOptType", opt->type)), cvtBoolToConstZ(opt->required),
            strZ(bldEnum("cfgSection", opt->section)));

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

    // Option data for getopt_long()
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option data for getopt_long()\n"
        COMMENT_BLOCK_END "\n"
        "static const struct option optionList[] =\n"
        "{\n");

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);

        // Determine if the option is indexed
        unsigned int indexTotal = 1;
        const BldCfgOptionGroup *optGrp = NULL;

        if (opt->group != NULL)
        {
            optGrp = lstFind(bldCfg.optGrpList, &opt->group);
            CHECK(optGrp != NULL);

            indexTotal = optGrp->indexTotal;
        }

        bldCfgRenderLf(config, optIdx != 0);

        strCatFmt(
            config,
            "    // %s option%s\n"
            COMMENT_SEPARATOR "\n",
            strZ(opt->name), opt->deprecateList != NULL ? " and deprecations" : "");

        for (unsigned int index = 0; index < indexTotal; index++)
        {
            const String *optName = opt->name;
            const String *optShift = EMPTY_STR;

            // Generate indexed name
            if (optGrp != NULL)
            {
                optName = strNewFmt("%s%u%s", strZ(optGrp->name), index + 1, strZ(strSub(optName, strSize(optGrp->name))));
                optShift = strNewFmt(" | (%u << PARSE_KEY_IDX_SHIFT)", index);
            }

            strCatFmt(
                config,
                "    {\n"
                "        .name = \"%s\",\n"
                "%s"
                "        .val = PARSE_OPTION_FLAG%s | %s,\n"
                "    },\n",
                strZ(optName),
                strEq(opt->type, OPT_TYPE_BOOLEAN_STR) ? "" : "        .has_arg = required_argument,\n", strZ(optShift),
                strZ(bldEnum("cfgOpt", opt->name)));

            if (opt->negate)
            {
                strCatFmt(
                    config,
                    "    {\n"
                    "        .name = \"no-%s\",\n"
                    "        .val = PARSE_OPTION_FLAG | PARSE_NEGATE_FLAG%s | %s,\n"
                    "    },\n",
                    strZ(optName), strZ(optShift), strZ(bldEnum("cfgOpt", opt->name)));
            }

            if (opt->reset)
            {
                strCatFmt(
                    config,
                    "    {\n"
                    "        .name = \"reset-%s\",\n"
                    "        .val = PARSE_OPTION_FLAG | PARSE_RESET_FLAG%s | %s,\n"
                    "    },\n",
                    strZ(optName), strZ(optShift), strZ(bldEnum("cfgOpt", opt->name)));
            }

            if (opt->deprecateList != 0)
            {
                for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(opt->deprecateList); deprecateIdx++)
                {
                    const BldCfgOptionDeprecate *const deprecate = lstGet(opt->deprecateList, deprecateIdx);
                    const String *deprecateName = deprecate->name;

                    // Skip the deprecation if it does not apply to this index
                    if (deprecate->index > 0 && deprecate->index != index + 1)
                        continue;

                    // Generate name if deprecation applies to all indexes
                    int deprecateIndexPos = strChr(deprecateName, '?');

                    if (deprecateIndexPos != -1)
                    {
                        CHECK(optGrp != NULL);

                        deprecateName = strNewFmt(
                            "%s%u%s", strZ(strSubN(deprecateName, 0, (size_t)deprecateIndexPos)), index + 1,
                            strZ(strSub(deprecateName, (size_t)deprecateIndexPos + 1)));
                    }

                    strCatFmt(
                        config,
                        "    {\n"
                        "        .name = \"%s\",\n"
                        "%s"
                        "        .val = PARSE_OPTION_FLAG | PARSE_DEPRECATE_FLAG%s | %s,\n"
                        "    },\n",
                        strZ(deprecateName),
                        strEq(opt->type, OPT_TYPE_BOOLEAN_STR) ? "" : "        .has_arg = required_argument,\n", strZ(optShift),
                        strZ(bldEnum("cfgOpt", opt->name)));

                    if (opt->negate)
                    {
                        strCatFmt(
                            config,
                            "    {\n"
                            "        .name = \"no-%s\",\n"
                            "        .val = PARSE_OPTION_FLAG | PARSE_DEPRECATE_FLAG | PARSE_NEGATE_FLAG%s | %s,\n"
                            "    },\n",
                            strZ(deprecateName), strZ(optShift), strZ(bldEnum("cfgOpt", opt->name)));
                    }

                    if (opt->reset && deprecate->reset)
                    {
                        strCatFmt(
                            config,
                            "    {\n"
                            "        .name = \"reset-%s\",\n"
                            "        .val = PARSE_OPTION_FLAG | PARSE_DEPRECATE_FLAG | PARSE_RESET_FLAG%s | %s,\n"
                            "    },\n",
                            strZ(deprecateName), strZ(optShift), strZ(bldEnum("cfgOpt", opt->name)));
                    }
                }
            }
        }
    }

    strCatZ(
        config,
        "    // Terminate option list\n"
        "    {\n"
        "        .name = NULL\n"
        "    }\n"
        "};\n");

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
