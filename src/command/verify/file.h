/***********************************************************************************************************************************
Verify File
***********************************************************************************************************************************/
#ifndef COMMAND_VERIFY_FILE_H
#define COMMAND_VERIFY_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
File result
***********************************************************************************************************************************/
typedef enum
{
    verifyOk,                                                       // Default result - file OK
    verifyFileMissing,
    verifyChecksumMismatch,
    verifySizeInvalid,
    verifyOtherError,
} VerifyResult;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Verify a file in the pgBackRest repository
VerifyResult verifyFile(
    const String *filePathName, uint64_t bundleId, uint64_t bundleOffset, uint64_t bundleSize, const String *fileChecksum,
    uint64_t fileSize, const String *cipherPass);

#endif
