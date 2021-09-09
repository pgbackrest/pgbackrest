/***********************************************************************************************************************************
Parse Configuration Yaml
***********************************************************************************************************************************/
#include "build.auto.h"

#include <yaml.h>

#include "common/log.h"
#include "common/type/convert.h"
#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "build/config/parse.h"

/***********************************************************************************************************************************
Command role constants
***********************************************************************************************************************************/
STRING_EXTERN(CMD_ROLE_ASYNC_STR,                                   CMD_ROLE_ASYNC);
STRING_EXTERN(CMD_ROLE_LOCAL_STR,                                   CMD_ROLE_LOCAL);
STRING_EXTERN(CMD_ROLE_MAIN_STR,                                    CMD_ROLE_MAIN);
STRING_EXTERN(CMD_ROLE_REMOTE_STR,                                  CMD_ROLE_REMOTE);

/***********************************************************************************************************************************
Command constants
***********************************************************************************************************************************/
STRING_EXTERN(CMD_HELP_STR,                                         CMD_HELP);
STRING_EXTERN(CMD_VERSION_STR,                                      CMD_VERSION);

/***********************************************************************************************************************************
Option type constants
***********************************************************************************************************************************/
STRING_EXTERN(OPT_TYPE_BOOLEAN_STR,                                 OPT_TYPE_BOOLEAN);
STRING_EXTERN(OPT_TYPE_HASH_STR,                                    OPT_TYPE_HASH);
STRING_EXTERN(OPT_TYPE_LIST_STR,                                    OPT_TYPE_LIST);
STRING_EXTERN(OPT_TYPE_STRING_STR,                                  OPT_TYPE_STRING);
STRING_EXTERN(OPT_TYPE_TIME_STR,                                    OPT_TYPE_TIME);

/***********************************************************************************************************************************
Option constants
***********************************************************************************************************************************/
STRING_EXTERN(OPT_STANZA_STR,                                       OPT_STANZA);

/***********************************************************************************************************************************
Section constants
***********************************************************************************************************************************/
STRING_EXTERN(SECTION_COMMAND_LINE_STR,                             SECTION_COMMAND_LINE);
STRING_EXTERN(SECTION_GLOBAL_STR,                                   SECTION_GLOBAL);
STRING_EXTERN(SECTION_STANZA_STR,                                   SECTION_STANZA);

/***********************************************************************************************************************************
Parse command list
***********************************************************************************************************************************/
typedef struct BldCfgCommandRaw
{
    const String *const name;                                       // See BldCfgCommand for comments
    bool internal;
    bool logFile;
    const String *logLevelDefault;
    bool lockRequired;
    bool lockRemoteRequired;
    const String *lockType;
    bool parameterAllowed;
    StringList *roleList;
} BldCfgCommandRaw;

