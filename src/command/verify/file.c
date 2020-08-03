/***********************************************************************************************************************************
Verify File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/verify/file.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/io/filter/sink.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "storage/helper.h"

VerifyFileResult
verifyFile(
    const String *filePathName, const String *fileChecksum, bool sizeCheck, uint64_t fileSize, CompressType fileCompressType,
    CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, filePathName);                   // Fully qualified file name
        FUNCTION_LOG_PARAM(STRING, fileChecksum);                   // Checksum that the file should be
        FUNCTION_LOG_PARAM(BOOL, sizeCheck);                        // Can the size be verified?
        FUNCTION_LOG_PARAM(UINT64, fileSize);                       // Size of file (if checkable, else 0)
        FUNCTION_LOG_PARAM(ENUM, fileCompressType);                 // Compress type for file
        FUNCTION_LOG_PARAM(ENUM, cipherType);                       // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
    FUNCTION_LOG_END();

    ASSERT(filePathName != NULL);
    ASSERT(fileChecksum != NULL);

    // Is the file valid?
    VerifyFileResult result = {.filePathName = strDup(filePathName)};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepare the file for reading
        IoRead *read = storageReadIo(storageNewReadP(storageRepo(), filePathName));
        IoFilterGroup *filterGroup = ioReadFilterGroup(read);

        // Add decryption filter
        if (cipherPass != NULL)
            ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
// CSHANG How do I tell if file could not decompress? And what about errors that could be thrown - do I need to catch all those here and try to return something?
        // Add decompression filter
        if (fileCompressType != compressTypeNone)
            ioFilterGroupAdd(filterGroup, decompressFilter(fileCompressType));

        // Add sha1 filter
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

        // Add size filter
        ioFilterGroupAdd(filterGroup, ioSizeNew());

        // Add IoSink so the file data is not transmitted from the remote
        ioFilterGroupAdd(filterGroup, ioSinkNew());
// CSHANG So if it is a WAL file and we get here, it is because at the time the verify command started, we read the archive DIR and found the file - but an expire could run and delete it out from under us, so it's not a problem but we need to indicate it is missing
        // If the file exists check the checksum/size
        if (ioReadDrain(read))
        {
            // Validate checksum
            if (!strEq(fileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))
            {
                result.fileResult = verifyChecksumMismatch;
            }
            // CSHANG does size filter return 0 if file size is 0? Assume it does...but just in case...
            // If the size can be checked, do so
            else if (sizeCheck && fileSize != varUInt64Force(ioFilterGroupResult(ioReadFilterGroup(read), SIZE_FILTER_TYPE_STR)))
            {
                result.fileResult = verifySizeInvalid;
            }
        }
        else
            result.fileResult = verifyFileMissing;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VERIFY_FILE_RESULT, result);
}
