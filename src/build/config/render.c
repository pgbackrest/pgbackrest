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

/**********************************************************************************************************************************/
void
bldCfgRender(const Storage *const storageRepo, const BldCfg bldCfg)
{
    // Build Header
    // -----------------------------------------------------------------------------------------------------------------------------
    const String *const header = bldHeader("config", "Command and Option Configuration");
    String *config = strNewFmt(
        "%s"
        "#ifndef CONFIG_CONFIG_AUTO_H\n"
        "#define CONFIG_CONFIG_AUTO_H\n",
        strZ(header));

    // Command constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command constants\n"
        COMMENT_BLOCK_END "\n");

    unsigned int cmdTotal = 0;

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.commandList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.commandList, cmdIdx);

        strCatFmt(config, "%s\n", strZ(bldDefineRender(bldConst("CFGCMD", cmd->name), strNewFmt("\"%s\"", strZ(cmd->name)))));

        cmdTotal++;
    }

    strCatFmt(config, "\n%s\n", strZ(bldDefineRender(STRDEF("CFG_COMMAND_TOTAL"), strNewFmt("%u", cmdTotal))));

    // Option group constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option group constants\n"
        COMMENT_BLOCK_END "\n");

    unsigned int optGrpTotal = 0;

    for (unsigned int optGrpIdx = 0; optGrpIdx < lstSize(bldCfg.optGrpList); optGrpIdx++)
        optGrpTotal++;

    strCatFmt(config, "%s\n", strZ(bldDefineRender(STRDEF("CFG_OPTION_GROUP_TOTAL"), strNewFmt("%u", optGrpTotal))));

    // Option constants
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatZ(
        config,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Option constants\n"
        COMMENT_BLOCK_END "\n");

    unsigned int optTotal = 0;

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);

        if (opt->group == NULL)
            strCatFmt(config, "%s\n", strZ(bldDefineRender(bldConst("CFGOPT", opt->name), strNewFmt("\"%s\"", strZ(opt->name)))));

        optTotal++;
    }

    strCatFmt(config, "\n%s\n", strZ(bldDefineRender(STRDEF("CFG_OPTION_TOTAL"), strNewFmt("%u", optTotal))));

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

            if (opt->cmdList != NULL)
            {
                for (unsigned int optCmdListIdx = 0; optCmdListIdx < lstSize(opt->cmdList); optCmdListIdx++)
                {
                    BldCfgOptionCommand *optCmd = lstGet(opt->cmdList, optCmdListIdx);

                    if (optCmd->allowList != NULL)
                    {
                        for (unsigned int allowListIdx = 0; allowListIdx < strLstSize(optCmd->allowList); allowListIdx++)
                            strLstAddIfMissing(allowList, strLstGet(optCmd->allowList, allowListIdx));
                    }
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

                    strCatFmt(
                        config, "%s\n",
                        strZ(
                            bldDefineRender(
                                strUpper(
                                    strReplaceChr(strNewFmt("CFGOPTVAL_%s_%s_Z", strZ(opt->name), strZ(allowListItem)), '-', '_')),
                                strNewFmt("\"%s\"", strZ(allowListItem)))));
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

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.commandList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.commandList, cmdIdx);

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

    // Build C
    // -----------------------------------------------------------------------------------------------------------------------------
    config = strNewFmt(
        "%s"
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "Command data\n"
        COMMENT_BLOCK_END "\n"
        "static const ConfigCommandData configCommandData[CFG_COMMAND_TOTAL] = CONFIG_COMMAND_LIST\n"
        "(\n",
        strZ(header));

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.commandList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.commandList, cmdIdx);

        if (cmdIdx != 0)
            strCatZ(config, "\n");

        strCatFmt(
            config,
            "    CONFIG_COMMAND\n"
            "    (\n"
            "        CONFIG_COMMAND_NAME(%s)\n"
            "\n"
            "        CONFIG_COMMAND_LOG_FILE(%s)\n"
            "        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(%s)\n"
            "        CONFIG_COMMAND_LOCK_REQUIRED(%s)\n"
            "        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(%s)\n"
            "        CONFIG_COMMAND_LOCK_TYPE(%s)\n"
            "    )\n",
            strZ(bldConst("CFGCMD", cmd->name)), cvtBoolToConstZ(cmd->logFile), strZ(bldEnum("logLevel", cmd->logLevelDefault)),
            cvtBoolToConstZ(cmd->lockRequired), cvtBoolToConstZ(cmd->lockRemoteRequired), strZ(bldEnum("lockType", cmd->lockType)));
    }

    strCatZ(
        config,
        ")\n");

    bldPut(storageRepo, "src/config/config.auto.c", BUFSTR(config));
}
