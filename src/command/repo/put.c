/***********************************************************************************************************************************
Repository Put Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/handleRead.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Write source IO to destination file
***********************************************************************************************************************************/
void
storagePutProcess(IoRead *source)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
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
        StorageWrite *destination = storageNewWriteP(storageRepoWrite(), file);

        // Add encryption if needed
        if (!cfgOptionBool(cfgOptRaw))
        {
            CipherType repoCipherType = cipherType(cfgOptionStr(cfgOptRepoCipherType));

            if (repoCipherType != cipherTypeNone)
            {
                // Check for a passphrase parameter
                const String *cipherPass = cfgOptionStrNull(cfgOptCipherPass);

                // If not passed as a parameter use the repo passphrase
                if (cipherPass == NULL)
                    cipherPass = cfgOptionStr(cfgOptRepoCipherPass);

                // Add encryption filter
                cipherBlockFilterGroupAdd(
                    ioWriteFilterGroup(storageWriteIo(destination)), repoCipherType, cipherModeEncrypt, cipherPass);
            }
        }

        // Open source and destination
        ioReadOpen(source);
        ioWriteOpen(storageWriteIo(destination));

        // Copy data from source to destination
        Buffer *buffer = bufNew(ioBufferSize());

        do
        {
            ioRead(source, buffer);
            ioWrite(storageWriteIo(destination), buffer);
            bufUsedZero(buffer);
        }
        while (!ioReadEof(source));

        // Close the source and destination
        ioReadClose(source);
        ioWriteClose(storageWriteIo(destination));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdStoragePut(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storagePutProcess(ioHandleReadNew(STRDEF("stdin"), STDIN_FILENO, ioTimeoutMs()));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
