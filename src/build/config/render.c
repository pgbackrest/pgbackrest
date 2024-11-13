/***********************************************************************************************************************************
Render Configuration Data
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/log.h"
#include "common/type/convert.h"
#include "config/common.h"
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
                for (unsigned int allowListIdx = 0; allowListIdx < lstSize(opt->allowList); allowListIdx++)
                    strLstAddIfMissing(allowList, ((const BldCfgOptionValue *)lstGet(opt->allowList, allowListIdx))->value);
            }

            for (unsigned int optCmdListIdx = 0; optCmdListIdx < lstSize(opt->cmdList); optCmdListIdx++)
            {
                BldCfgOptionCommand *optCmd = lstGet(opt->cmdList, optCmdListIdx);

                if (optCmd->allowList != NULL)
                {
                    for (unsigned int allowListIdx = 0; allowListIdx < lstSize(optCmd->allowList); allowListIdx++)
                        strLstAddIfMissing(allowList, ((const BldCfgOptionValue *)lstGet(optCmd->allowList, allowListIdx))->value);
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
Render parse.auto.c.inc
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

static String *
bldCfgRenderLabel(const String *const config, const bool label, const String *const labelText)
{
    const StringList *const lines = strLstNewSplitZ(config, "\n");
    String *const result = strNew();

    const String *const labelComment = strNewFmt("// %s", strZ(labelText));

    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lines); lineIdx++)
    {
        const String *const line = strLstGet(lines, lineIdx);

        if (lineIdx != 0)
            strCatChr(result, '\n');

        if (!label || strSize(line) + 1 + strSize(labelComment) > 132)
            strCat(result, line);
        else
            strCatFmt(result, "%s%*s", strZ(line), (int)(132 - strSize(line)), strZ(labelComment));
    }

    return result;
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
            case '-':
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

                    case '-':
                        strCatZ(result, "DS");
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
        return strNewFmt("PARSE_RULE_VAL_STRID(%s)", strZ(bldEnum("", scalar)));

    if (strEq(optType, OPT_TYPE_STRING_STR))
        return strNewFmt("PARSE_RULE_VAL_STR(%s)", strZ(bldCfgRenderEnumStr(scalar)));

    if (strEq(optType, OPT_TYPE_BOOLEAN_STR))
        return strNewFmt("PARSE_RULE_VAL_BOOL_%s", strEqZ(scalar, "true") ? "TRUE" : "FALSE");

    if (strEq(optType, OPT_TYPE_INTEGER_STR))
        return strNewFmt("PARSE_RULE_VAL_INT(%s)", strZ(bldCfgRenderEnumStr(scalar)));

    if (strEq(optType, OPT_TYPE_SIZE_STR))
        return strNewFmt("PARSE_RULE_VAL_SIZE(%s)", strZ(scalar));

    CHECK_FMT(AssertError, strEq(optType, OPT_TYPE_TIME_STR), "invalid type '%s'", strZ(optType));

    return strNewFmt("PARSE_RULE_VAL_TIME(%s)", strZ(scalar));

    // WHERE IS THE 1 TIME COMING FROM?
}

// Helper to render validity
static String *
bldCfgRenderValid(const BldCfgOptionDepend *const depend)
{
    ASSERT(depend != NULL);

    String *const result = strNew();

    strCatZ(
        result,
        "                PARSE_RULE_OPTIONAL_DEPEND\n"
        "                (\n");

    if (depend->defaultValue != NULL)
    {
        strCatFmt(
            result,
            "                    PARSE_RULE_OPTIONAL_DEPEND_DEFAULT(%s),\n",
            strZ(bldCfgRenderScalar(depend->defaultValue, OPT_TYPE_BOOLEAN_STR)));
    }

    strCatFmt(
        result,
        "                    PARSE_RULE_VAL_OPT(%s),\n",
        strZ(bldEnum("", depend->option->name)));

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
bldCfgRenderAllowList(const List *const allowList, const String *const optType)
{
    ASSERT(allowList != NULL);
    ASSERT(optType != NULL);

    String *const result = strNew();

    strCatZ(
        result,
        "                PARSE_RULE_OPTIONAL_ALLOW_LIST\n"
        "                (\n");

    for (unsigned int allowIdx = 0; allowIdx < lstSize(allowList); allowIdx++)
    {
        const BldCfgOptionValue *const allow = lstGet(allowList, allowIdx);

        strCatFmt(result, "                    %s,\n", strZ(bldCfgRenderScalar(allow->value, optType)));

        if (allow->condition != NULL)
        {
            strCatFmt(
                result,
                "#ifndef %s\n"
                "                        PARSE_RULE_BOOL_FALSE,\n"
                "#endif\n",
                strZ(allow->condition));
        }
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
    else
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
#define BLD_CFG_RENDER_VALUE_MAP_MAX                                UINT8_MAX

static void
bldCfgRenderValueAdd(const String *optType, const bool literal, const String *const value, KeyValue *const ruleValMap)
{
    ASSERT(!strEq(optType, OPT_TYPE_BOOLEAN_STR) && !strEq(optType, OPT_TYPE_HASH_STR) && !strEq(optType, OPT_TYPE_LIST_STR));

    // Remap path to string
    if (strEq(optType, OPT_TYPE_PATH_STR))
        optType = OPT_TYPE_STRING_STR;

    // Generate string value
    const String *const valueStr = strNewFmt("%s%s%s", literal ? "" : "\"", strZ(value), literal ? "" : "\"");

    // Add values other than strings
    if (!strEq(optType, OPT_TYPE_STRING_STR))
    {
        if (kvGet(ruleValMap, VARSTR(optType)) == NULL)
            kvPutKv(ruleValMap, VARSTR(optType));

        kvPut(varKv(kvGet(ruleValMap, VARSTR(optType))), VARSTR(value), VARSTR(valueStr));
    }

    // Always add to string list
    if (kvGet(ruleValMap, VARSTR(OPT_TYPE_STRING_STR)) == NULL)
        kvPutKv(ruleValMap, VARSTR(OPT_TYPE_STRING_STR));

    kvPut(varKv(kvGet(ruleValMap, VARSTR(OPT_TYPE_STRING_STR))), VARSTR(valueStr), NULL);
}

// Helper to render value lists
static int
bldCfgRenderComparatorInt(const void *const int64Ptr1, const void *const int64Ptr2)
{
    return LST_COMPARATOR_CMP(
        cvtZToInt64(strZ(*(const String *const *)int64Ptr1)), cvtZToInt64(strZ(*(const String *const *)int64Ptr2)));
}

static int
bldCfgRenderComparatorSize(const void *const sizePtr1, const void *const sizePtr2)
{
    return LST_COMPARATOR_CMP(cfgParseSize(*(const String *const *)sizePtr1), cfgParseSize(*(const String *const *)sizePtr2));
}

static int
bldCfgRenderComparatorTime(const void *const timePtr1, const void *const timePtr2)
{
    return LST_COMPARATOR_CMP(cfgParseTime(*(const String *const *)timePtr1), cfgParseTime(*(const String *const *)timePtr2));
}

static String *
bldCfgRenderValueRender(
    const String *const optType, const KeyValue *const ruleValAllMap, const bool label, const char *const type,
    const char *const caption, const char *const abbr, const char *const macro, const char *const comment)
{
    String *const result = strNew();
    const KeyValue *const ruleValMap = varKv(kvGet(ruleValAllMap, VARSTR(optType)));
    StringList *const ruleValList = strLstNewVarLst(kvKeyList(ruleValMap));
    ASSERT(!strEq(optType, OPT_TYPE_STRING_STR) || strLstSize(ruleValList) <= BLD_CFG_RENDER_VALUE_MAP_MAX);

    // Sort list
    if (strEq(optType, OPT_TYPE_INTEGER_STR))
        lstComparatorSet((List *)ruleValList, bldCfgRenderComparatorInt);
    else if (strEq(optType, OPT_TYPE_SIZE_STR))
        lstComparatorSet((List *)ruleValList, bldCfgRenderComparatorSize);
    else if (strEq(optType, OPT_TYPE_TIME_STR))
        lstComparatorSet((List *)ruleValList, bldCfgRenderComparatorTime);

    strLstSort(ruleValList, sortOrderAsc);

    // Render value macro
    strCatFmt(
        result,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Rule %ss\n"
        COMMENT_BLOCK_END "\n",
        caption);

    strCatFmt(
        result, "%s\n",
        strZ(
            bldDefineRender(
                strNewFmt("PARSE_RULE_VAL_%s(value)", macro),
                strNewFmt(
                    "PARSE_RULE_U32_%zu(parseRuleVal%s##value)", bldCfgRenderVar128Size(strLstSize(ruleValList) - 1), abbr))));

    // Render values
    strCatFmt(
        result,
        "\n"
        "static const %s parseRuleValue%s[] =\n"
        "{\n",
        type, abbr);

    String *const resultVal = strNew();

    for (unsigned int ruleValIdx = 0; ruleValIdx < strLstSize(ruleValList); ruleValIdx++)
    {
        bldCfgRenderLf(resultVal, ruleValIdx != 0);

        if (strEq(optType, OPT_TYPE_STRING_STR))
            strCatFmt(resultVal, "    PARSE_RULE_STRPUB(%s),", strZ(strLstGet(ruleValList, ruleValIdx)));
        else if (strEq(optType, OPT_TYPE_STRING_ID_STR))
            strCatFmt(resultVal, "    %s,", strZ(bldStrId(strZ(strLstGet(ruleValList, ruleValIdx)))));
        else if (strEq(optType, OPT_TYPE_INTEGER_STR))
            strCatFmt(resultVal, "    %s,", strZ(strTrim(strLstGet(ruleValList, ruleValIdx))));
        else if (strEq(optType, OPT_TYPE_SIZE_STR))
            strCatFmt(resultVal, "    %" PRId64 ",", cfgParseSize(strLstGet(ruleValList, ruleValIdx)));
        else
            strCatFmt(resultVal, "    %" PRId64 ",", cfgParseTime(strLstGet(ruleValList, ruleValIdx)));
    }

    strCat(result, bldCfgRenderLabel(resultVal, label, STR(comment)));
    strCatZ(result, "\n};\n");

    // Render value to string map
    if (strEq(optType, OPT_TYPE_STRING_ID_STR) || strEq(optType, OPT_TYPE_INTEGER_STR) || strEq(optType, OPT_TYPE_SIZE_STR) ||
        strEq(optType, OPT_TYPE_TIME_STR))
    {
        strCatFmt(
            result,
            "\n"
            "static const uint8_t parseRuleValue%sStrMap[] =\n"
            "{\n",
            abbr);

        String *const resultMap = strNew();

        for (unsigned int ruleValIdx = 0; ruleValIdx < strLstSize(ruleValList); ruleValIdx++)
        {
            const Variant *const ruleStrId = kvGet(ruleValMap, VARSTR(strLstGet(ruleValList, ruleValIdx)));
            ASSERT(ruleStrId != NULL);

            bldCfgRenderLf(resultMap, ruleValIdx != 0);
            strCatFmt(resultMap, "    %s,", zNewFmt("parseRuleValStr%s", strZ(bldCfgRenderEnumStr(varStr(ruleStrId)))));
        }

        strCat(result, bldCfgRenderLabel(resultMap, label, strNewFmt("%s/strmap", comment)));
        strCatZ(result, "\n};\n");
    }

    // Render value enum
    strCatZ(
        result,
        "\n"
        "typedef enum\n"
        "{\n");

    String *const resultEnum = strNew();

    for (unsigned int ruleValIdx = 0; ruleValIdx < strLstSize(ruleValList); ruleValIdx++)
    {
        bldCfgRenderLf(resultEnum, ruleValIdx != 0);
        strCatFmt(resultEnum, "    parseRuleVal%s", abbr);

        if (strEq(optType, OPT_TYPE_STRING_STR) || strEq(optType, OPT_TYPE_INTEGER_STR))
            strCatFmt(resultEnum, "%s,", strZ(bldCfgRenderEnumStr(strLstGet(ruleValList, ruleValIdx))));
        else
            strCatFmt(resultEnum, "%s,", strZ(bldEnum("", strLstGet(ruleValList, ruleValIdx))));
    }

    strCat(result, bldCfgRenderLabel(resultEnum, label, strNewFmt("%s/enum", comment)));
    strCatFmt(result, "\n} ParseRuleValue%s;\n", abbr);

    return result;
}

static void
bldCfgRenderParseAutoC(const Storage *const storageRepo, const BldCfg bldCfg, const bool label)
{
    String *const config = strNew();
    KeyValue *const ruleValMap = kvNew();

    // Command parse rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
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
                strNewFmt("PARSE_RULE_U32_%zu(cfgCmd##value)", bldCfgRenderVar128Size(lstSize(bldCfg.cmdList) - 1)))));

    strCatZ(
        config,
        "\n"
        "static const ParseRuleCommand parseRuleCommand[CFG_COMMAND_TOTAL] =\n"
        "{\n");

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);
        String *const configCmd = strNew();

        if (cmdIdx != 0)
            strCatZ(config, COMMENT_SEPARATOR "\n");

        strCatFmt(
            configCmd,
            "    PARSE_RULE_COMMAND\n"
            "    (\n"
            "        PARSE_RULE_COMMAND_NAME(\"%s\"),\n",
            strZ(cmd->name));

        if (cmd->lockRequired)
            strCatZ(configCmd, "        PARSE_RULE_COMMAND_LOCK_REQUIRED(true),\n");

        if (cmd->lockRemoteRequired)
            strCatZ(configCmd, "        PARSE_RULE_COMMAND_LOCK_REMOTE_REQUIRED(true),\n");

        strCatFmt(configCmd, "        PARSE_RULE_COMMAND_LOCK_TYPE(%s),\n", strZ(bldEnum("", cmd->lockType)));

        if (cmd->logFile)
            strCatZ(configCmd, "        PARSE_RULE_COMMAND_LOG_FILE(true),\n");

        strCatFmt(
            configCmd, "        PARSE_RULE_COMMAND_LOG_LEVEL_DEFAULT(%s),\n", strZ(bldEnum("", cmd->logLevelDefault)));

        if (cmd->parameterAllowed)
            strCatZ(configCmd, "        PARSE_RULE_COMMAND_PARAMETER_ALLOWED(true),\n");

        strCatZ(
            configCmd,
            "\n"
            "        PARSE_RULE_COMMAND_ROLE_VALID_LIST\n"
            "        (\n");

        for (unsigned int cmdRoleIdx = 0; cmdRoleIdx < strLstSize(cmd->roleList); cmdRoleIdx++)
        {
            strCatFmt(
                configCmd,
                "            PARSE_RULE_COMMAND_ROLE(%s)\n",
                strZ(bldEnum("", strLstGet(cmd->roleList, cmdRoleIdx))));
        }

        strCatZ(
            configCmd,
            "        ),\n"
            "    ),");

        strCat(config, bldCfgRenderLabel(configCmd, label, strNewFmt("cmd/%s", strZ(cmd->name))));
        strCatChr(config, '\n');
    }

    strCatZ(
        config,
        "};\n");

    // Option group rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
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
        String *const configOptGrp = strNew();

        if (optGrpIdx != 0)
            strCatZ(config, COMMENT_SEPARATOR "\n");

        strCatFmt(
            configOptGrp,
            "    PARSE_RULE_OPTION_GROUP\n"
            "    (\n"
            "        PARSE_RULE_OPTION_GROUP_NAME(\"%s\"),\n"
            "    ),",
            strZ(optGrp->name));

        strCat(config, bldCfgRenderLabel(configOptGrp, label, strNewFmt("opt-grp/%s", strZ(optGrp->name))));
        strCatChr(config, '\n');
    }

    strCatZ(
        config,
        "};\n");

    // Option rules
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
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
                strNewFmt("PARSE_RULE_U32_%zu(cfgOpt##value)", bldCfgRenderVar128Size(lstSize(bldCfg.optList) - 1)))));

    strCatZ(
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
        String *const configOpt = strNew();

        if (optIdx != 0)
            strCatZ(config, COMMENT_SEPARATOR "\n");

        strCatFmt(
            configOpt,
            "    PARSE_RULE_OPTION\n"
            "    (\n"
            "        PARSE_RULE_OPTION_NAME(\"%s\"),\n"
            "        PARSE_RULE_OPTION_TYPE(%s),\n",
            strZ(opt->name), strZ(bldEnum("", opt->type)));

        if (opt->boolLike)
            strCatZ(configOpt, "        PARSE_RULE_OPTION_BOOL_LIKE(true),\n");

        if (opt->beta)
            strCatZ(configOpt, "        PARSE_RULE_OPTION_BETA(true),\n");

        if (opt->negate)
            strCatZ(configOpt, "        PARSE_RULE_OPTION_NEGATE(true),\n");

        if (opt->reset)
            strCatZ(configOpt, "        PARSE_RULE_OPTION_RESET(true),\n");

        strCatFmt(
            configOpt,
            "        PARSE_RULE_OPTION_REQUIRED(%s),\n"
            "        PARSE_RULE_OPTION_SECTION(%s),\n",
            cvtBoolToConstZ(opt->required), strZ(bldEnum("", opt->section)));

        if (opt->secure)
            strCatZ(configOpt, "        PARSE_RULE_OPTION_SECURE(true),\n");

        if (strEq(opt->type, OPT_TYPE_HASH_STR) || strEq(opt->type, OPT_TYPE_LIST_STR))
            strCatZ(configOpt, "        PARSE_RULE_OPTION_MULTI(true),\n");

        if (opt->group != NULL)
        {
            strCatFmt(
                configOpt,
                "        PARSE_RULE_OPTION_GROUP_ID(%s),\n",
                strZ(bldEnum("", opt->group)));
        }

        // If an unindexed deprecation matches the base option name of an indexed option
        if (opt->deprecateList != NULL)
        {
            for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(opt->deprecateList); deprecateIdx++)
            {
                const BldCfgOptionDeprecate *const deprecate = lstGet(opt->deprecateList, deprecateIdx);

                if (strEq(deprecate->name, opt->name) && deprecate->unindexed)
                {
                    strCatZ(configOpt, "        PARSE_RULE_OPTION_DEPRECATE_MATCH(true),\n");
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
                        "            PARSE_RULE_OPTION_COMMAND(%s)\n", strZ(bldEnum("", optCmd->name)));
                }
            }

            if (!strEmpty(configRole))
            {
                strCatFmt(
                    configOpt,
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

            bldCfgRenderValueAdd(opt->type, false, opt->allowRangeMin, ruleValMap);
            bldCfgRenderValueAdd(opt->type, false, opt->allowRangeMax, ruleValMap);
        }

        if (opt->allowList != NULL)
        {
            kvAdd(optionalDefaultRule, ruleAllowList, VARSTR(bldCfgRenderAllowList(opt->allowList, opt->type)));

            for (unsigned int allowIdx = 0; allowIdx < lstSize(opt->allowList); allowIdx++)
            {
                bldCfgRenderValueAdd(
                    opt->type, false, ((const BldCfgOptionValue *)lstGet(opt->allowList, allowIdx))->value, ruleValMap);
            }
        }

        if (opt->defaultValue != NULL)
        {
            kvAdd(optionalDefaultRule, ruleDefault, VARSTR(bldCfgRenderDefault(opt->defaultValue, opt->defaultLiteral, opt->type)));

            if (!strEq(opt->type, OPT_TYPE_BOOLEAN_STR))
                bldCfgRenderValueAdd(opt->type, opt->defaultLiteral, opt->defaultValue, ruleValMap);
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

                for (unsigned int allowIdx = 0; allowIdx < lstSize(optCmd->allowList); allowIdx++)
                {
                    bldCfgRenderValueAdd(
                        opt->type, false, ((const BldCfgOptionValue *)lstGet(optCmd->allowList, allowIdx))->value, ruleValMap);
                }
            }

            // Defaults
            if (optCmd->defaultValue != NULL)
            {
                kvAdd(
                    optionalCmdRuleType, ruleDefault,
                    VARSTR(bldCfgRenderDefault(optCmd->defaultValue, opt->defaultLiteral, opt->type)));

                if (!strEq(opt->type, OPT_TYPE_BOOLEAN_STR))
                    bldCfgRenderValueAdd(opt->type, opt->defaultLiteral, optCmd->defaultValue, ruleValMap);
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
                for (unsigned int ruleIdx = 0; ruleIdx < LENGTH_OF(ruleList); ruleIdx++)
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
                configOpt,
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
                        strCatChr(configOpt, '\n');

                    strCatZ(
                        configOpt,
                        "            PARSE_RULE_OPTIONAL_GROUP\n"
                        "            (\n"
                        "                PARSE_RULE_FILTER_CMD\n"
                        "                (\n");

                    for (unsigned int cmdIdx = 0; cmdIdx < varLstSize(cmdList); cmdIdx++)
                    {
                        strCatFmt(
                            configOpt,
                            "                    PARSE_RULE_VAL_CMD(%s),\n",
                            strZ(bldEnum("", varStr(varLstGet(cmdList, cmdIdx)))));
                    }

                    strCatFmt(
                        configOpt,
                        "                ),\n"
                        "%s"
                        "            ),\n",
                        strZ(varStr(group)));
                }
            }

            if (optionalDefaultRuleSize != 0)
            {
                if (optionalCmdRuleSize != 0)
                    strCatChr(configOpt, '\n');

                strCatZ(
                    configOpt,
                    "            PARSE_RULE_OPTIONAL_GROUP\n"
                    "            (\n");

                for (unsigned int ruleIdx = 0; ruleIdx < optionalDefaultRuleSize; ruleIdx++)
                {
                    const Variant *const key = varLstGet(kvKeyList(optionalDefaultRule), ruleIdx);

                    if (ruleIdx != 0)
                        strCatChr(configOpt, '\n');

                    strCatFmt(
                        configOpt,
                        "%s,\n",
                        strZ(varStr(kvGet(optionalDefaultRule, key))));
                }

                strCatZ(configOpt, "            ),\n");
            }

            strCatZ(configOpt, "        ),\n");
        }

        strCatZ(configOpt, "    ),");

        // Add option to config
        strCat(config, bldCfgRenderLabel(configOpt, label, strNewFmt("opt/%s", strZ(opt->name))));
        strCatChr(config, '\n');
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
                const BldCfgRenderOptionDeprecate bldCfgRenderOptionDeprecate =
                {
                    .name = deprecate->name,
                    .option = opt,
                    .indexed = deprecate->indexed,
                    .unindexed = deprecate->unindexed,
                };

                lstAdd(deprecateCombineList, &bldCfgRenderOptionDeprecate);
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
        String *const configOptDep = strNew();

        if (deprecateIdx != 0)
            strCatZ(config, COMMENT_SEPARATOR "\n");

        strCatFmt(
            configOptDep,
            "    {\n"
            "        .name = \"%s\",\n"
            "        .id = %s,\n",
            strZ(deprecate->name), strZ(bldEnum("cfgOpt", deprecate->option->name)));

        if (deprecate->indexed)
            strCatZ(configOptDep, "        .indexed = true,\n");

        if (deprecate->unindexed)
            strCatZ(configOptDep, "        .unindexed = true,\n");

        strCatZ(configOptDep, "    },");

        strCat(config, bldCfgRenderLabel(configOptDep, label, strNewFmt("opt-deprecate/%s", strZ(deprecate->option->name))));
        strCatChr(config, '\n');
    }

    strCatZ(config, "};\n");

    // Order for option parse resolution
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Order for option parse resolution\n"
        COMMENT_BLOCK_END "\n"
        "static const uint8_t optionResolveOrder[] =\n"
        "{\n");

    // Render resolve order
    String *const configResolve = strNew();

    for (unsigned int optResolveIdx = 0; optResolveIdx < lstSize(bldCfg.optResolveList); optResolveIdx++)
    {
        bldCfgRenderLf(configResolve, optResolveIdx != 0);

        strCatFmt(
            configResolve, "    %s,",
            strZ(bldEnum("cfgOpt", (*(BldCfgOption **)lstGet(bldCfg.optResolveList, optResolveIdx))->name)));
    }

    strCat(config, bldCfgRenderLabel(configResolve, label, STRDEF("opt-resolve-order")));
    strCatZ(config, "\n};\n");

    // Rule values
    // -----------------------------------------------------------------------------------------------------------------------------
    String *const configVal = strNew();

    strCat(
        configVal,
        bldCfgRenderValueRender(OPT_TYPE_STRING_STR, ruleValMap, label, "StringPubConst", "String", "Str", "STR", "val/str"));
    strCat(
        configVal,
        bldCfgRenderValueRender(OPT_TYPE_STRING_ID_STR, ruleValMap, label, "StringId", "StringId", "StrId", "STRID", "val/strid"));
    strCat(configVal, bldCfgRenderValueRender(OPT_TYPE_INTEGER_STR, ruleValMap, label, "int", "Int", "Int", "INT", "val/int"));
    strCat(configVal, bldCfgRenderValueRender(OPT_TYPE_SIZE_STR, ruleValMap, label, "int64_t", "Size", "Size", "SIZE", "val/size"));
    strCat(
        configVal,
        bldCfgRenderValueRender(OPT_TYPE_TIME_STR, ruleValMap, label, "unsigned int", "Time", "Time", "TIME", "val/time"));

    // Write to storage
    // -----------------------------------------------------------------------------------------------------------------------------
    bldPut(
        storageRepo, "src/config/parse.auto.c.inc",
        BUFSTR(strNewFmt("%s%s%s", strZ(bldHeader(CONFIG_MODULE, PARSE_AUTO_COMMENT)), strZ(configVal), strZ(config))));
}

/**********************************************************************************************************************************/
void
bldCfgRender(const Storage *const storageRepo, const BldCfg bldCfg, const bool label)
{
    bldCfgRenderConfigAutoH(storageRepo, bldCfg);
    bldCfgRenderParseAutoC(storageRepo, bldCfg, label);
}
