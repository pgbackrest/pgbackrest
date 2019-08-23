/***********************************************************************************************************************************
Check Common Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/check/common.h"
#include "common/debug.h"
#include "config/config.h"
#include "db/db.h"
#include "db/helper.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check the database path and version are configured correctly
***********************************************************************************************************************************/
void
checkDbConfig(const unsigned int pgVersion, const unsigned int dbIdx, const Db *dbObject, bool isStandby)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, dbIdx);
        FUNCTION_TEST_PARAM(DB, dbObject);
        FUNCTION_TEST_PARAM(BOOL, isStandby);
    FUNCTION_TEST_END();

    ASSERT(dbIdx > 0);
    ASSERT(dbObject != NULL);

    unsigned int dbVersion = dbPgVersion(dbObject);
    String *dbPath = dbPgDataPath(dbObject);
    unsigned int pgPath = cfgOptPgPath + (dbIdx - 1);

    // Error if the version from the control file and the configured pg-path do not match the values obtained from the database
    if (pgVersion != dbVersion || strCmp(cfgOptionStr(pgPath), dbPath) != 0)
    {
        THROW_FMT(
            DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
            PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\nHINT: the %s and %s settings likely reference different clusters",
            strPtr(pgVersionToStr(dbVersion)), strPtr(dbPath), strPtr(pgVersionToStr(pgVersion)), strPtr(cfgOptionStr(pgPath)),
            strPtr(cfgOptionStr(pgPath)), cfgOptionName(pgPath), cfgOptionName(cfgOptPgPort + (dbIdx - 1)));
    }

    // Check archive configuration if option is valid for the command and set
    if (!isStandby && cfgOptionValid(cfgOptArchiveCheck) && cfgOptionBool(cfgOptArchiveCheck))
    {
        String *archiveMode = dbArchiveMode(dbObject);

        // Error if archive_mode = off since pg_start_backup () will fail
        if (strCmpZ(archiveMode, "off")
        {
            THROW(ArchiveDisabledError, "archive_mode must be enabled");
        }

        // Error if archive_mode = always (support has not been added yet)
        if (strCmpZ(archiveMode, "always")
        {
            THROW(FeatureNotSupportedError, "archive_mode=always not supported");
        }
// CSHANG Must add this in as well...
        // # Check if archive_command is set
        // my $strArchiveCommand = $self->executeSqlOne('show archive_command');
        //
        // if (index($strArchiveCommand, PROJECT_EXE) == -1)
        // {
        //     confess &log(ERROR,
        //         'archive_command ' . (defined($strArchiveCommand) ? "'${strArchiveCommand}'" : '[null]') . ' must contain \'' .
        //         PROJECT_EXE . '\'', ERROR_ARCHIVE_COMMAND_INVALID);
        // }
    }
}

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Validate the archive and backup info files
***********************************************************************************************************************************/
void
checkStanzaInfo(const InfoPgData *archiveInfo, const InfoPgData *backupInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, archiveInfo);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, backupInfo);
    FUNCTION_TEST_END();

    ASSERT(archiveInfo != NULL);
    ASSERT(backupInfo != NULL);

    // Error if there is a mismatch between the archive and backup info files
    if (archiveInfo->id != backupInfo->id || archiveInfo->systemId != backupInfo->systemId ||
        archiveInfo->version != backupInfo->version)
    {
        THROW_FMT(
            FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = %u, version = %s, system-id = %" PRIu64 "\n"
            "backup : id = %u, version = %s, system-id = %" PRIu64 "\n"
            "HINT: this may be a symptom of repository corruption!",
            archiveInfo->id, strPtr(pgVersionToStr(archiveInfo->version)), archiveInfo->systemId, backupInfo->id,
            strPtr(pgVersionToStr(backupInfo->version)), backupInfo->systemId);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Load and validate the database data of the info files against each other and the current database
***********************************************************************************************************************************/
void
checkStanzaInfoPg(
    const Storage *storage, const unsigned int pgVersion, const uint64_t pgSystemId, CipherType cipherType,
    const String *cipherPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_TEST_END();

    ASSERT(storage != NULL);

    // Check that the backup and archive info files exist
    InfoArchive *infoArchive = infoArchiveNewLoad(storage, INFO_ARCHIVE_PATH_FILE_STR, cipherType, cipherPass);
    InfoPgData archiveInfoPg = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));
    InfoBackup *infoBackup = infoBackupNewLoad(storage, INFO_BACKUP_PATH_FILE_STR, cipherType, cipherPass);
    InfoPgData backupInfoPg = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

    // Check that the info files pg data match each other
    checkStanzaInfo(&archiveInfoPg, &backupInfoPg);

    // Check that the version and system id match the current database
    if (pgVersion != archiveInfoPg.version || pgSystemId != archiveInfoPg.systemId)
    {
// CSHANG Might want to consider adding a StanzaConfigError or StanzaMismatchError and catch it to have different hints in stanza commands vs check command
        THROW(FileInvalidError, "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
    }

    FUNCTION_TEST_RETURN_VOID();
}
