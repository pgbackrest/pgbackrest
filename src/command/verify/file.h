/***********************************************************************************************************************************
Verify File
***********************************************************************************************************************************/
#ifndef COMMAND_VERIFY_FILE_H
#define COMMAND_VERIFY_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Verify file types
***********************************************************************************************************************************/
// typedef enum  // CSHANG don't think we need
// {
//     verifyFileArchive,
//     verifyFileBackup,
// } VerifyFileType;

/***********************************************************************************************************************************
File result
***********************************************************************************************************************************/
typedef enum
{
    verifyOk,                                                       // default result - file OK
    verifyFileMissing,
    verifyChecksumMismatch,
    verifySizeInvalid,
    verifyOtherError,
} VerifyResult;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Verify a file in the pgBackRest repository
typedef struct VerifyFileResult
{
    VerifyResult fileResult;
    String *filePathName;
    // VerifyFileType fileType;  // CSHANG Don't think we need
} VerifyFileResult;

VerifyResult verifyFile(
    const String *filePathName, const String *fileChecksum, bool sizeCheck, uint64_t fileSize, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_VERIFY_FILE_RESULT_TYPE                                                                                       \
    VerifyFileResult
#define FUNCTION_LOG_VERIFY_FILE_RESULT_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(&value, "VerifyFileResult", buffer, bufferSize)

#endif
