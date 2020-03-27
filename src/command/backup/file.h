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
typedef struct BackupFileResult
{
    BackupCopyResult backupCopyResult;
    uint64_t copySize;
    String *copyChecksum;
    uint64_t repoSize;
    KeyValue *pageChecksumResult;
} BackupFileResult;

BackupFileResult backupFile(
    const String *pgFile, bool pgFileIgnoreMissing, uint64_t pgFileSize, const String *pgFileChecksum, bool pgFileChecksumPage,
    uint64_t pgFileChecksumPageLsnLimit, const String *repoFile, bool repoFileHasReference, CompressType repoFileCompressType,
    int repoFileCompressLevel, const String *backupLabel, bool delta, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BACKUP_FILE_RESULT_TYPE                                                                                       \
    BackupFileResult
#define FUNCTION_LOG_BACKUP_FILE_RESULT_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(&value, "BackupFileResult", buffer, bufferSize)

#endif
