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

/**********************************************************************************************************************************/
BldPg
bldPgParse(const Storage *const storageRepo)
{
    // Initialize yaml
    Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/postgres/postgres.yaml"))));
    yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

    // Parse postgres
    return (BldPg){.pgList = bldPgVersionList(yaml)};
}
