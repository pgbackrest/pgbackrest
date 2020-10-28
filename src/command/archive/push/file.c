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

/**********************************************************************************************************************************/
String *
archivePushFile(
    const String *walSource, unsigned int pgVersion, uint64_t pgSystemId, const String *archiveFile,
    const ArchivePushFileRepoData *repoData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSource);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM_P(VOID, repoData);
    FUNCTION_LOG_END();

    ASSERT(walSource != NULL);
    ASSERT(archiveFile != NULL);
    ASSERT(repoData != NULL);

    String *result = NULL;

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

        // Get wal segment checksum and compare it to what exists in the repo, if any
        String *walSegmentFile = NULL;

        if (isSegment)
        {
            // Generate a sha1 checksum for the wal segment
            IoRead *read = storageReadIo(storageNewReadP(storageLocal(), walSource));
            ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
            ioReadDrain(read);

            const String *walSegmentChecksum = varStr(ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR));

            for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
            {
                // If the wal segment already exists in the repo then compare checksums
                walSegmentFile = walSegmentFind(storageRepoIdx(repoIdx), repoData[repoIdx].archiveId, archiveFile, 0);

                if (walSegmentFile != NULL)
                {
                    String *walSegmentRepoChecksum = strSubN(walSegmentFile, strSize(archiveFile) + 1, HASH_TYPE_SHA1_SIZE_HEX);

                    if (strEq(walSegmentChecksum, walSegmentRepoChecksum))
                    {
                        MEM_CONTEXT_PRIOR_BEGIN()
                        {
                            result = strNewFmt(
                                "WAL file '%s' already exists in the archive with the same checksum"
                                    "\nHINT: this is valid in some recovery scenarios but may also indicate a problem.",
                                strZ(archiveFile));
                        }
                        MEM_CONTEXT_PRIOR_END();
                    }
                    else
                        THROW_FMT(ArchiveDuplicateError, "WAL file '%s' already exists in the archive", strZ(archiveFile));
                }
            }

            // Append the checksum to the archive destination
            strCatFmt(archiveDestination, "-%s", strZ(walSegmentChecksum));
        }

        // Only copy if the file was not found in the archive
        if (walSegmentFile == NULL)
        {
            for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
            {
                StorageRead *source = storageNewReadP(storageLocal(), walSource);

                // Is the file compressible during the copy?
                bool compressible = true;

                // If the file will be compressed then add compression filter
                if (isSegment && repoData[repoIdx].compressType != compressTypeNone)
                {
                    compressExtCat(archiveDestination, repoData[repoIdx].compressType);
                    ioFilterGroupAdd(
                        ioReadFilterGroup(
                            storageReadIo(source)), compressFilter(repoData[repoIdx].compressType,
                            repoData[repoIdx].compressLevel));
                    compressible = false;
                }

                // If there is a cipher then add the encrypt filter
                if (repoData[repoIdx].cipherType != cipherTypeNone)
                {
                    ioFilterGroupAdd(
                        ioReadFilterGroup(storageReadIo(source)),
                        cipherBlockNew(
                            cipherModeEncrypt, repoData[repoIdx].cipherType, BUFSTR(repoData[repoIdx].cipherPass), NULL));
                    compressible = false;
                }

                // Copy the file
                // NEED A CUSTOM COPY HERE TO WRITE EACH BUFFER TO ALL REPOS
                storageCopyP(
                    source,
                    storageNewWriteP(
                        storageRepoIdxWrite(repoIdx),
                        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(repoData[repoIdx].archiveId), strZ(archiveDestination)),
                    .compressible = compressible));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
