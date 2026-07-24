/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#include <build.h>

#include "command/backup/file.h"
#include "command/backup/protocol.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Comparator to order the files processed together in a single backup job for efficient copying.

Block incremental files sort before whole files, then by the prior backup map file they read and the offset within it. This lets
the prior block maps be read from the repo in one combined, in-order pass (see StorageReadMulti) rather than opening and seeking
each one separately, and it keeps files in the same relative order from backup to backup so the same read pattern benefits future
backups and restore. Whole files sort by size so the smallest sit nearest the block incremental data and benefit from read over.
The rationale for each ordering step is in the inline comments below.
***********************************************************************************************************************************/
static int
backupFileComparator(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    const BackupFile *const file1 = item1;
    const BackupFile *const file2 = item2;

    // Order block incremental files before whole files. This produces slightly smaller maps since the offsets are smaller. Also
    // whole files can have reads combined and read over more often without block maps/lists in between them.
    const bool file1BlockIncr = file1->blockIncrSize != 0;
    const bool file2BlockIncr = file2->blockIncrSize != 0;

    if (file1BlockIncr != file2BlockIncr)
        FUNCTION_TEST_RETURN(INT, file1BlockIncr ? -1 : 1);

    // If both files are block incremental order by the prior map file and then the prior map offset. The map file encodes the
    // reference and prior bundle, so ordering on it groups reads on the same file together (and is exactly what the multi-read uses
    // to identify a file), while the offset orders the reads within that file so they can be combined and benefit from read over.
    // Files with no prior map (NULL) sort first and contribute no reads.
    if (file1BlockIncr)
    {
        // Order by prior map file so reads to the same file are grouped
        const int compare = strCmp(file1->blockIncrMapPriorFile, file2->blockIncrMapPriorFile);

        if (compare != 0)
            FUNCTION_TEST_RETURN(INT, compare);

        // Files sharing a prior map file are ordered by map offset so reads in the repo are ordered and more likely to be combined
        // and benefit from read over. Distinct files have non-overlapping map regions so their offsets are never equal here. Files
        // with no prior map have offset 0 and fall through to the size ordering below.
        if (file1->blockIncrMapPriorFile != NULL)
        {
            ASSERT(file1->blockIncrMapPriorOffset != file2->blockIncrMapPriorOffset);
            FUNCTION_TEST_RETURN(INT, file1->blockIncrMapPriorOffset < file2->blockIncrMapPriorOffset ? -1 : 1);
        }
    }

    // Order by size ascending so the smaller whole files sort nearest the block incremental maps (which will be stored at the end
    // of the block incremental data in the future). Both tend to be small so keeping them together benefits read over on restore.
    // Ascending also makes new block incremental maps slightly smaller since the first offset of each reference is stored in full,
    // so files placed earlier get smaller offsets.
    if (file1->pgFileSize < file2->pgFileSize)
        FUNCTION_TEST_RETURN(INT, -1);
    else if (file1->pgFileSize > file2->pgFileSize)
        FUNCTION_TEST_RETURN(INT, 1);

    // If all the above are the same then use name asc to generate a deterministic ordering (names must be unique)
    ASSERT(!strEq(file1->pgFile, file2->pgFile));
    FUNCTION_TEST_RETURN(INT, strCmp(file1->pgFile, file2->pgFile));
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerResult *
backupFileProtocol(PackRead *const param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);

    ProtocolServerResult *const result = protocolServerResultNewP();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Backup options that apply to all files
        const String *const repoFile = pckReadStrP(param);
        const uint64_t bundleId = pckReadU64P(param);
        const bool bundleRaw = bundleId != 0 ? pckReadBoolP(param) : false;
        const unsigned int blockIncrReference = (unsigned int)pckReadU64P(param);
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const int repoFileCompressLevel = pckReadI32P(param);
        const CipherType cipherType = (CipherType)pckReadU64P(param);
        const String *const cipherPass = pckReadStrP(param);
        const PgPageSize pageSize = pckReadU32P(param);
        const String *const pgVersionForce = pckReadStrP(param);

        // Build the file list
        List *const fileList = lstNewP(sizeof(BackupFile), .comparator = backupFileComparator);

        while (!pckReadNullP(param))
        {
            BackupFile file = {.pgFile = pckReadStrP(param)};
            file.pgFileDelta = pckReadBoolP(param);
            file.pgFileIgnoreMissing = pckReadBoolP(param);
            file.pgFileSize = pckReadU64P(param);
            file.pgFileSizeOriginal = pckReadU64P(param);
            file.pgFileCopyExactSize = pckReadBoolP(param);
            file.pgFileChecksum = pckReadBinP(param);
            file.pgFileChecksumPage = pckReadBoolP(param);
            file.pgFilePageHeaderCheck = pckReadBoolP(param);
            file.blockIncrSize = (size_t)pckReadU64P(param);

            if (file.blockIncrSize > 0)
            {
                file.blockIncrChecksumSize = (size_t)pckReadU64P(param);
                file.blockIncrSuperSize = pckReadU64P(param);
                file.blockIncrMapPriorFile = pckReadStrP(param);

                if (file.blockIncrMapPriorFile != NULL)
                {
                    file.blockIncrMapPriorOffset = pckReadU64P(param);
                    file.blockIncrMapPriorSize = pckReadU64P(param);
                }
            }

            file.manifestFile = pckReadStrP(param);
            file.repoFileChecksum = pckReadBinP(param);
            file.repoFileSize = pckReadU64P(param);
            file.manifestFileResume = pckReadBoolP(param);
            file.manifestFileHasReference = pckReadBoolP(param);

            lstAdd(fileList, &file);
        }

        // Sort files for efficient processing
        lstSort(fileList, sortOrderAsc);

        // Backup file
        const List *const resultList = backupFile(
            repoFile, bundleId, bundleRaw, blockIncrReference, repoFileCompressType, repoFileCompressLevel, cipherType, cipherPass,
            pgVersionForce, pageSize, fileList);

        // Return result
        PackWrite *const data = protocolServerResultData(result);

        for (unsigned int resultIdx = 0; resultIdx < lstSize(resultList); resultIdx++)
        {
            const BackupFileResult *const fileResult = lstGet(resultList, resultIdx);

            ASSERT(
                fileResult->backupCopyResult == backupCopyResultSkip || fileResult->copySize != 0 ||
                bufEq(fileResult->copyChecksum, HASH_TYPE_SHA1_ZERO_BUF));

            pckWriteStrP(data, fileResult->manifestFile);
            pckWriteU32P(data, fileResult->backupCopyResult);
            pckWriteBoolP(data, fileResult->repoInvalid);
            pckWriteU64P(data, fileResult->copySize);
            pckWriteU64P(data, fileResult->bundleOffset);
            pckWriteU64P(data, fileResult->blockIncrMapSize);
            pckWriteU64P(data, fileResult->repoSize);
            pckWriteBinP(data, fileResult->copyChecksum);
            pckWriteBinP(data, fileResult->repoChecksum);
            pckWritePackP(data, fileResult->pageChecksumResult);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER_RESULT, result);
}
