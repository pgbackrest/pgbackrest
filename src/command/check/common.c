/***********************************************************************************************************************************
Check Common Handler
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

#include "command/check/common.h"
#include "common/debug.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "postgres/interface.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Helper function
***********************************************************************************************************************************/
static bool
checkArchiveCommand(const String *const archiveCommand)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, archiveCommand);
    FUNCTION_TEST_END();

    bool result = archiveCommand != NULL;

    if (result && strstr(strZ(archiveCommand), PROJECT_BIN) == NULL)
        result = false;

    if (!result)
    {
        THROW_FMT(
            ArchiveCommandInvalidError, "archive_command '%s' must contain %s",
            archiveCommand != NULL ? strZ(archiveCommand) : "[" NULL_Z "]", PROJECT_BIN);
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
checkDbConfig(const unsigned int pgVersion, const unsigned int pgIdx, const Db *const dbObject, const bool isStandby)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, pgIdx);
        FUNCTION_TEST_PARAM(DB, dbObject);
        FUNCTION_TEST_PARAM(BOOL, isStandby);
    FUNCTION_TEST_END();

    ASSERT(dbObject != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Error if the version from the control file and the configured pg-path do not match the values obtained from the database
        const unsigned int dbVersion = dbPgVersion(dbObject);
        const String *const dbPath = dbPgDataPath(dbObject);

        if (pgVersion != dbVersion || strCmp(cfgOptionIdxStr(cfgOptPgPath, pgIdx), dbPath) != 0)
        {
            THROW_FMT(
                DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
                PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\nHINT: the %s and %s settings likely reference different clusters.",
                strZ(pgVersionToStr(dbVersion)), strZ(dbPath), strZ(pgVersionToStr(pgVersion)),
                strZ(cfgOptionIdxDisplay(cfgOptPgPath, pgIdx)), strZ(cfgOptionIdxDisplay(cfgOptPgPath, pgIdx)),
                cfgOptionIdxName(cfgOptPgPath, pgIdx), cfgOptionIdxName(cfgOptPgPort, pgIdx));
        }

        // Check archive configuration if option is valid for the command and set
        if (!isStandby && cfgOptionValid(cfgOptArchiveCheck) && cfgOptionBool(cfgOptArchiveCheck))
        {
            // Error if archive_mode = off since backup start will fail
            if (strCmpZ(dbArchiveMode(dbObject), "off") == 0)
            {
                THROW(ArchiveDisabledError, "archive_mode must be enabled");
            }

            // Error if archive_mode = always unless check is disabled (support has not been added yet)
            if (cfgOptionBool(cfgOptArchiveModeCheck) && strCmpZ(dbArchiveMode(dbObject), "always") == 0)
            {
                THROW(FeatureNotSupportedError, "archive_mode=always not supported");
            }

            // Check if archive_command is set and is valid
            checkArchiveCommand(dbArchiveCommand(dbObject));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
checkStanzaInfo(const unsigned int repoIdx, const InfoPg *const archiveInfoPg, const InfoPg *const backupInfoPg)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(INFO_PG, archiveInfoPg);
        FUNCTION_TEST_PARAM(INFO_PG, backupInfoPg);
    FUNCTION_TEST_END();

    ASSERT(archiveInfoPg != NULL);
    ASSERT(backupInfoPg != NULL);

    const InfoPgData archiveInfo = infoPgData(archiveInfoPg, infoPgDataCurrentId(archiveInfoPg));
    const InfoPgData backupInfo = infoPgData(backupInfoPg, infoPgDataCurrentId(backupInfoPg));

    // Error if there is a mismatch between the archive and backup info files
    if (archiveInfo.id != backupInfo.id || archiveInfo.systemId != backupInfo.systemId ||
        archiveInfo.version != backupInfo.version)
    {
        THROW_FMT(
            FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = %u, version = %s, system-id = %" PRIu64 "\n"
            "backup : id = %u, version = %s, system-id = %" PRIu64 "\n"
            "HINT: this may be a symptom of repository corruption!",
            archiveInfo.id, strZ(pgVersionToStr(archiveInfo.version)), archiveInfo.systemId, backupInfo.id,
            strZ(pgVersionToStr(backupInfo.version)), backupInfo.systemId);
    }

    // Error if the info files are at different repository formats. The formats are written together but stored apart, so an
    // upgrade interrupted between the two saves leaves them mismatched.
    if (infoPgFormat(archiveInfoPg) != infoPgFormat(backupInfoPg))
    {
        const unsigned int formatArchive = infoPgFormat(archiveInfoPg);
        const unsigned int formatBackup = infoPgFormat(backupInfoPg);

        THROW_FMT(
            FileInvalidError,
            "backup info file and archive info file are at different repository formats\n"
            "archive: format = %u\n"
            "backup : format = %u\n"
            "HINT: run " CFGCMD_STANZA_UPGRADE " with --%s=%u to complete an interrupted upgrade.",
            formatArchive, formatBackup, cfgOptionIdxName(cfgOptRepoFormat, repoIdx),
            formatArchive > formatBackup ? formatArchive : formatBackup);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
checkStanzaInfoPg(
    const unsigned int repoIdx, const Storage *const storage, const unsigned int pgVersion, const uint64_t pgSystemId,
    const CipherSpec *const cipherSpecMain)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(STORAGE, storage);
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT64, pgSystemId);
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpecMain);
    FUNCTION_TEST_END();

    ASSERT(storage != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check that the backup and archive info files exist
        const InfoArchive *const infoArchive = infoArchiveLoadFile(storage, INFO_ARCHIVE_PATH_FILE_STR, cipherSpecMain);
        const InfoPgData archiveInfoPg = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));
        const InfoBackup *const infoBackup = infoBackupLoadFile(storage, INFO_BACKUP_PATH_FILE_STR, cipherSpecMain);

        // Check that the info files pg data and repository format match each other
        checkStanzaInfo(repoIdx, infoArchivePg(infoArchive), infoBackupPg(infoBackup));

        // Check that the version and system id match the current database
        if (pgVersion != archiveInfoPg.version || pgSystemId != archiveInfoPg.systemId)
        {
            THROW(
                FileInvalidError,
                "backup and archive info files exist but do not match the database\n"
                "HINT: is this the correct stanza?\n"
                "HINT: did an error occur during stanza-upgrade?");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}
