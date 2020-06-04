/***********************************************************************************************************************************
Repository Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"
#include "storage/s3/storage.intern.h"

/**********************************************************************************************************************************/
void
cmdRepoCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(storageType(storageRepo()), STORAGE_S3_TYPE_STR))
        {
            storageS3Request(
                (StorageS3 *)storageDriver(storageRepoWrite()), HTTP_VERB_PUT_STR, FSLASH_STR, NULL, NULL, false, false);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