// Helper to parse command roles
static StringList *
bldCfgParseCommandRole(Yaml *const yaml)
{
    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
        YamlEvent commandRoleVal = yamlEventNext(yaml);

        if (commandRoleVal.type != yamlEventTypeMapEnd)
        {
            do
            {
                yamlEventCheck(commandRoleVal, yamlEventTypeScalar);

                strLstAdd(result, commandRoleVal.value);

                yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
                yamlEventNextCheck(yaml, yamlEventTypeMapEnd);

                commandRoleVal = yamlEventNext(yaml);
            }
            while (commandRoleVal.type != yamlEventTypeMapEnd);
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

static List *
bldCfgParseCommandList(Yaml *const yaml)
{
    List *const result = lstNewP(sizeof(BldCfgCommand), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeScalar);
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent cmd = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(cmd, yamlEventTypeScalar);

            BldCfgCommandRaw cmdRaw =
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
                        cmdRaw.roleList = bldCfgParseCommandRole(yaml);
                    }
                    else
                    {
                        YamlEvent cmdDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                        if (strEqZ(cmdDef.value, "internal"))
                        {
                            cmdRaw.internal = yamlBoolParse(cmdDefVal);
                        }
                        else if (strEqZ(cmdDef.value, "lock-type"))
                        {
                            cmdRaw.lockType = cmdDefVal.value;
                        }
                        else if (strEqZ(cmdDef.value, "lock-remote-required"))
                        {
                            cmdRaw.lockRemoteRequired = yamlBoolParse(cmdDefVal);
                        }
                        else if (strEqZ(cmdDef.value, "lock-required"))
                        {
                            cmdRaw.lockRequired = yamlBoolParse(cmdDefVal);
                        }
                        else if (strEqZ(cmdDef.value, "log-file"))
                        {
                            cmdRaw.logFile = yamlBoolParse(cmdDefVal);
                        }
                        else if (strEqZ(cmdDef.value, "log-level-default"))
                        {
                            cmdRaw.logLevelDefault = strLower(strDup(cmdDefVal.value));
                        }
                        else if (strEqZ(cmdDef.value, "parameter-allowed"))
                        {
                            cmdRaw.parameterAllowed = strLower(strDup(cmdDefVal.value));
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

            // Create role list if not defined
            if (cmdRaw.roleList == NULL)
                cmdRaw.roleList = strLstNew();

            // Add main to the role list and resort
            strLstAddIfMissing(cmdRaw.roleList, CMD_ROLE_MAIN_STR);
            strLstSort(cmdRaw.roleList, sortOrderAsc);

            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                lstAdd(
                    result,
                    &(BldCfgCommand)
                    {
                        .name = strDup(cmdRaw.name),
                        .internal = cmdRaw.internal,
                        .logFile = cmdRaw.logFile,
                        .logLevelDefault = strDup(cmdRaw.logLevelDefault),
                        .lockRequired = cmdRaw.lockRequired,
                        .lockRemoteRequired = cmdRaw.lockRemoteRequired,
                        .lockType = strDup(cmdRaw.lockType),
                        .parameterAllowed = cmdRaw.parameterAllowed,
                        .roleList = strLstDup(cmdRaw.roleList),
                    });
            }
            MEM_CONTEXT_END();

            cmd = yamlEventNext(yaml);
        }
        while (cmd.type != yamlEventTypeMapEnd);

        lstSort(result, sortOrderAsc);
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Parse option group list
***********************************************************************************************************************************/
typedef struct BldCfgOptionGroupRaw
{
    const String *const name;                                       // See BldCfgOptionGroup for comments
} BldCfgOptionGroupRaw;

static List *
bldCfgParseOptionGroupList(Yaml *const yaml)
{
    List *const result = lstNewP(sizeof(BldCfgOptionGroup), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeScalar);
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent optGrp = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(optGrp, yamlEventTypeScalar);
            BldCfgOptionGroupRaw optGrpRaw = {.name = optGrp.value};

            yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
            yamlEventNextCheck(yaml, yamlEventTypeMapEnd);

            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                lstAdd(result, &(BldCfgOptionGroup){.name = strDup(optGrpRaw.name)});
            }
            MEM_CONTEXT_END();

            optGrp = yamlEventNext(yaml);
        }
        while (optGrp.type != yamlEventTypeMapEnd);

        lstSort(result, sortOrderAsc);
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Parse option list
***********************************************************************************************************************************/
typedef struct BldCfgOptionDependRaw
{
    const String *option;                                           // See BldCfgOptionDepend for comments
    const StringList *valueList;
} BldCfgOptionDependRaw;

typedef struct BldCfgOptionDeprecateRaw
{
    const String *name;                                             // See BldCfgOptionDeprecate for comments
    bool indexed;
    bool unindexed;
} BldCfgOptionDeprecateRaw;

typedef struct BldCfgOptionCommandRaw
{
    const String *name;                                             // See BldCfgOptionCommand for comments
    const Variant *internal;
    const Variant *required;
    const String *defaultValue;
    const BldCfgOptionDependRaw *depend;
    const StringList *allowList;
    const StringList *roleList;
} BldCfgOptionCommandRaw;

typedef struct BldCfgOptionRaw
{
    const String *name;                                             // See BldCfgOption for comments
    const String *type;
    const String *section;
    bool internal;
    const Variant *required;
    const Variant *negate;
    bool reset;
    const String *defaultValue;
    bool defaultLiteral;
    const String *group;
    bool secure;
    const BldCfgOptionDependRaw *depend;
    const StringList *allowList;
    const String *allowRangeMin;
    const String *allowRangeMax;
    const List *cmdList;
    const StringList *cmdRoleList;
    const List *deprecateList;
} BldCfgOptionRaw;

// Helper to parse allow list
static const StringList *
bldCfgParseAllowList(Yaml *const yaml, const List *const optList)
{
    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        YamlEvent allowListVal = yamlEventNext(yaml);

        // If allow list is defined
        if (allowListVal.type == yamlEventTypeSeqBegin)
        {
            YamlEvent allowListVal = yamlEventNext(yaml);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = strLstNew();
            }
            MEM_CONTEXT_PRIOR_END();

            do
            {
                yamlEventCheck(allowListVal, yamlEventTypeScalar);
                strLstAdd(result, allowListVal.value);

                allowListVal = yamlEventNext(yaml);
            }
            while (allowListVal.type != yamlEventTypeSeqEnd);
        }
        else
        {

            // Else allow list is inherited
            CHECK(optList != NULL);
            yamlEventCheck(allowListVal, yamlEventTypeScalar);

            const BldCfgOptionRaw *const optInherit = lstFind(optList, &allowListVal.value);
            CHECK(optInherit != NULL);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = strLstDup(optInherit->allowList);
            }
            MEM_CONTEXT_PRIOR_END();
        }

        CHECK(result != NULL);
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

