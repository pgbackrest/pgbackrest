/***********************************************************************************************************************************
Parse Define Yaml
***********************************************************************************************************************************/
#include "build.auto.h"

#include <yaml.h>

#include "common/debug.h"
#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "command/test/define.h"

/***********************************************************************************************************************************
Parse module list
***********************************************************************************************************************************/
static void
testDefParseModuleList(Yaml *const yaml, List *const moduleList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(YAML, yaml);
        FUNCTION_LOG_PARAM(LIST, moduleList);
    FUNCTION_LOG_END();

    // Global lists to be copied to next test
    StringList *const globalDependList = strLstNew();
    StringList *const globalFeatureList = strLstNew();

    // Module list
    YAML_SEQ_BEGIN(yaml)
    {
        // Module
        YAML_MAP_BEGIN(yaml)
        {
            // Module name
            yamlScalarNextCheckZ(yaml, "name");
            const String *const moduleName = yamlScalarNext(yaml).value;

            // Submodule List
            yamlScalarNextCheckZ(yaml, "test");

            YAML_SEQ_BEGIN(yaml)
            {
                TestDefModule testDefModule = {0};
                StringList *includeList = NULL;
                List *coverageList = NULL;

                // Submodule
                YAML_MAP_BEGIN(yaml)
                {
                    YamlEvent subModuleDef = yamlEventNext(yaml);

                    if (strEqZ(subModuleDef.value, "binReq"))
                    {
                        testDefModule.binRequired = yamlBoolParse(yamlScalarNext(yaml));
                    }
                    else if (strEqZ(subModuleDef.value, "containerReq"))
                    {
                        testDefModule.containerRequired = yamlBoolParse(yamlScalarNext(yaml));
                    }
                    else if (strEqZ(subModuleDef.value, "coverage"))
                    {
                        coverageList = lstNewP(sizeof(TestDefCoverage), .comparator = lstComparatorStr);

                        YAML_SEQ_BEGIN(yaml)
                        {
                            TestDefCoverage testDefCoverage = {0};

                            if (yamlEventPeek(yaml).type == yamlEventTypeScalar)
                            {
                                testDefCoverage.name = yamlScalarNext(yaml).value;
                                testDefCoverage.coverable = true;
                            }
                            else
                            {
                                YAML_MAP_BEGIN(yaml)
                                {
                                    testDefCoverage.name = yamlScalarNext(yaml).value;
                                    yamlScalarNextCheckZ(yaml, "noCode");
                                }
                                YAML_MAP_END();
                            }

                            MEM_CONTEXT_OBJ_BEGIN(coverageList)
                            {
                                testDefCoverage.name = strDup(testDefCoverage.name);
                                lstAdd(coverageList, &testDefCoverage);
                            }
                            MEM_CONTEXT_OBJ_END();

                            // Also add to the global depend list
                            if (testDefCoverage.coverable)
                                strLstAddIfMissing(globalDependList, testDefCoverage.name);
                        }
                        YAML_SEQ_END();
                    }
                    else if (strEqZ(subModuleDef.value, "define"))
                    {
                        testDefModule.flag = yamlScalarNext(yaml).value;
                    }
                    else if (strEqZ(subModuleDef.value, "depend"))
                    {
                        YAML_SEQ_BEGIN(yaml)
                        {
                            strLstAddIfMissing(globalDependList, yamlEventNext(yaml).value);
                        }
                        YAML_SEQ_END();
                    }
                    else if (strEqZ(subModuleDef.value, "feature"))
                    {
                        testDefModule.feature = yamlScalarNext(yaml).value;
                    }
                    else if (strEqZ(subModuleDef.value, "harness"))
                    {
                        if (yamlEventPeek(yaml).type == yamlEventTypeScalar)
                        {
                            yamlScalarNext(yaml);
                        }
                        else
                        {
                            YAML_MAP_BEGIN(yaml)
                            {
                                yamlScalarNextCheckZ(yaml, "name");
                                yamlScalarNext(yaml);

                                yamlScalarNextCheckZ(yaml, "shim");

                                YAML_MAP_BEGIN(yaml)
                                {
                                    yamlScalarNext(yaml);

                                    if (yamlEventPeek(yaml).type == yamlEventTypeScalar)
                                    {
                                        yamlScalarNext(yaml);
                                    }
                                    else
                                    {
                                        YAML_MAP_BEGIN(yaml)
                                        {
                                            yamlScalarNextCheckZ(yaml, "function");

                                            YAML_SEQ_BEGIN(yaml)
                                            {
                                                yamlScalarNext(yaml);
                                            }
                                            YAML_SEQ_END();
                                        }
                                        YAML_MAP_END();
                                    }
                                }
                                YAML_MAP_END();
                            }
                            YAML_MAP_END();
                        }
                    }
                    else if (strEqZ(subModuleDef.value, "include"))
                    {
                        includeList = strLstNew();

                        YAML_SEQ_BEGIN(yaml)
                        {
                            strLstAdd(includeList, yamlEventNext(yaml).value);
                        }
                        YAML_SEQ_END();
                    }
                    else if (strEqZ(subModuleDef.value, "name"))
                    {
                        testDefModule.name = strNewFmt("%s/%s", strZ(moduleName), strZ(yamlScalarNext(yaml).value));
                    }
                    else if (strEqZ(subModuleDef.value, "total"))
                    {
                        testDefModule.total = cvtZToUInt(strZ(yamlScalarNext(yaml).value));
                    }
                    else
                    {
                        THROW_FMT(
                            FormatError, "unexpected scalar '%s' at line %zu, column %zu", strZ(subModuleDef.value),
                            subModuleDef.line, subModuleDef.column);
                    }
                }
                YAML_MAP_END();

                // Depend list is the global list minus the coverage and include lists
                StringList *const dependList = strLstNew();

                for (unsigned int dependIdx = 0; dependIdx < strLstSize(globalDependList); dependIdx++)
                {
                    const String *const depend = strLstGet(globalDependList, dependIdx);

                    if ((coverageList == NULL || !lstExists(coverageList, &depend)) &&
                        (includeList == NULL || !strLstExists(includeList, depend)))
                    {
                        strLstAdd(dependList, depend);
                    }
                }

                // Add test module
                MEM_CONTEXT_OBJ_BEGIN(moduleList)
                {
                    testDefModule.name = strDup(testDefModule.name);
                    testDefModule.coverageList = lstMove(coverageList, memContextCurrent());
                    testDefModule.flag = strDup(testDefModule.flag);
                    testDefModule.includeList = strLstMove(includeList, memContextCurrent());

                    if (strLstSize(dependList) > 0)
                        testDefModule.dependList = strLstMove(dependList, memContextCurrent());

                    if (testDefModule.feature != NULL)
                        testDefModule.feature = strUpper(strDup(testDefModule.feature));

                    if (strLstSize(globalFeatureList) > 0)
                        testDefModule.featureList = strLstDup(globalFeatureList);
                }
                MEM_CONTEXT_OBJ_END();

                lstAdd(moduleList, &testDefModule);

                // Add feature to global list
                if (testDefModule.feature != NULL)
                    strLstAdd(globalFeatureList, testDefModule.feature);
            }
            YAML_SEQ_END();
        }
        YAML_MAP_END();
    }
    YAML_SEQ_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
TestDef
testDefParse(const Storage *const storageRepo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
    FUNCTION_LOG_END();

    // Module list
    List *const moduleList = lstNewP(sizeof(TestDefModule), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Initialize yaml
        Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("test/define.yaml"))));
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        // Parse unit tests
        yamlScalarNextCheckZ(yaml, "unit");
        testDefParseModuleList(yaml, moduleList);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT((TestDef){.moduleList = moduleList});
}
