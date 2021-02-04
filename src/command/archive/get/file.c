/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/get/file.h"
#include "command/archive/common.h"
#include "command/control/common.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/log.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
ArchiveGetFileResult archiveGetFile(
    const Storage *storage, const String *request, const List *actualList, const String *walDestination, bool durable)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, request);
        FUNCTION_LOG_PARAM(LIST, actualList);
        FUNCTION_LOG_PARAM(STRING, walDestination);
        FUNCTION_LOG_PARAM(BOOL, durable);
    FUNCTION_LOG_END();

    ASSERT(request != NULL);
    ASSERT(actualList != NULL && !lstEmpty(actualList));
    ASSERT(walDestination != NULL);

    ArchiveGetFileResult result = {0};

    // Test for stop file
    lockStopTest();

    // Check all files in the actual list and return as soon as one is copied
    bool copied = false;

    for (unsigned int actualIdx = 0; actualIdx < lstSize(actualList); actualIdx++)
    {
        ArchiveGetFile *actual = lstGet(actualList, actualIdx);

        // Is the file compressible during the copy?
        bool compressible = true;

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                StorageWrite *destination = storageNewWriteP(
                    storage, walDestination, .noCreatePath = true, .noSyncFile = !durable, .noSyncPath = !durable,
                    .noAtomic = !durable);

                // If there is a cipher then add the decrypt filter
                if (actual->cipherType != cipherTypeNone)
                {
                    ioFilterGroupAdd(
                        ioWriteFilterGroup(storageWriteIo(destination)),
                        cipherBlockNew(cipherModeDecrypt, actual->cipherType, BUFSTR(actual->cipherPassArchive), NULL));
                    compressible = false;
                }

                // If file is compressed then add the decompression filter
                CompressType compressType = compressTypeFromName(actual->file);

                if (compressType != compressTypeNone)
                {
                    ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), decompressFilter(compressType));
                    compressible = false;
                }

                // Copy the file
                storageCopyP(
                    storageNewReadP(
                        storageRepoIdx(actual->repoIdx), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(actual->file)),
                        .compressible = compressible),
                    destination);
            }
            MEM_CONTEXT_TEMP_END();

            // File was successfully copied
            result.actualIdx = actualIdx;
            copied = true;
        }
        CATCH_ANY()
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.warn = strCatFmt(
                    result.warn == NULL ? strNew("") : strCatChr(result.warn, '\n'), "repo%u: [%s] %s",
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, actual->repoIdx), errorTypeName(errorType()), errorMessage());
            }
            MEM_CONTEXT_PRIOR_END();
        }
        TRY_END();

        // Stop on success
        if (copied)
            break;
    }

    // If no file was successfully copied then error
    if (!copied)
    {
        ASSERT(result.warn != NULL);
        THROW_FMT(FileReadError, "unable to get %s:\n%s", strZ(request), strZ(result.warn));
    }

    FUNCTION_LOG_RETURN_STRUCT(result);
}