// Helper to parse allow range
static void
bldCfgParseAllowRange(Yaml *const yaml, BldCfgOptionRaw *const opt)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeSeqBegin);

        YamlEvent allowRangeMinVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);
        YamlEvent allowRangeMaxVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            opt->allowRangeMin = strDup(allowRangeMinVal.value);
            opt->allowRangeMax = strDup(allowRangeMaxVal.value);
        }
        MEM_CONTEXT_PRIOR_END();

        yamlEventNextCheck(yaml, yamlEventTypeSeqEnd);
    }
    MEM_CONTEXT_TEMP_END();
}

// Helper to parse depend
static const BldCfgOptionDependRaw *
bldCfgParseDepend(Yaml *const yaml, const List *const optList)
{
    const BldCfgOptionDependRaw *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        YamlEvent dependVal = yamlEventNext(yaml);

        if (dependVal.type == yamlEventTypeMapBegin)
        {
            YamlEvent dependDef = yamlEventNext(yaml);
            BldCfgOptionDependRaw optDependRaw = {0};

            do
            {
                yamlEventCheck(dependDef, yamlEventTypeScalar);

                if (strEqZ(dependDef.value, "list"))
                {
                    yamlEventNextCheck(yaml, yamlEventTypeSeqBegin);
                    YamlEvent dependDefVal = yamlEventNext(yaml);

                    StringList *const valueList = strLstNew();

                    do
                    {
                        yamlEventCheck(dependDefVal, yamlEventTypeScalar);

                        strLstAdd(valueList, dependDefVal.value);

                        dependDefVal = yamlEventNext(yaml);
                    }
                    while (dependDefVal.type != yamlEventTypeSeqEnd);

                    optDependRaw.valueList = valueList;
                }
                else
                {
                    YamlEvent dependDefVal = yamlEventNext(yaml);
                    yamlEventCheck(dependDefVal, yamlEventTypeScalar);

                    if (strEqZ(dependDef.value, "option"))
                    {
                        optDependRaw.option = dependDefVal.value;
                    }
                    else
                        THROW_FMT(FormatError, "unknown depend definition '%s'", strZ(dependDef.value));
                }

                dependDef = yamlEventNext(yaml);
            }
            while (dependDef.type != yamlEventTypeMapEnd);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                BldCfgOptionDependRaw *optDepend = memNew(sizeof(BldCfgOptionDependRaw));

                *optDepend = (BldCfgOptionDependRaw)
                {
                    .option = strDup(optDependRaw.option),
                    .valueList = strLstDup(optDependRaw.valueList)
                };

                result = optDepend;
            }
            MEM_CONTEXT_PRIOR_END();
        }
        else
        {
            // Else depend is inherited
            CHECK(optList != NULL);
            yamlEventCheck(dependVal, yamlEventTypeScalar);

            const BldCfgOptionRaw *const optInherit = lstFind(optList, &dependVal.value);

            if (optInherit == NULL)
                THROW_FMT(FormatError, "dependency inherited from option '%s' before it is defined", strZ(dependVal.value));

            result = optInherit->depend;
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;

}

