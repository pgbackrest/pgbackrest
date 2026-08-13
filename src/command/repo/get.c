/***********************************************************************************************************************************
Repository Get Command
***********************************************************************************************************************************/
#include <build.h>

#include <unistd.h>

#include "command/repo/common.h"
#include "command/repo/get.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
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
static int
storageGetProcess(IoWrite *const destination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, destination);
    FUNCTION_LOG_END();

    // Get source file
    if (strLstSize(cfgCommandParam()) != 1)
        THROW(ParamRequiredError, "source file required");

    const String *file = strLstGet(cfgCommandParam(), 0);

    // Assume the file is missing
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is path valid for repo?
        file = repoPathIsValid(file);

        // Create new file read
        IoRead *const source = storageReadIo(
            storageNewReadP(storageRepo(), file, .ignoreMissing = cfgOptionBool(cfgOptIgnoreMissing)));

        // Add decryption if needed
        if (!cfgOptionBool(cfgOptRaw))
        {
            const CipherType repoCipherType = cfgOptionStrId(cfgOptRepoCipherType);

            if (repoCipherType != cipherTypeNone)
            {
                // Determine the passphrase using the following pattern:
                //
                // REPO / (main passphrase)
                //      / archive / (main passphrase)
                //      / archive / stanza / (archive passphrase)
                //      / backup  / (main passphrase)
                //      / backup  / stanza / (manifest passphrase)
                //      / backup  / stanza / set / (backup passphrase)
                //      / backup  / stanza / backup.history / (manifest passphrase)
                //
                // Nothing should be stored at the top level of the repo except the backup/archive paths. The backup/archive paths
                // should contain only stanza paths.
                // -----------------------------------------------------------------------------------------------------------------
                const CipherSpec *cipherSpec = NULL;
                const StringList *const filePathSplitLst = strLstNewSplit(file, FSLASH_STR);

                // At a minimum the path must contain archive/backup, a stanza, and a file
                if (strLstSize(filePathSplitLst) > 2)
                {
                    const String *const stanza = strLstGet(filePathSplitLst, 1);

                    // If stanza option is specified then it must match the given file path
                    if (cfgOptionStrNull(cfgOptStanza) != NULL && !strEq(stanza, cfgOptionStr(cfgOptStanza)))
                    {
                        THROW_FMT(
                            OptionInvalidValueError, "stanza name '%s' given in option doesn't match the given path",
                            strZ(cfgOptionDisplay(cfgOptStanza)));
                    }

                    // Archive path
                    if (strEq(strLstGet(filePathSplitLst, 0), STORAGE_PATH_ARCHIVE_STR))
                    {
                        cipherSpec = cfgCipherSpecMain();

                        // Find the archive passphrase
                        if (!strEndsWithZ(file, INFO_ARCHIVE_FILE) && !strEndsWithZ(file, INFO_ARCHIVE_FILE INFO_COPY_EXT))
                        {
                            const InfoArchive *const info = infoArchiveLoadFile(
                                storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), INFO_ARCHIVE_FILE),
                                cfgCipherSpecMain());
                            cipherSpec = infoArchiveCipherSpec(info);
                        }
                    }

                    // Backup path
                    if (strEq(strLstGet(filePathSplitLst, 0), STORAGE_PATH_BACKUP_STR))
                    {
                        cipherSpec = cfgCipherSpecMain();

                        if (!strEndsWithZ(file, INFO_BACKUP_FILE) && !strEndsWithZ(file, INFO_BACKUP_FILE INFO_COPY_EXT))
                        {
                            // Find the manifest passphrase
                            const InfoBackup *const info = infoBackupLoadFile(
                                storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanza), INFO_BACKUP_FILE),
                                cfgCipherSpecMain());
                            const CipherSpec *const cipherSpecManifest = infoBackupCipherSpec(info);

                            // Find the backup passphrase if not a manifest
                            if (!strEq(strLstGet(filePathSplitLst, 2), STRDEF(BACKUP_PATH_HISTORY)) &&
                                !strEndsWithZ(file, BACKUP_MANIFEST_FILE) &&
                                !strEndsWithZ(file, BACKUP_MANIFEST_FILE INFO_COPY_EXT))
                            {
                                const Manifest *const manifest = manifestLoadFile(
                                    storageRepo(),
                                    strNewFmt(
                                        STORAGE_PATH_BACKUP "/%s/%s/%s", strZ(stanza), strZ(strLstGet(filePathSplitLst, 2)),
                                        BACKUP_MANIFEST_FILE),
                                    cipherSpecManifest);
                                cipherSpec = manifestCipherSpec(manifest);
                            }
                            // Else use the manifest passphrase
                            else
                                cipherSpec = cipherSpecManifest;
                        }
                    }
                }

                // Error when unable to determine cipher passphrase
                if (cipherSpec == NULL)
                    THROW_FMT(OptionInvalidValueError, "unable to determine cipher passphrase for '%s'", strZ(file));

                ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);

                // Add encryption filter
                cipherBlockFilterGroupAdd(ioReadFilterGroup(source), cipherModeDecrypt, cipherSpec);
            }
        }

        // Open source
        if (ioReadOpen(source))
        {
            // Open the destination file now that we know the source exists and is readable
            ioWriteOpen(destination);

            // Copy data from source to destination
            ioCopyP(source, destination);

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
FN_EXTERN int
cmdStorageGet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Assume the file is missing
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            result = storageGetProcess(ioFdWriteNew(STRDEF("stdout"), STDOUT_FILENO, cfgOptionUInt64(cfgOptIoTimeout)));
        }
        // Ignore write errors because it's possible (even likely) that this output is being piped to something like head which will
        // exit when it gets what it needs and leave us writing to a broken pipe. It would be better to just ignore the broken pipe
        // error but currently we don't store system error codes.
        CATCH(FileWriteError)
        {
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
