/***********************************************************************************************************************************
Parse Configuration Yaml
***********************************************************************************************************************************/
#include "build.auto.h"

#include <yaml.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "build/config/parse.h"

/**********************************************************************************************************************************/
// Helper to parse allow list
static const StringList *
bldCfgParseAllowList(Yaml *const yaml, const List *const optList)
{
    YamlEvent allowListVal = yamlEventNext(yaml);

    // If allow list is defined
    if (allowListVal.type == yamlEventTypeSeqBegin)
    {
        YamlEvent allowListVal = yamlEventNext(yaml);
        StringList *result = strLstNew();

        do
        {
            yamlEventCheck(allowListVal, yamlEventTypeScalar);
            strLstAdd(result, allowListVal.value);

            allowListVal = yamlEventNext(yaml);
        }
        while (allowListVal.type != yamlEventTypeSeqEnd);

        strLstSort(result, sortOrderAsc);

        return result;
    }

    // Else allow list is inherited
    CHECK(optList != NULL);
    yamlEventCheck(allowListVal, yamlEventTypeScalar);

    const BldCfgOption *const optInherit = lstFind(optList, &allowListVal.value);
    CHECK(optInherit != NULL);

    return optInherit->allowList;
}

// Helper to parse allow range
static void
bldCfgParseAllowRange(Yaml *const yaml)
{
    yamlEventNextCheck(yaml, yamlEventTypeSeqBegin);

    YamlEvent allowRangeVal = yamlEventNext(yaml);

    do
    {
        yamlEventCheck(allowRangeVal, yamlEventTypeScalar);

        allowRangeVal = yamlEventNext(yaml);
    }
    while (allowRangeVal.type != yamlEventTypeSeqEnd);
}

// Helper to parse command roles
static void
bldCfgParseCommandRole(Yaml *const yaml)
{
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    YamlEvent commandRoleVal = yamlEventNext(yaml);

    if (commandRoleVal.type != yamlEventTypeMapEnd)
    {
        do
        {
            yamlEventCheck(commandRoleVal, yamlEventTypeScalar);
            yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
            yamlEventNextCheck(yaml, yamlEventTypeMapEnd);

            commandRoleVal = yamlEventNext(yaml);
        }
        while (commandRoleVal.type != yamlEventTypeMapEnd);
    }
}

// Helper to parse depend
static void
bldCfgParseDepend(Yaml *const yaml)
{
    YamlEvent dependVal = yamlEventNext(yaml);

    if (dependVal.type == yamlEventTypeMapBegin)
    {
        YamlEvent dependDef = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(dependDef, yamlEventTypeScalar);

            if (strEqZ(dependDef.value, "list"))
            {
                yamlEventNextCheck(yaml, yamlEventTypeSeqBegin);

                YamlEvent dependDefVal = yamlEventNext(yaml);

                do
                {
                    yamlEventCheck(dependDefVal, yamlEventTypeScalar);
                    dependDefVal = yamlEventNext(yaml);
                }
                while (dependDefVal.type != yamlEventTypeSeqEnd);
            }
            else
            {
                YamlEvent dependDefVal = yamlEventNext(yaml);
                yamlEventCheck(dependDefVal, yamlEventTypeScalar);

                if (strEqZ(dependDef.value, "option"))
                {
                }
                else
                    THROW_FMT(FormatError, "unknown depend definition '%s'", strZ(dependDef.value));
            }

            dependDef = yamlEventNext(yaml);
        }
        while (dependDef.type != yamlEventTypeMapEnd);
    }
    else
        yamlEventCheck(dependVal, yamlEventTypeScalar);
}

// Helper to parse deprecate
static void
bldCfgParseOptionDeprecate(Yaml *const yaml)
{
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    YamlEvent optDeprecate = yamlEventNext(yaml);

    do
    {
        yamlEventCheck(optDeprecate, yamlEventTypeScalar);
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent optDeprecateDef = yamlEventNext(yaml);

        if (optDeprecateDef.type == yamlEventTypeScalar)
        {
            do
            {
                yamlEventCheck(optDeprecateDef, yamlEventTypeScalar);
                yamlEventNextCheck(yaml, yamlEventTypeScalar);

                if (strEqZ(optDeprecateDef.value, "index"))
                {
                }
                else if (strEqZ(optDeprecateDef.value, "reset"))
                {
                }
                else
                    THROW_FMT(FormatError, "unknown deprecate definition '%s'", strZ(optDeprecateDef.value));

                optDeprecateDef = yamlEventNext(yaml);
            }
            while (optDeprecateDef.type != yamlEventTypeMapEnd);
        }
        else
            yamlEventCheck(optDeprecateDef, yamlEventTypeMapEnd);

        optDeprecate = yamlEventNext(yaml);
    }
    while (optDeprecate.type != yamlEventTypeMapEnd);
}

