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

/**********************************************************************************************************************************/
BldPg
bldPgParse(const Storage *const storageRepo)
{

    // !!!
    StringList *const typeList = strLstNew();
    strLstAddZ(typeList, "CheckPoint");
    strLstAddZ(typeList, "ControlFileData");
    strLstAddZ(typeList, "DBState");
    strLstAddZ(typeList, "DB_STARTUP");
    strLstAddZ(typeList, "DB_SHUTDOWNED");
    strLstAddZ(typeList, "DB_SHUTDOWNED_IN_RECOVERY");
    strLstAddZ(typeList, "DB_SHUTDOWNING");
    strLstAddZ(typeList, "DB_IN_CRASH_RECOVERY");
    strLstAddZ(typeList, "DB_IN_ARCHIVE_RECOVERY");
    strLstAddZ(typeList, "DB_IN_PRODUCTION");
    strLstAddZ(typeList, "FullTransactionId");
    strLstAddZ(typeList, "int64");
    strLstAddZ(typeList, "MultiXactId");
    strLstAddZ(typeList, "MultiXactOffset");
    strLstAddZ(typeList, "Oid");
    strLstAddZ(typeList, "pg_crc32");
    strLstAddZ(typeList, "pg_crc32c");
    strLstAddZ(typeList, "pg_time_t");
    strLstAddZ(typeList, "TimeLineID");
    strLstAddZ(typeList, "XLogLongPageHeaderData");
    strLstAddZ(typeList, "XLogPageHeaderData");
    strLstAddZ(typeList, "XLogRecPtr");

    // Parse defines from version.vendor.h
    StringList *const defineList = bldPgParseDefine(
        strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("src/postgres/interface/version.vendor.h")))));
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
