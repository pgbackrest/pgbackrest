/***********************************************************************************************************************************
Backup File
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_FILE_H
#define COMMAND_BACKUP_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Backup file types
***********************************************************************************************************************************/
typedef enum
{
    backupCopyResultChecksum,
    backupCopyResultCopy,
    backupCopyResultReCopy,
    backupCopyResultSkip,
    backupCopyResultNoOp,
} BackupCopyResult;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the archive to the specified destination
typedef struct BackupFile
{
    const String *pgFile;                                           // Pg file to backup
    bool pgFileIgnoreMissing;                                       // Ignore missing pg file
    uint64_t pgFileSize;                                            // Expected pg file size
    bool pgFileCopyExactSize;                                       // Copy only pg expected size
    const String *pgFileChecksum;                                   // Expected pg file checksum
    bool pgFileChecksumPage;                                        // Validate page checksums?
    uint64_t pgFileChecksumPageLsnLimit;                            // Upper limit of pages to validate
    const String *repoFile;                                         // Repo file
    bool repoFileHasReference;                                      // Reference to prior backup, if any
} BackupFile;

// Copy a file from the PostgreSQL data directory to the repository
typedef struct BackupFileResult
{
    BackupCopyResult backupCopyResult;
    uint64_t copySize;
    String *copyChecksum;
    uint64_t repoSize;
    Pack *pageChecksumResult;
} BackupFileResult;

List *backupFile(
    CompressType repoFileCompressType, int repoFileCompressLevel, const String *backupLabel, bool delta, CipherType cipherType,
    const String *cipherPass, const List *fileList);

#endif
