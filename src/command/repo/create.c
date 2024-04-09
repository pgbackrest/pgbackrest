/***********************************************************************************************************************************
Repository Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "storage/azure/storage.intern.h"
#include "storage/gcs/storage.intern.h"
#include "storage/helper.h"
#include "storage/s3/storage.intern.h"

/**********************************************************************************************************************************/
FN_EXTERN void
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
                storageGcsRequestP(
                    (StorageGcs *)storageDriver(storageRepoWrite()), HTTP_VERB_POST_STR, .noBucket = true,
                    .content = BUFSTR(
                        jsonWriteResult(
                            jsonWriteObjectEnd(
                                jsonWriteStr(
                                    jsonWriteKeyZ(
                                        jsonWriteObjectBegin(
                                            jsonWriteNewP()), GCS_JSON_NAME), cfgOptionStr(cfgOptRepoGcsBucket))))));

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
