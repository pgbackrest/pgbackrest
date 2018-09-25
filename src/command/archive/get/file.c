/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#include "command/archive/get/file.h"
#include "command/archive/common.h"
#include "command/control/control.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/log.h"
#include "compress/gzip.h"
#include "compress/gzipDecompress.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "storage/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check if a WAL file exists in the repository
***********************************************************************************************************************************/
static String *
archiveGetCheck(const String *archiveFile)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, archiveFile);

        FUNCTION_TEST_ASSERT(archiveFile != NULL);
    FUNCTION_DEBUG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get pg control info
        PgControl controlInfo = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        // Attempt to load the archive info file
        InfoArchive *info = infoArchiveNew(storageRepo(), strNew(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), false);

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
                    String *walSegmentFile = walSegmentFind(storageRepo(), archiveId, archiveFile);

                    if (walSegmentFile != NULL)
                    {
                        archiveFileActual = strNewFmt("%s/%s", strPtr(strSubN(archiveFile, 0, 16)), strPtr(walSegmentFile));
                        break;
                    }
                }
                // Else if not a WAL segment, see if it exists in the archive dir
                else if (
                    storageExistsNP(
                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(archiveFile))))
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
                strPtr(pgVersionToStr(controlInfo.version)), controlInfo.systemId);
        }

        if (archiveFileActual != NULL)
        {
            memContextSwitch(MEM_CONTEXT_OLD());
            result = strNewFmt("%s/%s", strPtr(archiveId), strPtr(archiveFileActual));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Copy a file from the archive to the specified destination
***********************************************************************************************************************************/
int
archiveGetFile(const String *archiveFile, const String *walDestination)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, archiveFile);
        FUNCTION_DEBUG_PARAM(STRING, walDestination);

        FUNCTION_TEST_ASSERT(archiveFile != NULL);
        FUNCTION_TEST_ASSERT(walDestination != NULL);
    FUNCTION_DEBUG_END();

    // By default result indicates WAL segment not found
    int result = 1;

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure the file exists and other checks pass
        String *archiveFileActual = archiveGetCheck(archiveFile);

        if (archiveFileActual != NULL)
        {
            StorageFileWrite *destination = storageNewWriteP(
                storageLocalWrite(), walDestination, .noCreatePath = true,  .noSyncFile = true, .noSyncPath = true,
                .noAtomic = true);

            // If file is gzipped then add the decompression filter
            if (strEndsWithZ(archiveFileActual, "." GZIP_EXT))
            {
                IoFilterGroup *filterGroup = ioFilterGroupNew();
                ioFilterGroupAdd(filterGroup, gzipDecompressFilter(gzipDecompressNew(false)));
                ioWriteFilterGroupSet(storageFileWriteIo(destination), filterGroup);
            }

            // Copy the file
            storageCopyNP(
                storageNewReadNP(storageRepo(), strNewFmt("%s/%s", STORAGE_REPO_ARCHIVE, strPtr(archiveFileActual))), destination);

            // The WAL file was found
            result = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(INT, result);
}
