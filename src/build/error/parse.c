/***********************************************************************************************************************************
Parse Error Yaml
***********************************************************************************************************************************/
#include <build.h>

#include <yaml.h>

#include "common/log.h"
#include "common/macro.h"
#include "common/type/convert.h"
#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "build/error/parse.h"

/***********************************************************************************************************************************
Error min/max codes
***********************************************************************************************************************************/
#define ERROR_CODE_MIN                                              25
#define ERROR_CODE_MAX                                              125

/***********************************************************************************************************************************
Parse error list
***********************************************************************************************************************************/
typedef struct BldErrErrorRaw
{
    const String *const name;                                       // See BldErrError for comments
    unsigned int code;
    bool fatal;
} BldErrErrorRaw;

static List *
bldErrParseErrorList(Yaml *const yaml)
{
    List *const result = lstNewP(sizeof(BldErrError), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        YAML_MAP_BEGIN(yaml)
        {
            BldErrErrorRaw errRaw = {.name = yamlScalarNext(yaml).value};

            // If scalar then it is an error code
            if (yamlEventPeekIs(yaml, yamlEventTypeScalar))
            {
                errRaw.code = cvtZToUInt(strZ(yamlScalarNext(yaml).value));
            }
            // Else there may be multiple definitions
            else
            {
                YAML_MAP_BEGIN(yaml)
                {
                    const String *const errDef = yamlScalarNext(yaml).value;
                    const YamlEvent errDefVal = yamlScalarNext(yaml);

                    if (strEqZ(errDef, "code"))
                        errRaw.code = cvtZToUInt(strZ(errDefVal.value));
                    else if (strEqZ(errDef, "fatal"))
                        errRaw.fatal = yamlBoolParse(errDefVal);
                    else
                        THROW_FMT(FormatError, "unknown error definition '%s'", strZ(errDef));
                }
                YAML_MAP_END();
            }

            if (errRaw.code < ERROR_CODE_MIN || errRaw.code > ERROR_CODE_MAX)
            {
                THROW_FMT(
                    FormatError, "error '%s' code must be >= " STRINGIFY(ERROR_CODE_MIN) " and <= " STRINGIFY(ERROR_CODE_MAX),
                    strZ(errRaw.name));
            }

            // Add to list
            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                const BldErrError bldErrError =
                {
                    .name = strDup(errRaw.name),
                    .code = errRaw.code,
                    .fatal = errRaw.fatal,
                };

                lstAdd(result, &bldErrError);
            }
            MEM_CONTEXT_END();
        }
        YAML_MAP_END();
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/**********************************************************************************************************************************/
BldErr
bldErrParse(const Storage *const storageRepo)
{
    // Initialize yaml
    Yaml *const yaml = yamlNew(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/error/error.yaml"))));

    // Parse error
    const List *const errList = bldErrParseErrorList(yaml);

    return (BldErr){.errList = errList};
}