// Helper to reconcile depend
static const BldCfgOptionDepend *
bldCfgParseDependReconcile(const BldCfgOptionDependRaw *const optDependRaw, const List *const optList)
{
    BldCfgOptionDepend *result = NULL;

    if (optDependRaw != NULL)
    {
        const BldCfgOption *const optDepend = lstFind(optList, &optDependRaw->option);

        if (optDepend == NULL)
            THROW_FMT(FormatError, "dependency on undefined option '%s'", strZ(optDependRaw->option));

        result = memNew(sizeof(BldCfgOptionDepend));

        memcpy(
            result,
            &(BldCfgOptionDepend){.option = optDepend, .valueList = strLstDup(optDependRaw->valueList)},
            sizeof(BldCfgOptionDepend));
    }

    return result;
}

// Helper to parse deprecate
static List *
bldCfgParseOptionDeprecate(Yaml *const yaml)
{
    List *result = lstNewP(sizeof(BldCfgOptionCommandRaw), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
        YamlEvent optDeprecate = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(optDeprecate, yamlEventTypeScalar);
            const String *name = optDeprecate.value;
            bool indexed = false;

            yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
            yamlEventNextCheck(yaml, yamlEventTypeMapEnd);

            // Determine if this deprecation is indexed
            const size_t questionPos = (size_t)strChr(name, '?');

            if (questionPos != (size_t)-1)
            {
                name = strNewFmt("%s%s", strZ(strSubN(name, 0, questionPos)), strZ(strSub(name, questionPos + 1)));
                indexed = true;
            }

            // Create final deprecation if it does not aready exist
            BldCfgOptionDeprecateRaw *deprecate = lstFind(result, &name);

            if (deprecate == NULL)
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    lstAdd(result, &(BldCfgOptionDeprecateRaw){.name = strDup(name)});
                }
                MEM_CONTEXT_PRIOR_END();

                deprecate = lstFind(result, &name);
                CHECK(deprecate != NULL);
            }

            // Set indexed/unindexed flags
            if (indexed)
                deprecate->indexed = true;
            else
                deprecate->unindexed = true;

            optDeprecate = yamlEventNext(yaml);
        }
        while (optDeprecate.type != yamlEventTypeMapEnd);

        lstSort(result, sortOrderAsc);
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

// Helper to reconcile deprecate
static List *
bldCfgParseOptionDeprecateReconcile(const List *const optDeprecateRawList)
{
    List *result = NULL;

    if (optDeprecateRawList != NULL)
    {
        result = lstNewP(sizeof(BldCfgOptionDeprecate), .comparator = lstComparatorStr);

        for (unsigned int optDeprecateRawIdx = 0; optDeprecateRawIdx < lstSize(optDeprecateRawList); optDeprecateRawIdx++)
        {
            const BldCfgOptionDeprecateRaw *const optDeprecateRaw = lstGet(optDeprecateRawList, optDeprecateRawIdx);

            lstAdd(
                result,
                &(BldCfgOptionDeprecate)
                {
                    .name = strDup(optDeprecateRaw->name),
                    .indexed = optDeprecateRaw->indexed,
                    .unindexed = optDeprecateRaw->unindexed,
                });
        }
    }

    return result;
}

