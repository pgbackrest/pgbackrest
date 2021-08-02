/***********************************************************************************************************************************
Parse Error Yaml
***********************************************************************************************************************************/
#include "build.auto.h"

#include <yaml.h>

#include "common/log.h"
#include "common/type/convert.h"
#include "storage/posix/storage.h"

#include "build/common/yaml.h"
#include "build/error/parse.h"

/***********************************************************************************************************************************
Parse error list
***********************************************************************************************************************************/
typedef struct BldErrErrorRaw
{
    const String *const name;                                       // See BldErrError for comments
    unsigned int code;                                        
} BldErrErrorRaw;

static List *
bldErrParseErrorList(Yaml *const yaml)
{
    List *const result = lstNewP(sizeof(BldErrError), .comparator = lstComparatorStr);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        yamlEventNextCheck(yaml, yamlEventTypeMapBegin);

        YamlEvent err = yamlEventNext(yaml);

        do
        {
            yamlEventCheck(err, yamlEventTypeScalar);

            BldErrErrorRaw errRaw =
            {
                .name = err.value,
            };

            YamlEvent errVal = yamlEventNextCheck(yaml, yamlEventTypeScalar);
            errRaw.code = cvtZToUInt(strZ(errVal.value));

            MEM_CONTEXT_BEGIN(lstMemContext(result))
            {
                lstAdd(
                    result,
                    &(BldErrError)
                    {
                        .name = strDup(errRaw.name),
                        .code = errRaw.code,
                    });
            }
            MEM_CONTEXT_END();

            err = yamlEventNext(yaml);
        }
        while (err.type != yamlEventTypeMapEnd);

        // !!! lstSort(result, sortOrderAsc);
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
