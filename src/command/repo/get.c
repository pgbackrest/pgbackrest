/***********************************************************************************************************************************
Repository Get Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

#include "info/infoArchive.h"
#include "info/infoBackup.h"

/***********************************************************************************************************************************
Write source file to destination IO
***********************************************************************************************************************************/
int
storageGetProcess(IoWrite *destination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_READ, destination);
    FUNCTION_LOG_END();

    // Get source file
    const String *file = NULL;

    if (strLstSize(cfgCommandParam()) == 1)
        file = strLstGet(cfgCommandParam(), 0);
    else
        THROW(ParamRequiredError, "source file required");

    // Assume the file is missing
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoRead *source = storageReadIo(storageNewReadP(storageRepo(), file, .ignoreMissing = cfgOptionBool(cfgOptIgnoreMissing)));

        // Add decryption if needed
        if (!cfgOptionBool(cfgOptRaw))
        {
            CipherType repoCipherType = cipherType(cfgOptionStr(cfgOptRepoCipherType));

            if (repoCipherType != cipherTypeNone)
            {
                // Check for a passphrase parameter
                const String *cipherPass = cfgOptionStrNull(cfgOptCipherPass);

                // If not passed as a parameter, find the passphrase
                if (cipherPass == NULL)
                {
                    // If backup.info or archive.info, use the config repo passphrase
                    if (strEndsWithZ(file, INFO_BACKUP_FILE) ||
                        strEndsWithZ(file, INFO_BACKUP_FILE".copy") ||
                        strEndsWithZ(file, INFO_ARCHIVE_FILE) ||
                        strEndsWithZ(file, INFO_ARCHIVE_FILE".copy"))
                    {
                        cipherPass = cfgOptionStr(cfgOptRepoCipherPass);
                    }
                    else
                    {                  
                        // Find out if we look for a file in archive or in backup repo
                        const String *storagePathArchive = storagePathP(storageRepo(), STORAGE_PATH_ARCHIVE_STR);
                        if (strBeginsWith(file, storagePathArchive) || strBeginsWith(file, STORAGE_PATH_ARCHIVE_STR))
                        {
                            // Guess the stanza name
                            StringList *tmpSplitLst;
                            if (strBeginsWith(file, FSLASH_STR))
                                tmpSplitLst = strLstNewSplit(strSub(file, strSize(storagePathArchive) + 1), FSLASH_STR);
                            else
                                tmpSplitLst = strLstNewSplit(strSub(file, strSize(STORAGE_PATH_ARCHIVE_STR) + 1), FSLASH_STR);

                            const String *stanza = strLstGet(tmpSplitLst, 0);
                            InfoArchive *info = infoArchiveLoadFile(
                                storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strPtr(stanza), INFO_ARCHIVE_FILE),
                                repoCipherType, cfgOptionStr(cfgOptRepoCipherPass));
                            cipherPass = strDup(infoArchiveCipherPass(info));
                        }
                        
                        const String *storagePathBackup = storagePathP(storageRepo(), STORAGE_PATH_BACKUP_STR);
                        if (strBeginsWith(file, storagePathBackup) || strBeginsWith(file, STORAGE_PATH_BACKUP_STR))
                        {
                            // Guess the stanza name
                            StringList *tmpSplitLst;
                            if (strBeginsWith(file, FSLASH_STR))
                                tmpSplitLst = strLstNewSplit(strSub(file, strSize(storagePathBackup) + 1), FSLASH_STR);
                            else
                                tmpSplitLst = strLstNewSplit(strSub(file, strSize(STORAGE_PATH_BACKUP_STR) + 1), FSLASH_STR);

                            const String *stanza = strLstGet(tmpSplitLst, 0);
                            InfoBackup *info = infoBackupLoadFile(
                                storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strPtr(stanza), INFO_BACKUP_FILE),
                                repoCipherType, cfgOptionStr(cfgOptRepoCipherPass));
                            cipherPass = strDup(infoBackupCipherPass(info));
                        }
                    }
                }

                // Add encryption filter
                cipherBlockFilterGroupAdd(ioReadFilterGroup(source), repoCipherType, cipherModeDecrypt, cipherPass);
            }
        }

        // Open source
        if (ioReadOpen(source))
        {
            // Open the destination file now that we know the source exists and is readable
            ioWriteOpen(destination);

            // Copy data from source to destination
            Buffer *buffer = bufNew(ioBufferSize());

            do
            {
                ioRead(source, buffer);
                ioWrite(destination, buffer);
                bufUsedZero(buffer);
            }
            while (!ioReadEof(source));

            // Close the source and destination
            ioReadClose(source);
            ioWriteClose(destination);

            // Source file exists
            result = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
int
cmdStorageGet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Assume the file is missing
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            result = storageGetProcess(ioHandleWriteNew(STRDEF("stdout"), STDOUT_FILENO));
        }
        // Ignore write errors because it's possible (even likely) that this output is being piped to something like head which
        // will exit when it gets what it needs and leave us writing to a broken pipe.  It would be better to just ignore the broken
        // pipe error but currently we don't store system error codes.
        CATCH(FileWriteError)
        {
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
