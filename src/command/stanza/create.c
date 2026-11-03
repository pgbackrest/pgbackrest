/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include <build.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "command/check/common.h"
#include "command/control/common.h"
#include "command/stanza/common.h"
#include "command/stanza/create.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/format/format.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Does the passphrase look randomly generated? It must be hex, base64, or unpadded base64url that decodes to at least 256 bits, the
decoded bytes must not all be text, and there must be enough distinct byte values and enough distinct differences between adjacent
bytes. Too few distinct differences means repeated values, a sequence, or a short repeating pattern. 32 random bytes are all text
with a probability of about 10^-14 and have fewer than 16 distinct values or differences with a probability of about 10^-15. The
passphrase could still be a weak secret that was hashed or encoded some other way, which we can't detect.
***********************************************************************************************************************************/
// Bytes the key must decode to, i.e. 256 bits
#define CIPHER_PASS_GENERATED_SIZE_MIN                              32

// Distinct byte values and distinct differences between adjacent bytes the key must contain
#define CIPHER_PASS_GENERATED_DISTINCT_MIN                          16

// Prefix of the passphrase examples in the documentation
#define CIPHER_PASS_EXAMPLE_PREFIX                                  "zWaf6XtpjIVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGi"

// Encodings to try in order. A hex passphrase may also be valid base64.
static const EncodingType cipherPassGeneratedEncoding[] = {encodingHex, encodingBase64, encodingBase64Url};

