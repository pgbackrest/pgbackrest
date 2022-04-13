/***********************************************************************************************************************************
Repository Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "storage/helper.h"
#include "storage/azure/storage.intern.h"
#include "storage/gcs/storage.intern.h"
#include "storage/s3/storage.intern.h"

/**********************************************************************************************************************************/
void
cmdRepoCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        switch (storageType(storageRepo()))
        {
            case STORAGE_AZURE_TYPE:
            {
                storageAzureRequestP(
                    (StorageAzure *)storageDriver(storageRepoWrite()), HTTP_VERB_PUT_STR,
                    .query = httpQueryAdd(httpQueryNewP(), AZURE_QUERY_RESTYPE_STR, AZURE_QUERY_VALUE_CONTAINER_STR));

                break;
            }

            case STORAGE_GCS_TYPE:
            {
                const KeyValue *const kvContent = kvPut(kvNew(), GCS_JSON_NAME_VAR, VARSTR(cfgOptionStr(cfgOptRepoGcsBucket)));

                storageGcsRequestP(
                    (StorageGcs *)storageDriver(storageRepoWrite()), HTTP_VERB_POST_STR, .noBucket = true,
                    .content = BUFSTR(jsonFromKv(kvContent)));

                break;
            }

            case STORAGE_S3_TYPE:
                storageS3RequestP((StorageS3 *)storageDriver(storageRepoWrite()), HTTP_VERB_PUT_STR, FSLASH_STR);
                break;

            // Other storage types do not require the repo to be created
            default:
                break;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
