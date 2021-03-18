/***********************************************************************************************************************************
Archive Push File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/push/file.h"
#include "command/archive/common.h"
#include "command/control/common.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Catch write errors during processing

We want to continue when there are write errors during processing so add them to a list to be reported later and return false so the
caller knows to stop writing on the affected repo.
***********************************************************************************************************************************/
typedef enum
{
    archivePushFileIoTypeOpen,
    archivePushFileIoTypeWrite,
    archivePushFileIoTypeClose,
} ArchivePushFileIoType;

static bool
archivePushFileIo(ArchivePushFileIoType type, IoWrite *write, const Buffer *buffer, unsigned int repoIdx, StringList *errorList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(IO_WRITE, write);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(STRING_LIST, errorList);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);
    ASSERT(errorList != NULL);

    bool result = true;

    // Process write operation
    TRY_BEGIN()
    {
        switch (type)
        {
            case archivePushFileIoTypeOpen:
                ioWriteOpen(write);
                break;

            case archivePushFileIoTypeWrite:
                ASSERT(buffer != NULL);
                ioWrite(write, buffer);
                break;

            case archivePushFileIoTypeClose:
                ioWriteClose(write);
                break;
        }
    }
    // Handle errors
    CATCH_ANY()
    {
        strLstAdd(
            errorList,
            strNewFmt(
                "repo%u: [%s] %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), errorTypeName(errorType()), errorMessage()));
        result = false;
    }
    TRY_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
ArchivePushFileResult
archivePushFile(
    const String *walSource, unsigned int pgVersion, uint64_t pgSystemId, const String *archiveFile, CompressType compressType,
    int compressLevel, const List *const repoList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSource);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(ENUM, compressType);
        FUNCTION_LOG_PARAM(INT, compressLevel);
        FUNCTION_LOG_PARAM_P(VOID, repoList);
    FUNCTION_LOG_END();

    ASSERT(walSource != NULL);
    ASSERT(archiveFile != NULL);
    ASSERT(repoList != NULL);
    ASSERT(lstSize(repoList) > 0);

    ArchivePushFileResult result = {.warnList = strLstNew()};
    StringList *errorList = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a WAL segment?
        bool isSegment = walIsSegment(archiveFile);

        // If this is a segment compare archive version and systemId to the WAL header
        if (isSegment)
        {
            PgWal walInfo = pgWalFromFile(walSource, storageLocal());

            if (walInfo.version != pgVersion || walInfo.systemId != pgSystemId)
            {
                THROW_FMT(
                    ArchiveMismatchError,
                    "WAL file '%s' version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
                    strZ(walSource), strZ(pgVersionToStr(walInfo.version)), walInfo.systemId, strZ(pgVersionToStr(pgVersion)),
                    pgSystemId);
            }
        }

        // Set archive destination initially to the archive file, this will be updated later for wal segments
        String *archiveDestination = strDup(archiveFile);

        // Assume that all repos need a copy of the archive file
        bool destinationCopyAny = true;
        bool *destinationCopy = memNew(sizeof(bool) * lstSize(repoList));

        for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
            destinationCopy[repoListIdx] = true;

        // Get wal segment checksum and compare it to what exists in the repo, if any
        if (isSegment)
        {
            // Assume that no repos need a copy of the WAL segment and update when a repo needing a copy is found
            destinationCopyAny = false;

            // Generate a sha1 checksum for the wal segment
            IoRead *read = storageReadIo(storageNewReadP(storageLocal(), walSource));
            ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
            ioReadDrain(read);

            const String *walSegmentChecksum = varStr(ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR));

            // Check each repo for the WAL segment
            for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
            {
                const ArchivePushFileRepoData *const repoData = lstGet(repoList, repoListIdx);

                // If the wal segment already exists in the repo then compare checksums
                const String *walSegmentFile = walSegmentFind(
                    storageRepoIdx(repoData->repoIdx), repoData->archiveId, archiveFile, 0);

                if (walSegmentFile != NULL)
                {
                    String *walSegmentRepoChecksum = strSubN(walSegmentFile, strSize(archiveFile) + 1, HASH_TYPE_SHA1_SIZE_HEX);

                    // If the checksums are the same then succeed but warn in case this is a symptom of some other issue
                    if (strEq(walSegmentChecksum, walSegmentRepoChecksum))
                    {
                        MEM_CONTEXT_PRIOR_BEGIN()
                        {
                            // Add warning to the result that will be returned to the main process
                            strLstAdd(
                                result.warnList,
                                strNewFmt(
                                    "WAL file '%s' already exists in the repo%u archive with the same checksum"
                                    "\nHINT: this is valid in some recovery scenarios but may also indicate a problem.",
                                    strZ(archiveFile), cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoData->repoIdx)));
                        }
                        MEM_CONTEXT_PRIOR_END();

                        // No need to copy to this repo
                        destinationCopy[repoData->repoIdx] = false;
                    }
                    // Else error so we don't overwrite the existing segment
                    else
                    {
                        THROW_FMT(
                            ArchiveDuplicateError, "WAL file '%s' already exists in the repo%u archive with a different checksum",
                            strZ(archiveFile), cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoData->repoIdx));
                    }
                }
                // Else the repo needs a copy
                else
                    destinationCopyAny = true;
            }

            // Append the checksum to the archive destination
            strCatFmt(archiveDestination, "-%s", strZ(walSegmentChecksum));
        }

        // Copy the file if one or more repos require it
        if (destinationCopyAny)
        {
            // Source file is read once and copied to all repos
            StorageRead *source = storageNewReadP(storageLocal(), walSource);

            // Is the file compressible during the copy?
            bool compressible = true;

            // If the file will be compressed then add compression filter
            if (isSegment && compressType != compressTypeNone)
            {
                compressExtCat(archiveDestination, compressType);
                ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(source)), compressFilter(compressType, compressLevel));
                compressible = false;
            }

            // Initialize per-repo destination files
            StorageWrite **destination = memNew(sizeof(StorageWrite *) * lstSize(repoList));

            for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
            {
                const ArchivePushFileRepoData *const repoData = lstGet(repoList, repoListIdx);

                // Does this repo need a copy?
                if (destinationCopy[repoData->repoIdx])
                {
                    // Create destination file
                    destination[repoData->repoIdx] = storageNewWriteP(
                        storageRepoIdxWrite(repoData->repoIdx),
                        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(repoData->archiveId), strZ(archiveDestination)),
                        .compressible = compressible);

                    // If there is a cipher then add the encrypt filter
                    if (repoData->cipherType != cipherTypeNone)
                    {
                        ioFilterGroupAdd(
                            ioWriteFilterGroup(storageWriteIo(destination[repoData->repoIdx])),
                            cipherBlockNew(cipherModeEncrypt, repoData->cipherType, BUFSTR(repoData->cipherPass), NULL));
                    }
                }
            }

            // Open source file
            ioReadOpen(storageReadIo(source));

            // Open the destination files now that we know the source file exists and is readable
            for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
            {
                const unsigned int repoIdx = ((ArchivePushFileRepoData *)lstGet(repoList, repoListIdx))->repoIdx;

                if (destinationCopy[repoIdx])
                {
                    destinationCopy[repoIdx] = archivePushFileIo(
                        archivePushFileIoTypeOpen, storageWriteIo(destination[repoIdx]), NULL, repoIdx, errorList);
                }
            }

            // Copy data from source to destination
            Buffer *read = bufNew(ioBufferSize());

            do
            {
                // Read from source
                ioRead(storageReadIo(source), read);

                // Write to each destination
                for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
                {
                    const unsigned int repoIdx = ((ArchivePushFileRepoData *)lstGet(repoList, repoListIdx))->repoIdx;

                    if (destinationCopy[repoIdx])
                    {
                        destinationCopy[repoIdx] = archivePushFileIo(
                            archivePushFileIoTypeWrite, storageWriteIo(destination[repoIdx]), read, repoIdx, errorList);
                    }
                }

                // Clear buffer
                bufUsedZero(read);
            }
            while (!ioReadEof(storageReadIo(source)));

            // Close the source and destination files
            ioReadClose(storageReadIo(source));

            for (unsigned int repoListIdx = 0; repoListIdx < lstSize(repoList); repoListIdx++)
            {
                const unsigned int repoIdx = ((ArchivePushFileRepoData *)lstGet(repoList, repoListIdx))->repoIdx;

                if (destinationCopy[repoIdx])
                {
                    destinationCopy[repoIdx] = archivePushFileIo(
                        archivePushFileIoTypeClose, storageWriteIo(destination[repoIdx]), NULL, repoIdx, errorList);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    // Throw any errors, even if some files were successful. It is important that PostgreSQL recieves an error so it does not
    // remove the file.
    if (strLstSize(errorList) > 0)
        THROW_FMT(CommandError, CFGCMD_ARCHIVE_PUSH " command encountered error(s):\n%s", strZ(strLstJoin(errorList, "\n")));

    FUNCTION_LOG_RETURN_STRUCT(result);
}
