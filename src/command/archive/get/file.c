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
void
archiveGetFile(
    const Storage *storage, const String *archiveFile, const String *walDestination, bool durable, CipherType cipherType,
    const String *cipherPassArchive)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(STRING, walDestination);
        FUNCTION_LOG_PARAM(BOOL, durable);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassArchive);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);
    ASSERT(walDestination != NULL);

    // Is the file compressible during the copy?
    bool compressible = true;

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageWrite *destination = storageNewWriteP(
            storage, walDestination, .noCreatePath = true, .noSyncFile = !durable, .noSyncPath = !durable, .noAtomic = !durable);

        // If there is a cipher then add the decrypt filter
        if (cipherType != cipherTypeNone)
        {
            ioFilterGroupAdd(
                ioWriteFilterGroup(storageWriteIo(destination)),
                cipherBlockNew(cipherModeDecrypt, cipherType, BUFSTR(cipherPassArchive), NULL));
            compressible = false;
        }

        // If file is compressed then add the decompression filter
        CompressType compressType = compressTypeFromName(archiveFile);

        if (compressType != compressTypeNone)
        {
            ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), decompressFilter(compressType));
            compressible = false;
        }

        // Copy the file
        storageCopyP(
            storageNewReadP(storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveFile)), .compressible = compressible),
            destination);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
