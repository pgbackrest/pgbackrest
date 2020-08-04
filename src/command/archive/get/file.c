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

/***********************************************************************************************************************************
Check if a WAL file exists in the repository
***********************************************************************************************************************************/
#define FUNCTION_LOG_ARCHIVE_GET_CHECK_RESULT_TYPE                                                                                 \
    ArchiveGetCheckResult
#define FUNCTION_LOG_ARCHIVE_GET_CHECK_RESULT_FORMAT(value, buffer, bufferSize)                                                    \
    objToLog(&value, "ArchiveGetCheckResult", buffer, bufferSize)

typedef struct ArchiveGetCheckResult
{
    String *archiveFileActual;
    String *cipherPass;
} ArchiveGetCheckResult;

static ArchiveGetCheckResult
archiveGetCheck(const String *archiveFile, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);

    ArchiveGetCheckResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get pg control info
        PgControl controlInfo = pgControlFromFile(storagePg());

        // Attempt to load the archive info file
        InfoArchive *info = infoArchiveLoadFile(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType, cipherPass);

        // Loop through the pg history in case the WAL we need is not in the most recent archive id
        String *archiveId = NULL;
        const String *archiveFileActual = NULL;

        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoArchivePg(info)); pgIdx++)
        {
            InfoPgData pgData = infoPgData(infoArchivePg(info), pgIdx);

            // Only use the archive id if it matches the current cluster
            if (pgData.systemId == controlInfo.systemId && pgData.version == controlInfo.version)
            {
                archiveId = infoPgArchiveId(infoArchivePg(info), pgIdx);

                // If a WAL segment search among the possible file names
                if (walIsSegment(archiveFile))
                {
                    String *walSegmentFile = walSegmentFind(storageRepo(), archiveId, archiveFile, 0);

                    if (walSegmentFile != NULL)
                    {
                        archiveFileActual = strNewFmt("%s/%s", strZ(strSubN(archiveFile, 0, 16)), strZ(walSegmentFile));
                        break;
                    }
                }
                // Else if not a WAL segment, see if it exists in the archive dir
                else if (
                    storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(archiveFile))))
                {
                    archiveFileActual = archiveFile;
                    break;
                }
            }
        }

        // Error if no archive id was found -- this indicates a mismatch with the current cluster
        if (archiveId == NULL)
        {
            THROW_FMT(
                ArchiveMismatchError, "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
                strZ(pgVersionToStr(controlInfo.version)), controlInfo.systemId);
        }

        if (archiveFileActual != NULL)
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.archiveFileActual = strNewFmt("%s/%s", strZ(archiveId), strZ(archiveFileActual));
                result.cipherPass = strDup(infoArchiveCipherPass(info));
            }
            MEM_CONTEXT_PRIOR_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(ARCHIVE_GET_CHECK_RESULT, result);
}

/**********************************************************************************************************************************/
int
archiveGetFile(
    const Storage *storage, const String *archiveFile, const String *walDestination, bool durable, CipherType cipherType,
    const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(STRING, walDestination);
        FUNCTION_LOG_PARAM(BOOL, durable);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);
    ASSERT(walDestination != NULL);

    // By default result indicates WAL segment not found
    int result = 1;

    // Is the file compressible during the copy?
    bool compressible = true;

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure the file exists and other checks pass
        ArchiveGetCheckResult archiveGetCheckResult = archiveGetCheck(archiveFile, cipherType, cipherPass);

        if (archiveGetCheckResult.archiveFileActual != NULL)
        {
            StorageWrite *destination = storageNewWriteP(
                storage, walDestination, .noCreatePath = true, .noSyncFile = !durable, .noSyncPath = !durable,
                .noAtomic = !durable);

            // If there is a cipher then add the decrypt filter
            if (cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioWriteFilterGroup(storageWriteIo(destination)), cipherBlockNew(cipherModeDecrypt, cipherType,
                        BUFSTR(archiveGetCheckResult.cipherPass), NULL));
                compressible = false;
            }

            // If file is compressed then add the decompression filter
            CompressType compressType = compressTypeFromName(archiveGetCheckResult.archiveFileActual);

            if (compressType != compressTypeNone)
            {
                ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), decompressFilter(compressType));
                compressible = false;
            }

            // Copy the file
            storageCopyP(
                storageNewReadP(
                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveGetCheckResult.archiveFileActual)),
                    .compressible = compressible),
                destination);

            // The WAL file was found
            result = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
