/***********************************************************************************************************************************
Parse PostgreSQL Interface Yaml
***********************************************************************************************************************************/
#include "build.auto.h"

#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "build/postgres/parse.h"

/***********************************************************************************************************************************
Parse version list
***********************************************************************************************************************************/
typedef struct BldPgVersionRaw
{
    const String *version;                                          // See BldPgVersion for comments
    bool release;
} BldPgVersionRaw;

static List *
bldPgVersionList(Yaml *const yaml)
{
    List *const result = lstNewP(sizeof(BldPgVersion), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        YamlEvent pgDef = yamlEventNextCheck(yaml, yamlEventTypeScalar);

        if (strEqZ(pgDef.value, "version"))
        {
            yamlEventNextCheck(yaml, yamlEventTypeSeqBegin);

            YamlEvent ver = yamlEventNext(yaml);

            do
            {
                BldPgVersionRaw pgRaw = {.release = true};

                if (ver.type == yamlEventTypeMapBegin)
                {
                    pgRaw.version = yamlEventNextCheck(yaml, yamlEventTypeScalar).value;
                    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

                    YamlEvent verDef = yamlEventNextCheck(yaml, yamlEventTypeScalar);
                    YamlEvent verDefVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);

                    if (strEqZ(verDef.value, "release"))
                    {
                        pgRaw.release = yamlBoolParse(verDefVal);
                    }
                    else
                        THROW_FMT(FormatError, "unknown postgres definition '%s'", strZ(verDef.value));

                    yamlEventNextCheck(yaml, yamlEventTypeMapEnd);
                    yamlEventNextCheck(yaml, yamlEventTypeMapEnd);
                }
                else
                    pgRaw.version = ver.value;

                // Add to list
                MEM_CONTEXT_BEGIN(lstMemContext(result))
                {
                    lstAdd(
                        result,
                        &(BldPgVersion)
                        {
                            .version = strDup(pgRaw.version),
                            .release = pgRaw.release,
                        });
                }
                MEM_CONTEXT_END();

                ver = yamlEventNext(yaml);
            }
            while (ver.type != yamlEventTypeSeqEnd);
        }
        else
            THROW_FMT(FormatError, "unknown postgres definition '%s'", strZ(pgDef.value));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Parse defines from header
***********************************************************************************************************************************/
static StringList *
bldPgParseDefine(const String *const header)
{
    const StringList *const lineList = strLstNewSplitZ(header, "\n");
    StringList *const result = strLstNew();

    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
    {
        const String *const line = strTrim(strLstGet(lineList, lineIdx));

        if (strBeginsWithZ(line, "#define"))
        {
            const String *const defineToken = strTrim(strLstGet(strLstNewSplitZ(line, " "), 1));

            if (strEmpty(defineToken))
                THROW_FMT(FormatError, "unable to find define -- are there extra spaces on '%s'", strZ(line));

            const StringList *defineList = strLstNewSplitZ(defineToken, "(");
            const String *define;

            if (strLstSize(defineList) > 1)
                define = strTrim(strLstGet(defineList, 0));
            else
            {
                defineList = strLstNewSplitZ(defineToken, "\t");
                define = strTrim(strLstGet(defineList, 0));
            }

            strLstAddIfMissing(result, define);
        }
    }

    return result;
}

/***********************************************************************************************************************************
Parse defines from header
***********************************************************************************************************************************/
static StringList *
bldPgParseType(const String *const header)
{
    const StringList *const lineList = strLstNewSplitZ(header, "\n");
    StringList *const result = strLstNew();
    bool scan = false;
    bool scanEnum = false;

    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
    {
        const String *const line = strTrim(strLstGet(lineList, lineIdx));
        const StringList *const tokenList = strLstNewSplitZ(line, " ");
        const String *const tokenFirst = strLstGet(tokenList, 0);

        if (strEqZ(tokenFirst, "typedef"))
        {
            ASSERT(!scan);

            const String *const tokenType = strLstGet(tokenList, 1);

            if (strEqZ(tokenType, "struct") || strEqZ(tokenType, "enum"))
            {
                scan = true;
                scanEnum = strEqZ(tokenType, "enum");
            }
            else
                strLstAddIfMissing(result, strLstGet(strLstNewSplitZ(strLstGet(tokenList, strLstSize(tokenList) - 1), ";"), 0));
        }
        else if (strEqZ(tokenFirst, "}"))
        {
            strLstAddIfMissing(result, strLstGet(strLstNewSplitZ(strLstGet(tokenList, strLstSize(tokenList) - 1), ";"), 0));

            scan = false;
            scanEnum = false;
        }
        else if (scanEnum && !strEqZ(tokenFirst, "{"))
            strLstAddIfMissing(result, strLstGet(strLstNewSplitZ(strLstGet(tokenList, 0), ","), 0));
    }

    return result;
}

/**********************************************************************************************************************************/
BldPg
bldPgParse(const Storage *const storageRepo)
{
    // Parse types from version.vendor.h
    const String *const vendorHeader = strNewBuf(
        storageGetP(storageNewReadP(storageRepo, STRDEF("src/postgres/interface/version.vendor.h"))));
    StringList *const typeList = bldPgParseType(vendorHeader);
    strLstSort(typeList, sortOrderAsc);

    // Parse defines from version.vendor.h
    StringList *const defineList = bldPgParseDefine(vendorHeader);
    strLstAddZ(defineList, "CATALOG_VERSION_NO_MAX");
    strLstAddZ(defineList, "PG_VERSION");
    strLstSort(defineList, sortOrderAsc);

    // Parse defines from version.intern.h
    const StringList *const functionList = bldPgParseDefine(
        strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("src/postgres/interface/version.intern.h")))));

    // Initialize yaml
    Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/postgres/postgres.yaml"))));
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    // Parse postgres
    return (BldPg){.pgList = bldPgVersionList(yaml), .typeList = typeList, .defineList = defineList, .functionList = functionList};
}