static bool
cipherPassGenerated(const CipherSpec *const cipherSpec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_TEST_END();

    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const pass = strNewBuf(cipherSpecPass(cipherSpec));
        const Buffer *key = NULL;

        // Decode with the first encoding the passphrase is valid in
        for (unsigned int encodingIdx = 0; encodingIdx < LENGTH_OF(cipherPassGeneratedEncoding) && key == NULL; encodingIdx++)
        {
            TRY_BEGIN()
            {
                key = bufNewDecode(cipherPassGeneratedEncoding[encodingIdx], pass);
            }
            // The passphrase is not valid in this encoding
            CATCH(FormatError)
            {
            }
            TRY_END();
        }

        if (key != NULL && bufUsed(key) >= CIPHER_PASS_GENERATED_SIZE_MIN)
        {
            bool text = true;
            bool byteFound[256] = {false};
            unsigned int byteDistinct = 0;
            bool deltaFound[256] = {false};
            unsigned int deltaDistinct = 0;

            for (size_t keyIdx = 0; keyIdx < bufUsed(key); keyIdx++)
            {
                const uint8_t byte = bufPtrConst(key)[keyIdx];

                // A byte that is neither printable ASCII nor a tab, LF, or CR means the key is not text
                if ((byte < ' ' || byte > '~') && byte != '\t' && byte != '\n' && byte != '\r')
                    text = false;

                // Count each byte value the first time it is seen
                if (!byteFound[byte])
                {
                    byteFound[byte] = true;
                    byteDistinct++;
                }

                // Count each difference from the prior byte the first time it is seen
                if (keyIdx > 0)
                {
                    const uint8_t delta = (uint8_t)(byte - bufPtrConst(key)[keyIdx - 1]);

                    if (!deltaFound[delta])
                    {
                        deltaFound[delta] = true;
                        deltaDistinct++;
                    }
                }
            }

            result =
                !text && byteDistinct >= CIPHER_PASS_GENERATED_DISTINCT_MIN &&
                deltaDistinct >= CIPHER_PASS_GENERATED_DISTINCT_MIN;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStanzaCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify that a stop was not issued before proceeding
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptForce))
            LOG_WARN("option --" CFGOPT_FORCE " is no longer supported");

        // Get the version and system information - validating it if the database is online
        const PgControl pgControl = pgValidate();

        // For each repository configured, create the stanza
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(
                CFGCMD_STANZA_CREATE " for stanza '%s' on %s", strZ(cfgOptionDisplay(cfgOptStanza)),
                cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            const Storage *const storageRepoReadStanza = storageRepoIdx(repoIdx);
            const Storage *const storageRepoWriteStanza = storageRepoIdxWrite(repoIdx);
            InfoArchive *infoArchive = NULL;
            InfoBackup *infoBackup = NULL;

            const bool archiveInfoFileExists = storageExistsP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR);
            const bool archiveInfoFileCopyExists = storageExistsP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_COPY_STR);
            const bool backupInfoFileExists = storageExistsP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR);
            const bool backupInfoFileCopyExists = storageExistsP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_COPY_STR);

            // If neither archive info nor backup info files exist and nothing else exists in the stanza directory
            // then create the stanza
            if (!archiveInfoFileExists && !archiveInfoFileCopyExists && !backupInfoFileExists && !backupInfoFileCopyExists)
            {
                const bool archiveNotEmpty = strLstSize(
                    storageListP(storageRepoReadStanza, STORAGE_REPO_ARCHIVE_STR)) > 0 ? true : false;
                const bool backupNotEmpty = strLstSize(
                    storageListP(storageRepoReadStanza, STORAGE_REPO_BACKUP_STR)) > 0 ? true : false;

                // If something else exists in the backup or archive directories for this stanza, then error
                if (archiveNotEmpty || backupNotEmpty)
                {
                    THROW_FMT(
                        PathNotEmptyError, "%s%s%snot empty", (backupNotEmpty ? "backup directory " : ""),
                        (backupNotEmpty && archiveNotEmpty ? "and/or " : ""), (archiveNotEmpty ? "archive directory " : ""));
                }

                // Format for the new stanza
                const unsigned int format = cfgOptionIdxUInt(cfgOptRepoFormat, repoIdx);

                // Check the passphrase of a new encrypted stanza
                if (cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx) != cipherTypeNone)
                {
                    // Error when the passphrase is a documentation example
                    if (strBeginsWithZ(cfgOptionIdxStr(cfgOptRepoCipherPass, repoIdx), CIPHER_PASS_EXAMPLE_PREFIX))
                    {
                        THROW_FMT(
                            OptionInvalidValueError,
                            "option '%s' must not be the example passphrase from the documentation\n"
                            "HINT: generate a passphrase with 'openssl rand -base64 32'.",
                            cfgOptionIdxName(cfgOptRepoCipherPass, repoIdx));
                    }

                    // Format >= 6 requires the passphrase to look generated
                    if (format >= REPOSITORY_FORMAT_6 && !cipherPassGenerated(cfgCipherSpecMainIdx(repoIdx)))
                    {
                        THROW_FMT(
                            OptionInvalidValueError,
                            "option '%s' must be at least 256 random bits encoded as hex, base64, or base64url to create a stanza"
                            " at repository format " STRINGIFY(REPOSITORY_FORMAT_6) " or higher\n"
                            "HINT: generate a passphrase with 'openssl rand -base64 32'.",
                            cfgOptionIdxName(cfgOptRepoCipherPass, repoIdx));
                    }
                }

                // If the repo is encrypted, generate a cipher passphrase for encrypting archive files. Format >= 6 adds it by
                // rotating to the first key id and earlier formats store it as the default key with no id.
                const CipherSpec *const cipherSpecArchive = cipherSpecGen(cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), format);
                CipherSpecMap *const cipherSpecMapArchive = cipherSpecMapNew();

                if (cipherSpecType(cipherSpecArchive) != cipherTypeNone && format < REPOSITORY_FORMAT_6)
                    cipherSpecMapAdd(cipherSpecMapArchive, CIPHER_SPEC_MAP_ID_DEFAULT_STR, cipherSpecArchive);

                // Create and save archive info
                infoArchive = infoArchiveNew(pgControl.version, pgControl.systemId, format, cipherSpecMapArchive);

                if (cipherSpecType(cipherSpecArchive) != cipherTypeNone && format >= REPOSITORY_FORMAT_6)
                    infoArchiveCipherRotate(infoArchive, cipherSpecArchive, time(NULL));

                infoArchiveSaveFile(infoArchive, storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));

                // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent manifests. Format >= 6 adds a
                // key for each backup set when the full backup is created.
                const CipherSpec *const cipherSpecManifest =
                    format < REPOSITORY_FORMAT_6 ? cipherSpecGen(cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), format) : NULL;

                // Create and save backup info
                infoBackup = infoBackupNew(
                    pgControl.version, pgControl.systemId, pgControl.catalogVersion, format, cipherSpecManifest);

                infoBackupSaveFile(infoBackup, storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));
            }
            // Else if at least one archive and one backup info file exists, then ensure both are valid
            else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
            {
                // Error if there is a mismatch between the archive and backup info files or the database version/system Id matches
                // current database
                checkStanzaInfoPg(
                    repoIdx, storageRepoReadStanza, pgControl.version, pgControl.systemId, cfgCipherSpecMainIdx(repoIdx));

                // The files are valid - upgrade
                const String *sourceFile = NULL;
                const String *destinationFile = NULL;

                // If the existing files are valid, then, if a file is missing, copy the existing one to the missing one to ensure
                // there is both a .info and .info.copy
                if (!archiveInfoFileExists || !archiveInfoFileCopyExists)
                {
                    sourceFile = archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;
                    destinationFile = !archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;

                    // Using get and put instead of copy in case the storage is remote
                    storagePutP(
                        storageNewWriteP(storageRepoWriteStanza, destinationFile),
                        storageGetP(storageNewReadP(storageRepoReadStanza, sourceFile)));
                }

                if (!backupInfoFileExists || !backupInfoFileCopyExists)
                {
                    sourceFile = backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;
                    destinationFile = !backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;

                    // Using get and put instead of copy in case the storage is remote
                    storagePutP(
                        storageNewWriteP(storageRepoWriteStanza, destinationFile),
                        storageGetP(storageNewReadP(storageRepoReadStanza, sourceFile)));
                }

                // If no files copied, then the stanza was already valid
                if (sourceFile == NULL)
                {
                    LOG_INFO_FMT(
                        "stanza '%s' already exists on %s and is valid", strZ(cfgOptionDisplay(cfgOptStanza)),
                        cfgOptionGroupName(cfgOptGrpRepo, repoIdx));
                }
            }
            // Else if both .info and corresponding .copy file are missing for one but not the other, then error - the user will
            // have to make a conscious effort to determine if deleting the stanza on this repo is appropriate or other action is
            else
            {
                THROW_FMT(
                    FileMissingError,
                    "%s on %s\n"
                    "HINT: this may be a symptom of repository corruption!",
                    (archiveInfoFileExists || archiveInfoFileCopyExists) ?
                        "archive.info exists but backup.info is missing" : "backup.info exists but archive.info is missing",
                    cfgOptionGroupName(cfgOptGrpRepo, repoIdx));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