// Helper to parse the option command list
static const List *
bldCfgParseOptionCommandList(Yaml *const yaml, const List *const optList)
{
    const List *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        YamlEvent optCmdVal = yamlEventNext(yaml);

        // If command list is defined
        if (optCmdVal.type == yamlEventTypeMapBegin)
        {
            List *const optCmdRawList = lstNewP(sizeof(BldCfgOptionCommandRaw), .comparator = lstComparatorStr);

            YamlEvent optCmd = yamlEventNext(yaml);

            do
            {
                yamlEventCheck(optCmd, yamlEventTypeScalar);
                BldCfgOptionCommandRaw optCmdRaw = {.name = optCmd.value};

                yamlEventNextCheck(yaml, yamlEventTypeMapBegin);
                YamlEvent optCmdDef = yamlEventNext(yaml);

                if (optCmdDef.type == yamlEventTypeScalar)
                {
                    do
                    {
                        yamlEventCheck(optCmdDef, yamlEventTypeScalar);

                        if (strEqZ(optCmdDef.value, "allow-list"))
                        {
                            optCmdRaw.allowList = bldCfgParseAllowList(yaml, NULL);
                        }
                        else if (strEqZ(optCmdDef.value, "command-role"))
                        {
                            optCmdRaw.roleList = bldCfgParseCommandRole(yaml);
                        }
                        else if (strEqZ(optCmdDef.value, "depend"))
                        {
                            MEM_CONTEXT_BEGIN(lstMemContext(optCmdRawList))
                            {
                                optCmdRaw.depend = bldCfgParseDepend(yaml, optList);
                            }
                            MEM_CONTEXT_END();
                        }
                        else
                        {
                            YamlEvent optCmdDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                            if (strEqZ(optCmdDef.value, "default"))
                            {
                                // If an override to inheritance
                                if (strEqZ(optCmdDefVal.value, "~"))
                                {
                                    optCmdRaw.defaultValue = NULL;
                                }
                                // Else set the value
                                else
                                    optCmdRaw.defaultValue = optCmdDefVal.value;
                            }
                            else if (strEqZ(optCmdDef.value, "internal"))
                            {
                                optCmdRaw.internal = varNewBool(yamlBoolParse(optCmdDefVal));
                            }
                            else if (strEqZ(optCmdDef.value, "required"))
                            {
                                optCmdRaw.required = varNewBool(yamlBoolParse(optCmdDefVal));
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

                MEM_CONTEXT_BEGIN(lstMemContext(optCmdRawList))
                {
                    lstAdd(
                        optCmdRawList,
                        &(BldCfgOptionCommandRaw)
                        {
                            .name = strDup(optCmdRaw.name),
                            .internal = varDup(optCmdRaw.internal),
                            .required = varDup(optCmdRaw.required),
                            .defaultValue = strDup(optCmdRaw.defaultValue),
                            .depend = optCmdRaw.depend,
                            .allowList = strLstDup(optCmdRaw.allowList),
                            .roleList = strLstDup(optCmdRaw.roleList),
                        });
                }
                MEM_CONTEXT_END();

                optCmd = yamlEventNext(yaml);
            }
            while (optCmd.type != yamlEventTypeMapEnd);

            lstSort(optCmdRawList, sortOrderAsc);

            result = lstMove(optCmdRawList, memContextPrior());
        }
        // Else command list is inherited
        else
        {
            CHECK(optList != NULL);
            yamlEventCheck(optCmdVal, yamlEventTypeScalar);

            const BldCfgOptionRaw *const optInherit = lstFind(optList, &optCmdVal.value);
            CHECK(optInherit != NULL);

            result = optInherit->cmdList;
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

static List *
bldCfgParseOptionList(Yaml *const yaml, const List *const cmdList, const List *const optGrpList)
{
    List *const result = lstNewP(sizeof(BldCfgOption), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        List *const optListRaw = lstNewP(sizeof(BldCfgOptionRaw), .comparator = lstComparatorStr);

        // Parse raw
        // -------------------------------------------------------------------------------------------------------------------------
        yamlEventNextCheck(yaml, yamlEventTypeScalar);
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent opt = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(opt, yamlEventTypeScalar);
            BldCfgOptionRaw optRaw = {.name = opt.value, .required = BOOL_TRUE_VAR};
            bool inheritFound = false;

            yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

            YamlEvent optDef = yamlEventNext(yaml);

            do
            {
                yamlEventCheck(optDef, yamlEventTypeScalar);

                if (strEqZ(optDef.value, "allow-list"))
                {
                    optRaw.allowList = bldCfgParseAllowList(yaml, optListRaw);
                }
                else if (strEqZ(optDef.value, "allow-range"))
                {
                    bldCfgParseAllowRange(yaml, &optRaw);
                }
                else if (strEqZ(optDef.value, "command"))
                {
                    optRaw.cmdList = bldCfgParseOptionCommandList(yaml, optListRaw);
                }
                else if (strEqZ(optDef.value, "command-role"))
                {
                    optRaw.cmdRoleList = bldCfgParseCommandRole(yaml);
                }
                else if (strEqZ(optDef.value, "depend"))
                {
                    optRaw.depend = bldCfgParseDepend(yaml, optListRaw);
                }
                else if (strEqZ(optDef.value, "deprecate"))
                {
                    optRaw.deprecateList = bldCfgParseOptionDeprecate(yaml);
                }
                else
                {
                    YamlEvent optDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                    if (strEqZ(optDef.value, "default"))
                    {
                        // If an override to inheritance
                        if (strEqZ(optDefVal.value, "~"))
                        {
                            optRaw.defaultValue = NULL;
                        }
                        // Else set the value
                        else
                            optRaw.defaultValue = optDefVal.value;
                    }
                    else if (strEqZ(optDef.value, "default-literal"))
                    {
                        optRaw.defaultLiteral = yamlBoolParse(optDefVal);
                    }
                    else if (strEqZ(optDef.value, "group"))
                    {
                        optRaw.group = optDefVal.value;

                        if (!lstExists(optGrpList, &optRaw.group))
                            THROW_FMT(FormatError, "option '%s' has invalid group '%s'", strZ(optRaw.name), strZ(optRaw.group));
                    }
                    else if (strEqZ(optDef.value, "inherit"))
                    {
                        const BldCfgOptionRaw *const optInherit = lstFind(optListRaw, &optDefVal.value);
                        CHECK(optInherit != NULL);

                        optRaw = *optInherit;
                        optRaw.name = opt.value;

                        // Deprecations cannot be inherited
                        optRaw.deprecateList = NULL;

                        inheritFound = true;
                    }
                    else if (strEqZ(optDef.value, "internal"))
                    {
                        optRaw.internal = yamlBoolParse(optDefVal);
                    }
                    else if (strEqZ(optDef.value, "negate"))
                    {
                        optRaw.negate = varNewBool(yamlBoolParse(optDefVal));
                    }
                    else if (strEqZ(optDef.value, "required"))
                    {
                        optRaw.required = varNewBool(yamlBoolParse(optDefVal));
                    }
                    else if (strEqZ(optDef.value, "section"))
                    {
                        optRaw.section = optDefVal.value;
                    }
                    else if (strEqZ(optDef.value, "secure"))
                    {
                        optRaw.secure = yamlBoolParse(optDefVal);
                    }
                    else if (strEqZ(optDef.value, "type"))
                    {
                        optRaw.type = optDefVal.value;
                    }
                    else
                        THROW_FMT(FormatError, "unknown option definition '%s'", strZ(optDef.value));
                }

                optDef = yamlEventNext(yaml);
            }
            while (optDef.type != yamlEventTypeMapEnd);

            // Type is required
            if (optRaw.type == NULL)
                THROW_FMT(FormatError, "option '%s' requires 'type'", strZ(optRaw.name));

            // Set defaults if not inherited
            if (!inheritFound)
            {
                // Section defaults to command line
                if (optRaw.section == NULL)
                    optRaw.section = SECTION_COMMAND_LINE_STR;

                // Set negate default if not defined
                if (optRaw.negate == NULL)
                {
                    optRaw.negate = varNewBool(
                        strEq(optRaw.type, OPT_TYPE_BOOLEAN_STR) && !strEq(optRaw.section, SECTION_COMMAND_LINE_STR));
                }

                // Build default command list if not defined
                if (optRaw.cmdList == NULL)
                {
                    List *optCmdList = lstNewP(sizeof(BldCfgOptionCommandRaw), .comparator = lstComparatorStr);

                    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(cmdList); cmdIdx++)
                    {
                        const BldCfgCommand *const cmd = lstGet(cmdList, cmdIdx);

                        if (!strEq(cmd->name, CMD_HELP_STR) && !strEq(cmd->name, CMD_VERSION_STR))
                            lstAdd(optCmdList, &(BldCfgOptionCommandRaw){.name = cmd->name});
                    }

                    lstSort(optCmdList, sortOrderAsc);
                    optRaw.cmdList = optCmdList;
                }
            }

            // Set reset
            optRaw.reset = !strEq(optRaw.section, SECTION_COMMAND_LINE_STR);

            lstAdd(optListRaw, &optRaw);

            opt = yamlEventNext(yaml);
        }
        while (opt.type != yamlEventTypeMapEnd);

        lstSort(optListRaw, sortOrderAsc);

        // Copy option list to result
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int optRawIdx = 0; optRawIdx < lstSize(optListRaw); optRawIdx++)
        {
            const BldCfgOptionRaw *const optRaw = lstGet(optListRaw, optRawIdx);

            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                lstAdd(
                    result,
                    &(BldCfgOption)
                    {
                        .name = strDup(optRaw->name),
                        .type = strDup(optRaw->type),
                        .section = strDup(optRaw->section),
                        .internal = optRaw->internal,
                        .required = varBool(optRaw->required),
                        .negate = varBool(optRaw->negate),
                        .reset = optRaw->reset,
                        .defaultValue = strDup(optRaw->defaultValue),
                        .defaultLiteral = optRaw->defaultLiteral,
                        .group = strDup(optRaw->group),
                        .secure = optRaw->secure,
                        .allowList = strLstDup(optRaw->allowList),
                        .allowRangeMin = strDup(optRaw->allowRangeMin),
                        .allowRangeMax = strDup(optRaw->allowRangeMax),
                        .deprecateList = bldCfgParseOptionDeprecateReconcile(optRaw->deprecateList),
                });
            }
            MEM_CONTEXT_END();
        }

        // Reconcile option list
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int optRawIdx = 0; optRawIdx < lstSize(optListRaw); optRawIdx++)
        {
            const BldCfgOptionRaw *const optRaw = lstGet(optListRaw, optRawIdx);

            // Reconcile option command list roles
            List *const cmdOptList = lstNewP(sizeof(BldCfgOptionCommand), .comparator = lstComparatorStr);

            for (unsigned int optCmdRawIdx = 0; optCmdRawIdx < lstSize(optRaw->cmdList); optCmdRawIdx++)
            {
                BldCfgOptionCommandRaw optCmd = *(BldCfgOptionCommandRaw *)lstGet(optRaw->cmdList, optCmdRawIdx);

                // Lookup command
                const BldCfgCommand *const cmd = lstFind(cmdList, &optCmd.name);

                if (cmd == NULL)
                {
                    THROW_FMT(
                        FormatError, "invalid command '%s' in option '%s' command list", strZ(optCmd.name), strZ(optRaw->name));
                }

                // Default required to option required if not defined
                if (optCmd.required == NULL)
                    optCmd.required = optRaw->required;

                // Default internal to option internal if not defined
                if (optCmd.internal == NULL)
                    optCmd.internal = varNewBool(optRaw->internal);

                // Default command role list if not defined
                if (optCmd.roleList == NULL)
                {
                    // Use option list as default if defined. It will need to be filtered against roles valid for each command
                    if (optRaw->cmdRoleList != NULL)
                    {
                        StringList *const roleList = strLstNew();

                        for (unsigned int cmdRoleIdx = 0; cmdRoleIdx < strLstSize(optRaw->cmdRoleList); cmdRoleIdx++)
                        {
                            const String *role = strLstGet(optRaw->cmdRoleList, cmdRoleIdx);

                            if (strLstExists(cmd->roleList, role))
                                strLstAdd(roleList, role);
                        }

                        optCmd.roleList = roleList;
                    }
                    // Else use option command list
                    else
                        optCmd.roleList = cmd->roleList;
                }

                CHECK(optCmd.roleList != NULL);

                MEM_CONTEXT_BEGIN(lstMemContext(cmdOptList))
                {
                    lstAdd(
                        cmdOptList,
                        &(BldCfgOptionCommand)
                        {
                            .name = strDup(optCmd.name),
                            .internal = varBool(optCmd.internal),
                            .required = varBool(optCmd.required),
                            .defaultValue = strDup(optCmd.defaultValue),
                            .depend = bldCfgParseDependReconcile(optCmd.depend, result),
                            .allowList = strLstDup(optCmd.allowList),
                            .roleList = strLstDup(optCmd.roleList),
                        });
                }
                MEM_CONTEXT_END();
            }

            const BldCfgOption *const opt = lstGet(result, optRawIdx);
            CHECK(strEq(opt->name, optRaw->name));

            // Assigning to const pointers this way is definitely cheating, but since we allocated the memory and know exactly where
            // it is so this is the easiest way to avoid a lot of indirection due to the dependency of BldCfgOptionDepend on
            // BldCfgOption. This would be completely unacceptable in production code but feels OK in this context.
            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                *((List **)&opt->cmdList) = lstMove(cmdOptList, memContextCurrent());
                *((const BldCfgOptionDepend **)&opt->depend) = bldCfgParseDependReconcile(optRaw->depend, result);
            }
            MEM_CONTEXT_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Build option resolve order list
***********************************************************************************************************************************/
static List *
bldCfgParseOptionResolveList(const List *const optList)
{
    List *const result = lstNewP(sizeof(BldCfgOption *), .comparator = lstComparatorStr);

    // The stanza option will always be resolved first since errors can be confusing when it is missing. That means it must exist
    // and cannot have any depedencies.
    // -----------------------------------------------------------------------------------------------------------------------------
    const BldCfgOption *const optStanza = lstFind(optList, &OPT_STANZA_STR);

    if (optStanza == NULL)
        THROW(FormatError, "option '" OPT_STANZA "' must exist");

    if (optStanza->depend != NULL)
        THROW(FormatError, "option '" OPT_STANZA "' may not depend on other option");

    for (unsigned int optCmdIdx = 0; optCmdIdx < lstSize(optStanza->cmdList); optCmdIdx++)
    {
        const BldCfgOptionCommand *const optStanzaCmd = lstGet(optStanza->cmdList, optCmdIdx);

        if (optStanzaCmd->depend != NULL)
            THROW_FMT(FormatError, "option '" OPT_STANZA "' command '%s' may not depend on other option", strZ(optStanzaCmd->name));
    }

    // Determine resolve order
    // -----------------------------------------------------------------------------------------------------------------------------
    StringList *const optResolveList = strLstNew();
    strLstAdd(optResolveList, OPT_STANZA_STR);

    do
    {
        // Was at least one option resolved in the loop?
        bool resolved = false;

        for (unsigned int optIdx = 0; optIdx < lstSize(optList); optIdx++)
        {
            const BldCfgOption *const opt = lstGet(optList, optIdx);

            // If the option has already been resolved then continue
            if (strLstExists(optResolveList, opt->name))
                continue;

            // If the option dependency has not been resolved then continue
            if (opt->depend != NULL && !strLstExists(optResolveList, opt->depend->option->name))
                continue;

            // If the option command dependency has not been resolved then continue
            unsigned int optCmdIdx = 0;

            for (; optCmdIdx < lstSize(opt->cmdList); optCmdIdx++)
            {
                const BldCfgOptionCommand *const optCmd = lstGet(opt->cmdList, optCmdIdx);

                if (optCmd->depend != NULL && !strLstExists(optResolveList, optCmd->depend->option->name))
                    break;
            }

            if (optCmdIdx < lstSize(opt->cmdList))
                continue;

            // Option dependencies have been resolved
            strLstAdd(optResolveList, opt->name);
            resolved = true;
        }

        // If nothing was resolved in the loop then there may be a circular reference
        if (!resolved)
        {
            // Find the options that were not resolved
            StringList *const unresolvedList = strLstNew();

            for (unsigned int optIdx = 0; optIdx < lstSize(optList); optIdx++)
            {
                const BldCfgOption *const opt = lstGet(optList, optIdx);

                if (!strLstExists(optResolveList, opt->name))
                    strLstAdd(unresolvedList, opt->name);
            }

            THROW_FMT(
                FormatError,
                "unable to resolve dependencies for option(s) '%s'\n"
                "HINT: are there circular dependencies?",
                strZ(strLstJoin(unresolvedList, ", ")));
        }
    }
    while (strLstSize(optResolveList) != lstSize(optList));

    // Copy resolved list
    for (unsigned int optResolveIdx = 0; optResolveIdx < strLstSize(optResolveList); optResolveIdx++)
    {
        const String *const optName = strLstGet(optResolveList, optResolveIdx);
        const BldCfgOption *const opt = lstFind(optList, &optName);
        lstAdd(result, &opt);
    }

    return result;
}

/**********************************************************************************************************************************/
BldCfg
bldCfgParse(const Storage *const storageRepo)
{
    // Initialize yaml
    Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/config/config.yaml"))));
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    // Parse configuration
    const List *const cmdList = bldCfgParseCommandList(yaml);
    const List *const optGrpList = bldCfgParseOptionGroupList(yaml);
    const List *const optList = bldCfgParseOptionList(yaml, cmdList, optGrpList);
    const List *const optResolveList = bldCfgParseOptionResolveList(optList);

    return (BldCfg){.cmdList = cmdList, .optGrpList = optGrpList, .optList = optList, .optResolveList = optResolveList};
}
