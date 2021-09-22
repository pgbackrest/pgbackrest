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

/**********************************************************************************************************************************/
VerifyResult
verifyFile(
    const String *filePathName, const String *fileChecksum, uint64_t fileSize, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, filePathName);                   // Fully qualified file name
        FUNCTION_LOG_PARAM(STRING, fileChecksum);                   // Checksum for the file
        FUNCTION_LOG_PARAM(UINT64, fileSize);                       // Size of file
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
    FUNCTION_LOG_END();

    ASSERT(filePathName != NULL);
    ASSERT(fileChecksum != NULL);

    // Is the file valid?
    VerifyResult result = verifyOk;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepare the file for reading
        IoRead *read = storageReadIo(storageNewReadP(storageRepo(), filePathName, .ignoreMissing = true));
        IoFilterGroup *filterGroup = ioReadFilterGroup(read);

        // Add decryption filter
        if (cipherPass != NULL)
            ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));

        // Add decompression filter
        if (compressTypeFromName(filePathName) != compressTypeNone)
            ioFilterGroupAdd(filterGroup, decompressFilter(compressTypeFromName(filePathName)));

        // Add sha1 filter
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

        // Add size filter
        ioFilterGroupAdd(filterGroup, ioSizeNew());

        // Add IoSink so the file data is not transmitted from the remote
        ioFilterGroupAdd(filterGroup, ioSinkNew());

        // If the file exists check the checksum/size
        if (ioReadDrain(read))
        {
            // Validate checksum
            if (!strEq(fileChecksum, pckReadStrP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE))))
            {
                result = verifyChecksumMismatch;
            }
            // If the size can be checked, do so
            else if (fileSize != pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE)))
                result = verifySizeInvalid;
        }
        else
            result = verifyFileMissing;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