// Helper to parse commands
static const List *
bldCfgParseOptionCommand(Yaml *const yaml, const List *const optList)
{
    YamlEvent optCmdVal = yamlEventNext(yaml);

    // If command list is defined
    if (optCmdVal.type == yamlEventTypeMapBegin)
    {
        List *result = lstNewP(sizeof(BldCfgOptionCommand), .comparator = lstComparatorStr);

        YamlEvent optCmd = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(optCmd, yamlEventTypeScalar);
            BldCfgOptionCommand optCmdData = {.name = optCmd.value};

            yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
            YamlEvent optCmdDef = yamlEventNext(yaml);

            if (optCmdDef.type == yamlEventTypeScalar)
            {
                do
                {
                    yamlEventCheck(optCmdDef, yamlEventTypeScalar);

                    if (strEqZ(optCmdDef.value, "allow-list"))
                    {
                        optCmdData.allowList = bldCfgParseAllowList(yaml, NULL);
                    }
                    else if (strEqZ(optCmdDef.value, "command-role"))
                    {
                        bldCfgParseCommandRole(yaml);
                    }
                    else if (strEqZ(optCmdDef.value, "depend"))
                    {
                        bldCfgParseDepend(yaml);
                    }
                    else
                    {
                        yamlEventNextCheck(yaml, yamlEventTypeScalar);

                        if (strEqZ(optCmdDef.value, "default"))
                        {
                        }
                        else if (strEqZ(optCmdDef.value, "internal"))
                        {
                        }
                        else if (strEqZ(optCmdDef.value, "required"))
                        {
                        }
                        else
                            THROW_FMT(FormatError, "unknown option command definition '%s'", strZ(optCmdDef.value));
                    }

                    optCmdDef = yamlEventNext(yaml);
                }
                while (optCmdDef.type != yamlEventTypeMapEnd);
            }
            else
                yamlEventCheck(optCmdDef, yamlEventTypeMapEnd);

            lstAdd(result, &optCmdData);

            optCmd = yamlEventNext(yaml);
        }
        while (optCmd.type != yamlEventTypeMapEnd);

        lstSort(result, sortOrderAsc);

        return result;
    }

    // Else command list is inherited
    CHECK(optList != NULL);
    yamlEventCheck(optCmdVal, yamlEventTypeScalar);

    const BldCfgOption *const optInherit = lstFind(optList, &optCmdVal.value);
    CHECK(optInherit != NULL);

    return optInherit->cmdList;
}

