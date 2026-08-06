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
#include "config/load.h"
#include "storage/helper.h"

#include "info/info.h"
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

        const CipherSpec *cipherSpecFile = NULL;                    // Cipher spec the file is encrypted with
        unsigned int formatInfo = 0;                                // Format when the file is an info file, else zero

        // Add decryption if needed
        if (!cfgOptionBool(cfgOptRaw))
        {
            const CipherType repoCipherType = cfgOptionStrId(cfgOptRepoCipherType);

            if (repoCipherType != cipherTypeNone)
            {
                // Check for a passphrase parameter. Derive it as a file with no header is derived, since a file put here with a
                // passphrase of its own was written the same way.
                const String *const cipherPassParam = cfgOptionStrNull(cfgOptCipherPass);
                const CipherSpec *cipherSpec =
                    cipherPassParam == NULL ?
                        NULL :
                        cipherSpecNewP(
                            repoCipherType, BUFSTR(cipherPassParam), .digest = infoFormatDigest(REPOSITORY_FORMAT_5));

                // If not passed as a parameter then determine the passphrase using the following pattern:
                //
                // REPO / (repo passphrase)
                //      / archive / (repo passphrase)
                //      / archive / stanza / (archive passphrase)
                //      / backup  / (repo passphrase)
                //      / backup  / stanza / (backup passphrase)
                //      / backup  / stanza / set / (manifest passphrase)
                //      / backup  / stanza / backup.history / (backup passphrase)
                //
                // Nothing should be stored at the top level of the repo except the backup/archive paths. The backup/archive paths
                // should contain only stanza paths.
                // -----------------------------------------------------------------------------------------------------------------
                if (cipherSpec == NULL)
                {
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
                            cipherSpec = cfgCipherSpec();

                            // Find the archive passphrase
                            if (!strEndsWithZ(file, INFO_ARCHIVE_FILE) && !strEndsWithZ(file, INFO_ARCHIVE_FILE INFO_COPY_EXT))
                            {
                                const InfoArchive *const info = infoArchiveLoadFile(
                                    storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), INFO_ARCHIVE_FILE),
                                    cipherSpec);
                                cipherSpec = infoArchiveCipherSpec(info);
                            }
                            // Else the file is the archive info, which is loaded to find the format it was written at
                            else
                                formatInfo = infoArchiveFormat(infoArchiveLoadFile(storageRepo(), file, cipherSpec));
                        }

                        // Backup path
                        if (strEq(strLstGet(filePathSplitLst, 0), STORAGE_PATH_BACKUP_STR))
                        {
                            cipherSpec = cfgCipherSpec();

                            if (!strEndsWithZ(file, INFO_BACKUP_FILE) && !strEndsWithZ(file, INFO_BACKUP_FILE INFO_COPY_EXT))
                            {
                                // Find the backup passphrase
                                const InfoBackup *const info = infoBackupLoadFile(
                                    storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanza), INFO_BACKUP_FILE),
                                    cipherSpec);
                                cipherSpec = infoBackupCipherSpec(info);

                                // Find the manifest passphrase
                                if (!strEq(strLstGet(filePathSplitLst, 2), STRDEF(BACKUP_PATH_HISTORY)) &&
                                    !strEndsWithZ(file, BACKUP_MANIFEST_FILE) &&
                                    !strEndsWithZ(file, BACKUP_MANIFEST_FILE INFO_COPY_EXT))
                                {
                                    const Manifest *const manifest = manifestLoadFile(
                                        storageRepo(),
                                        strNewFmt(
                                            STORAGE_PATH_BACKUP "/%s/%s/%s", strZ(stanza), strZ(strLstGet(filePathSplitLst, 2)),
                                            BACKUP_MANIFEST_FILE), cipherSpec);
                                    cipherSpec = manifestCipherSpecSub(manifest);
                                }
                            }
                            // Else the file is the backup info, which is loaded to find the format it was written at
                            else
                                formatInfo = infoBackupFormat(infoBackupLoadFile(storageRepo(), file, cipherSpec));
                        }
                    }
                }

                // Error when unable to determine cipher passphrase
                if (cipherSpec == NULL)
                    THROW_FMT(OptionInvalidValueError, "unable to determine cipher passphrase for '%s'", strZ(file));

                // An info file is read from behind the eight bytes in front of its salt, which are the header when it has one and
                // the magic the cipher writes when it does not. The format the load found says what the pass derives with.
                if (formatInfo != 0)
                {
                    cipherSpec = cipherSpecNewP(
                        cipherSpecType(cipherSpec), cipherSpecPass(cipherSpec), .digest = infoFormatDigest(formatInfo));
                }

                cipherSpecFile = cipherSpec;
            }
        }

        // Create new file read, starting past the header when the file is an info file
        IoRead *const source = storageReadIo(
            storageNewReadP(
                storageRepo(), file, .ignoreMissing = cfgOptionBool(cfgOptIgnoreMissing),
                .offset = formatInfo != 0 ? CIPHER_BLOCK_MAGIC_SIZE : 0));

        // Add decryption filter
        if (cipherSpecFile != NULL)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(source), cipherBlockNewP(cipherModeDecrypt, cipherSpecFile, .raw = formatInfo != 0));
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
