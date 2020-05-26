/***********************************************************************************************************************************
Verify File
***********************************************************************************************************************************/
#ifndef COMMAND_VERIFY_FILE_H
#define COMMAND_VERIFY_FILE_H

// #include "common/compress/helper.h"
// #include "common/crypto/common.h"
#include "common/type/string.h"
// #include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Verify a file in the repo
bool verifyFile(const String *repoFile);

// CSHANG Not sure yet what this looks like
    // const String *repoFile, const String *repoFileReference, CompressType repoFileCompressType, const String *pgFile,
    // const String *pgFileChecksum, bool pgFileZero, uint64_t pgFileSize, time_t pgFileModified, mode_t pgFileMode,
    // const String *pgFileUser, const String *pgFileGroup, time_t copyTimeBegin, bool delta, bool deltaForce,
    // const String *cipherPass

#endif
