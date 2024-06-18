/***********************************************************************************************************************************
Backup File
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_FILE_H
#define COMMAND_BACKUP_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/type/keyValue.h"
#include "postgres/interface.h"

/***********************************************************************************************************************************
Backup file types
***********************************************************************************************************************************/
typedef enum
{
    backupCopyResultChecksum,
    backupCopyResultCopy,
    backupCopyResultSkip,
    backupCopyResultNoOp,
    backupCopyResultTruncate,
} BackupCopyResult;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the PostgreSQL data directory to the repository
typedef struct BackupFile
{
    const String *pgFile;                                           // Pg file to backup
    bool pgFileDelta;                                               // Checksum pg file before copying
    bool pgFileIgnoreMissing;                                       // Ignore missing pg file
    uint64_t pgFileSize;                                            // Expected pg file size
    uint64_t pgFileSizeOriginal;                                    // Original size (from manifest build)
    bool pgFileCopyExactSize;                                       // Copy only pg expected size
    const Buffer *pgFileChecksum;                                   // Expected pg file checksum
    bool pgFileChecksumPage;                                        // Validate page checksums?
    bool pgFilePageHeaderCheck;                                     // Validate page headers?
    size_t blockIncrSize;                                           // Perform block incremental on this file?
    size_t blockIncrChecksumSize;                                   // Block checksum size
    uint64_t blockIncrSuperSize;                                    // Size of the super block
    const String *blockIncrMapPriorFile;                            // File containing prior block incremental map (NULL if none)
    uint64_t blockIncrMapPriorOffset;                               // Offset of prior block incremental map
    uint64_t blockIncrMapPriorSize;                                 // Size of prior block incremental map
    const String *manifestFile;                                     // Repo file
    const Buffer *repoFileChecksum;                                 // Expected repo file checksum
    uint64_t repoFileSize;                                          // Expected repo file size
    bool manifestFileResume;                                        // Checksum repo file before copying
    bool manifestFileHasReference;                                  // Reference to prior backup, if any
} BackupFile;

typedef struct BackupFileResult
{
    const String *manifestFile;                                     // Manifest file
    BackupCopyResult backupCopyResult;
    bool repoInvalid;                                               // File was recopied because repo file was invalid
    uint64_t copySize;
    const Buffer *copyChecksum;                                     // Checksum of pg file
    const Buffer *repoChecksum;                                     // Checksum of repo file (including compression, etc.)
    uint64_t bundleOffset;                                          // Offset in bundle if any
    uint64_t repoSize;
    uint64_t blockIncrMapSize;                                      // Size of block incremental map (0 if no map)
    Pack *pageChecksumResult;
} BackupFileResult;

FN_EXTERN List *backupFile(
    const String *repoFile, uint64_t bundleId, bool bundleRaw, unsigned int blockIncrReference, CompressType repoFileCompressType,
    int repoFileCompressLevel, CipherType cipherType, const String *cipherPass, const String *pgVersionForce, PgPageSize pageSize,
    const List *fileList);

#endif
