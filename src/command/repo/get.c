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
                /*******************************************************************************************************************
                Repository encryption design:
                REPO / (repoCipherPass)
                     / archive   / (repoCipherPass)
                     / archive   / stanza / (archiveCipherPass)
                     / backup    / (repoCipherPass)
                     / backup    / stanza / (backupCipherPass)
                     / backup    / stanza / set / (manifestCipherPass)
                     / backup    / stanza / backup.history / (manifestCipherPass)
                *******************************************************************************************************************/

                // Check for a passphrase parameter
                const String *cipherPass = cfgOptionStrNull(cfgOptCipherPass);

                // If not passed as a parameter, find the passphrase
                if (cipherPass == NULL)
                {
                    cipherPass = cfgOptionStr(cfgOptRepoCipherPass);

                    const String *relativeFile = strDup(file);
                    if (strBeginsWith(relativeFile, FSLASH_STR))
                    {
                        if (!strBeginsWith(relativeFile, cfgOptionStr(cfgOptRepoPath)))
                        {
                            THROW_FMT(
                                OptionInvalidValueError, "absolute path '%s' is not in base path '%s'",
                                strPtr(relativeFile), strPtr(cfgOptionStr(cfgOptRepoPath)));
                        }

                        relativeFile = strSub(file, strSize(cfgOptionStr(cfgOptRepoPath)) + 1);
                    }

                    // Encrypted files could be stored directly at the root of the repo if needed
                    // We should then at least be able to determine the archive or backup directory
                    StringList *filePathSplitLst = strLstNewSplit(relativeFile, FSLASH_STR);
                    if (strLstSize(filePathSplitLst) > 1)
                    {
                        const String *stanza = strLstGet(filePathSplitLst, 1);

                        // If stanza option is specified, it must match the given file path
                        if (cfgOptionStrNull(cfgOptStanza) != NULL && !strEq(stanza, cfgOptionStr(cfgOptStanza)))
                        {
                            THROW_FMT(
                                OptionInvalidValueError, "stanza name '%s' given in option doesn't match the given path",
                                strPtr(cfgOptionStr(cfgOptStanza)));
                        }

                        if (strEq(strLstGet(filePathSplitLst, 0), STORAGE_PATH_ARCHIVE_STR))
                        {
                            // Find the archiveCipherPass
                            if (!strEndsWithZ(relativeFile, INFO_ARCHIVE_FILE) &&
                                !strEndsWithZ(relativeFile, INFO_ARCHIVE_FILE".copy"))
                            {
                                InfoArchive *info = infoArchiveLoadFile(
                                    storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strPtr(stanza), INFO_ARCHIVE_FILE),
                                    repoCipherType, cipherPass);
                                cipherPass = infoArchiveCipherPass(info);
                            }
                        }                      
                        else if (strEq(strLstGet(filePathSplitLst, 0), STORAGE_PATH_BACKUP_STR))
                        {
                            if (!strEndsWithZ(relativeFile, INFO_BACKUP_FILE) &&
                                !strEndsWithZ(relativeFile, INFO_BACKUP_FILE".copy"))
                            {
                                // Find the backupCipherPass
                                InfoBackup *info = infoBackupLoadFile(
                                    storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strPtr(stanza), INFO_BACKUP_FILE),
                                    repoCipherType, cipherPass);
                                cipherPass = infoBackupCipherPass(info);

                                // Find the manifestCipherPass
                                if (!strEq(strLstGet(filePathSplitLst, 2), STRDEF("backup.history")) &&
                                    !strEndsWithZ(relativeFile, BACKUP_MANIFEST_FILE) &&
                                    !strEndsWithZ(relativeFile, BACKUP_MANIFEST_FILE".copy"))
                                {
                                    const Manifest *manifest = manifestLoadFile(
                                        storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s/%s", strPtr(stanza), 
                                        strPtr(strLstGet(filePathSplitLst, 2)), BACKUP_MANIFEST_FILE), repoCipherType, cipherPass);
                                    cipherPass = manifestCipherSubPass(manifest);
                                }
                            }
                        }
                        else
                        {
                            // Nothing should be stored at the top level of the repo
                            THROW_FMT(
                                OptionInvalidValueError, "unable to determine encryption key for '%s'", strPtr(relativeFile));
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
