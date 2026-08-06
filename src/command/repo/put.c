/***********************************************************************************************************************************
Repository Put Command
***********************************************************************************************************************************/
#include <build.h>

#include <unistd.h>

#include "command/repo/common.h"
#include "command/repo/put.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Write source IO to destination file
***********************************************************************************************************************************/
static void
storagePutProcess(IoRead *source)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, source);
    FUNCTION_LOG_END();

    // Get destination file
    const String *file = NULL;

    if (strLstSize(cfgCommandParam()) == 1)
        file = strLstGet(cfgCommandParam(), 0);
    else
        THROW(ParamRequiredError, "destination file required");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is path valid for repo?
        file = repoPathIsValid(file);

        StorageWrite *const destination = storageNewWriteP(storageRepoWrite(), file);

        // Add encryption if needed
        if (!cfgOptionBool(cfgOptRaw))
        {
            const CipherType repoCipherType = cfgOptionStrId(cfgOptRepoCipherType);

            if (repoCipherType != cipherTypeNone)
            {
                // Check for a passphrase parameter, otherwise use the repo cipher spec
                const String *const cipherPassParam = cfgOptionStrNull(cfgOptCipherPass);
                // Derive as a file with no header is derived, since that is what this command writes. An info file at format 6
                // or above carries a header that says otherwise and is not written here.
                const CipherSpec *const cipherSpec = cipherSpecNewP(
                    repoCipherType,
                    cipherPassParam == NULL ? cipherSpecPass(cfgCipherSpec()) : BUFSTR(cipherPassParam),
                    .digest = infoFormatDigest(REPOSITORY_FORMAT_5));

                // Add encryption filter
                cipherBlockFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), cipherModeEncrypt, cipherSpec);
            }
        }

        // Open source and destination
        ioReadOpen(source);
        ioWriteOpen(storageWriteIo(destination));

        // Copy data from source to destination
        ioCopyP(source, storageWriteIo(destination));

        // Close the source and destination
        ioReadClose(source);
        ioWriteClose(storageWriteIo(destination));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStoragePut(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storagePutProcess(ioFdReadNew(STRDEF("stdin"), STDIN_FILENO, ioTimeoutMs()));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
