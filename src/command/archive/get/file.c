/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
#include "command/archive/get/file.h"
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
FN_EXTERN ArchiveGetFileResult
archiveGetFile(
    const Storage *const storage, const String *const request, const List *const actualList, const String *const walDestination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, request);
        FUNCTION_LOG_PARAM(LIST, actualList);
        FUNCTION_LOG_PARAM(STRING, walDestination);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(request != NULL);
    ASSERT(actualList != NULL && !lstEmpty(actualList));
    ASSERT(walDestination != NULL);

    ArchiveGetFileResult result = {.warnList = strLstNew()};

    // Check all files in the actual list and return as soon as one is copied
    bool copied = false;

    for (unsigned int actualIdx = 0; actualIdx < lstSize(actualList); actualIdx++)
    {
        const ArchiveGetFile *const actual = lstGet(actualList, actualIdx);

        // Is the file compressible during the copy?
        bool compressible = true;

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                StorageWrite *const destination = storageNewWriteP(
                    storage, walDestination, .noCreatePath = true, .noSyncFile = true, .noSyncPath = true, .noAtomic = true);

                // If there is a cipher then add the decrypt filter
                if (actual->cipherType != cipherTypeNone)
                {
                    ioFilterGroupAdd(
                        ioWriteFilterGroup(storageWriteIo(destination)),
                        cipherBlockNewP(cipherModeDecrypt, actual->cipherType, BUFSTR(actual->cipherPassArchive)));
                    compressible = false;
                }

                // If file is compressed then add the decompression filter
                CompressType compressType = compressTypeFromName(actual->file);

                if (compressType != compressTypeNone)
                {
                    ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), decompressFilterP(compressType));
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
        // Log errors as warnings and continue
        CATCH_ANY()
        {
            strLstAddFmt(
                result.warnList, "%s: %s [%s] %s", cfgOptionGroupName(cfgOptGrpRepo, actual->repoIdx), strZ(actual->file),
                errorTypeName(errorType()), errorMessage());
        }
        TRY_END();

        // Stop on success
        if (copied)
            break;
    }

    // If no file was successfully copied then error
    if (!copied)
    {
        ASSERT(!strLstEmpty(result.warnList));
        THROW_FMT(FileReadError, "unable to get %s:\n%s", strZ(request), strZ(strLstJoin(result.warnList, "\n")));
    }

    FUNCTION_LOG_RETURN_STRUCT(result);
}
