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
// Copy a file from the PostgreSQL data directory to the repository
typedef struct BackupFile
{
    const String *pgFile;                                           // Pg file to backup
    bool pgFileDelta;                                               // Checksum pg file before copying
    bool pgFileIgnoreMissing;                                       // Ignore missing pg file
    uint64_t pgFileSize;                                            // Expected pg file size
    bool pgFileCopyExactSize;                                       // Copy only pg expected size
    const String *pgFileChecksum;                                   // Expected pg file checksum
    bool pgFileChecksumPage;                                        // Validate page checksums?
    uint64_t blockIncrSize;                                         // Perform block incremental on this file?
    const String *blockIncrMapFile;                                 // File containing the block incremental map (NULL if none)
    uint64_t blockIncrMapOffset;                                    // Offset of block incremental map
    uint64_t blockIncrMapSize;                                      // Size of block incremental map
    const String *manifestFile;                                     // Repo file
    bool manifestFileResume;                                        // Checksum repo file before copying
    bool manifestFileHasReference;                                  // Reference to prior backup, if any
} BackupFile;

typedef struct BackupFileResult
{
    const String *manifestFile;                                     // Manifest file
    BackupCopyResult backupCopyResult;
    uint64_t copySize;
    String *copyChecksum;
    uint64_t bundleOffset;                                          // Offset in bundle if any
    uint64_t repoSize;
    uint64_t blockIncrMapSize;                                      // Size of block incremental map (0 if no map)
    Pack *pageChecksumResult;
} BackupFileResult;

List *backupFile(
    const String *repoFile, uint64_t bundleId, unsigned int blockIncrReference, CompressType repoFileCompressType,
    int repoFileCompressLevel, CipherType cipherType, const String *cipherPass, const List *fileList);

#endif