BldCfg
bldCfgParse(const Storage *const storageRepo)
{
    BldCfg result =
    {
        .commandList = lstNewP(sizeof(BldCfgCommand), .comparator = lstComparatorStr),
        .optGrpList = lstNewP(sizeof(BldCfgOptionGroup), .comparator = lstComparatorStr),
        .optList = lstNewP(sizeof(BldCfgOption), .comparator = lstComparatorStr),
    };

    // Initialize yaml
    Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/config/config.yaml"))));
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    // Parse commands
    // -----------------------------------------------------------------------------------------------------------------------------
    yamlEventNextCheck(yaml, yamlEventTypeScalar);
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    YamlEvent cmd = yamlEventNext(yaml);

    do
    {
        yamlEventCheck(cmd, yamlEventTypeScalar);

        BldCfgCommand cmdData =
        {
            .name = cmd.value,
            .logFile = true,
            .logLevelDefault = strNewZ("info"),
            .lockType = strNewZ("none"),
        };

        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent cmdDef = yamlEventNext(yaml);

        if (cmdDef.type == yamlEventTypeScalar)
        {
            do
            {
                yamlEventCheck(cmdDef, yamlEventTypeScalar);

                if (strEqZ(cmdDef.value, "command-role"))
                {
                    bldCfgParseCommandRole(yaml);
                }
                else
                {
                    YamlEvent cmdDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                    if (strEqZ(cmdDef.value, "internal"))
                    {
                    }
                    else if (strEqZ(cmdDef.value, "lock-type"))
                    {
                        cmdData.lockType = cmdDefVal.value;
                    }
                    else if (strEqZ(cmdDef.value, "lock-remote-required"))
                    {
                        cmdData.lockRemoteRequired = yamlBoolParse(cmdDefVal);
                    }
                    else if (strEqZ(cmdDef.value, "lock-required"))
                    {
                        cmdData.lockRequired = yamlBoolParse(cmdDefVal);
                    }
                    else if (strEqZ(cmdDef.value, "log-file"))
                    {
                        cmdData.logFile = yamlBoolParse(cmdDefVal);
                    }
                    else if (strEqZ(cmdDef.value, "log-level-default"))
                    {
                        cmdData.logLevelDefault = strLower(strDup(cmdDefVal.value));
                    }
                    else if (strEqZ(cmdDef.value, "parameter-allowed"))
                    {
                    }
                    else
                        THROW_FMT(FormatError, "unknown command definition '%s'", strZ(cmdDef.value));
                }

                cmdDef = yamlEventNext(yaml);
            }
            while (cmdDef.type != yamlEventTypeMapEnd);
        }
        else
            yamlEventCheck(cmdDef, yamlEventTypeMapEnd);

        lstAdd(result.commandList, &cmdData);

        cmd = yamlEventNext(yaml);
    }
    while (cmd.type != yamlEventTypeMapEnd);

    lstSort(result.commandList, sortOrderAsc);

    // Parse option groups
    // -----------------------------------------------------------------------------------------------------------------------------
    yamlEventNextCheck(yaml, yamlEventTypeScalar);
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    YamlEvent optGrp = yamlEventNext(yaml);

    do
    {
        yamlEventCheck(optGrp, yamlEventTypeScalar);
        BldCfgOptionGroup optGrpData = {.name = optGrp.value};

        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent optGrpDef = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(optGrpDef, yamlEventTypeScalar);
            yamlEventNextCheck(yaml, yamlEventTypeScalar);

            if (strEqZ(optGrpDef.value, "indexTotal"))
            {
            }
            else if (strEqZ(optGrpDef.value, "prefix"))
            {
            }
            else
                THROW_FMT(FormatError, "unknown option group definition '%s'", strZ(optGrpDef.value));

            optGrpDef = yamlEventNext(yaml);
        }
        while (optGrpDef.type != yamlEventTypeMapEnd);

        lstAdd(result.optGrpList, &optGrpData);

        optGrp = yamlEventNext(yaml);
    }
    while (optGrp.type != yamlEventTypeMapEnd);

    lstSort(result.optGrpList, sortOrderAsc);

    // Parse options
    // -----------------------------------------------------------------------------------------------------------------------------
    yamlEventNextCheck(yaml, yamlEventTypeScalar);
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    YamlEvent opt = yamlEventNext(yaml);

    do
    {
        yamlEventCheck(opt, yamlEventTypeScalar);
        BldCfgOption optData = {.name = opt.value};

        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent optDef = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(optDef, yamlEventTypeScalar);

            if (strEqZ(optDef.value, "allow-list"))
            {
                optData.allowList = bldCfgParseAllowList(yaml, result.optList);
            }
            else if (strEqZ(optDef.value, "allow-range"))
            {
                bldCfgParseAllowRange(yaml);
            }
            else if (strEqZ(optDef.value, "command"))
            {
                optData.cmdList = bldCfgParseOptionCommand(yaml, result.optList);
            }
            else if (strEqZ(optDef.value, "command-role"))
            {
                bldCfgParseCommandRole(yaml);
            }
            else if (strEqZ(optDef.value, "depend"))
            {
                bldCfgParseDepend(yaml);
            }
            else if (strEqZ(optDef.value, "deprecate"))
            {
                bldCfgParseOptionDeprecate(yaml);
            }
            else
            {
                YamlEvent optDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                if (strEqZ(optDef.value, "default"))
                {
                }
                else if (strEqZ(optDef.value, "default-literal"))
                {
                }
                else if (strEqZ(optDef.value, "group"))
                {
                    optData.group = optDefVal.value;
                }
                else if (strEqZ(optDef.value, "inherit"))
                {
                    const BldCfgOption *const optInherit = lstFind(result.optList, &optDefVal.value);
                    CHECK(optInherit != NULL);

                    optData = *optInherit;
                    optData.name = opt.value;
                }
                else if (strEqZ(optDef.value, "internal"))
                {
                }
                else if (strEqZ(optDef.value, "negate"))
                {
                }
                else if (strEqZ(optDef.value, "required"))
                {
                }
                else if (strEqZ(optDef.value, "section"))
                {
                }
                else if (strEqZ(optDef.value, "secure"))
                {
                }
                else if (strEqZ(optDef.value, "type"))
                {
                    optData.type = optDefVal.value;
                }
                else
                    THROW_FMT(FormatError, "unknown option definition '%s'", strZ(optDef.value));
            }

            optDef = yamlEventNext(yaml);
        }
        while (optDef.type != yamlEventTypeMapEnd);

        lstAdd(result.optList, &optData);

        opt = yamlEventNext(yaml);
    }
    while (opt.type != yamlEventTypeMapEnd);

    lstSort(result.optList, sortOrderAsc);

    return result;
}
