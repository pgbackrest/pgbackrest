/***********************************************************************************************************************************
Restore File
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_FILE_H
#define COMMAND_RESTORE_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the backup to the specified destination
bool restoreFile(
    const String *repoFile, unsigned int repoIdx, const String *repoFileReference, CompressType repoFileCompressType,
    const String *pgFile, const String *pgFileChecksum, bool pgFileZero, uint64_t pgFileSize, time_t pgFileModified,
    mode_t pgFileMode, const String *pgFileUser, const String *pgFileGroup, time_t copyTimeBegin, bool delta, bool deltaForce,
    const String *cipherPass);

#endif
