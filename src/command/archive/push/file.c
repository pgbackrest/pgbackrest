/***********************************************************************************************************************************
Archive Push File
***********************************************************************************************************************************/
#include "command/archive/push/file.h"
#include "command/archive/common.h"
#include "command/control/control.h"
#include "common/compress/gzip/common.h"
#include "common/compress/gzip/compress.h"
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
Copy a file from the source to the archive
***********************************************************************************************************************************/
String *
archivePushFile(
    const String *walSource, const String *archiveId, unsigned int pgVersion, uint64_t pgSystemId, const String *archiveFile,
    CipherType cipherType, const String *cipherPass, bool compress, int compressLevel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSource);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(BOOL, compress);
        FUNCTION_LOG_PARAM(INT, compressLevel);
    FUNCTION_LOG_END();

    ASSERT(walSource != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(archiveFile != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a WAL segment?
        bool isSegment = walIsSegment(archiveFile);

        // If this is a segment compare archive version and systemId to the WAL header
        if (isSegment)
        {
            PgWal walInfo = pgWalFromFile(walSource);

            if (walInfo.version != pgVersion || walInfo.systemId != pgSystemId)
            {
                THROW_FMT(
                    ArchiveMismatchError,
                    "WAL file '%s' version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
                    strPtr(walSource), strPtr(pgVersionToStr(walInfo.version)), walInfo.systemId, strPtr(pgVersionToStr(pgVersion)),
                    pgSystemId);
            }
        }

        // Set archive destination initially to the archive file, this will be updated later for wal segments
        String *archiveDestination = strDup(archiveFile);

        // Get wal segment checksum and compare it to what exists in the repo, if any
        String *walSegmentFile = NULL;

        if (isSegment)
        {
            // Generate a sha1 checksum for the wal segment.  ??? Probably need a function in storage for this.
            IoRead *read = storageFileReadIo(storageNewReadNP(storageLocal(), walSource));
            IoFilterGroup *filterGroup = ioFilterGroupAdd(ioFilterGroupNew(), cryptoHashFilter(cryptoHashNew(HASH_TYPE_SHA1_STR)));
            ioReadFilterGroupSet(read, filterGroup);

            Buffer *buffer = bufNew(ioBufferSize());
            ioReadOpen(read);

            do
            {
                ioRead(read, buffer);
                bufUsedZero(buffer);
            }
            while (!ioReadEof(read));

            ioReadClose(read);
            String *walSegmentChecksum = varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR));

            // If the wal segment already exists in the repo then compare checksums
            walSegmentFile = walSegmentFind(storageRepo(), archiveId, archiveFile);

            if (walSegmentFile != NULL)
            {
                String *walSegmentRepoChecksum = strSubN(walSegmentFile, strSize(archiveFile) + 1, HASH_TYPE_SHA1_SIZE_HEX);

                if (strEq(walSegmentChecksum, walSegmentRepoChecksum))
                {
                    memContextSwitch(MEM_CONTEXT_OLD());
                    result = strNewFmt(
                        "WAL file '%s' already exists in the archive with the same checksum"
                            "\nHINT: this is valid in some recovery scenarios but may also indicate a problem.",
                        strPtr(archiveFile));
                    memContextSwitch(MEM_CONTEXT_TEMP());
                }
                else
                    THROW_FMT(ArchiveDuplicateError, "WAL file '%s' already exists in the archive", strPtr(archiveFile));
            }

            // Append the checksum to the archive destination
            strCatFmt(archiveDestination, "-%s", strPtr(walSegmentChecksum));
        }

        // Only copy if the file was not found in the archive
        if (walSegmentFile == NULL)
        {
            StorageFileRead *source = storageNewReadNP(storageLocal(), walSource);

            // Add filters
            IoFilterGroup *filterGroup = ioFilterGroupNew();

            // If the file will be compressed then add compression filter
            if (isSegment && compress)
            {
                strCat(archiveDestination, "." GZIP_EXT);
                ioFilterGroupAdd(filterGroup, gzipCompressFilter(gzipCompressNew(compressLevel, false)));
            }

            // If there is a cipher then add the encrypt filter
            if (cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    filterGroup,
                    cipherBlockFilter(
                        cipherBlockNew(cipherModeEncrypt, cipherType, bufNewStr(cipherPass), NULL)));
            }

            ioReadFilterGroupSet(storageFileReadIo(source), filterGroup);

            // Copy the file
            storageCopyNP(
                source,
                storageNewWriteNP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(archiveDestination))));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
