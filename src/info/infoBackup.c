/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/ini.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/info.h"
#include "info/infoBackup.h"
#include "info/infoManifest.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
??? INFO_BACKUP_SECTION should be in a separate include since it will also be used when reading the manifest
***********************************************************************************************************************************/
#define INFO_BACKUP_SECTION                                         "backup"
#define INFO_BACKUP_SECTION_BACKUP_CURRENT                          INFO_BACKUP_SECTION ":current"

STRING_EXTERN(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR,            INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE);
STRING_EXTERN(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR,      INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA);
STRING_EXTERN(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR,                 INFO_BACKUP_KEY_BACKUP_INFO_SIZE);
STRING_EXTERN(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR,           INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA);
STRING_EXTERN(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR,                 INFO_BACKUP_KEY_BACKUP_REFERENCE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    MemContext *memContext;                                         // Context that contains the InfoBackup
    InfoPg *infoPg;                                                 // Contents of the DB data
    List *backup;                                                   // List of current backups and their associated data
};


/***********************************************************************************************************************************
Create a new InfoBackup object
// ??? Need loadFile parameter
// ??? Need to handle ignoreMissing = true
***********************************************************************************************************************************/
InfoBackup *
infoBackupNew(const Storage *storage, const String *fileName, bool ignoreMissing, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);
        FUNCTION_DEBUG_PARAM(ENUM, cipherType);
        // cipherPass omitted for security

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoBackup")
    {
        // Create object
        this = memNew(sizeof(InfoBackup));
        this->memContext = MEM_CONTEXT_NEW();

        // Catch file missing error and add backup-specific hints before rethrowing
        TRY_BEGIN()
        {
            this->infoPg = infoPgNew(storage, fileName, infoPgBackup, cipherType, cipherPass);
        }
        CATCH_ANY()
        {
            THROWP_FMT(
                errorType(),
                "%s\n"
                "HINT: backup.info cannot be opened and is required to perform a backup.\n"
                "HINT: has a stanza-create been performed?",
                errorMessage());
        }
        TRY_END();

        const Ini *infoIni = infoPgIni(this->infoPg);
        const String *backupCurrentSection = strNew(INFO_BACKUP_SECTION_BACKUP_CURRENT);

        // If there are current backups, then parse the json for each into a list object
        if (strLstExists(iniSectionList(infoIni), backupCurrentSection))
        {
            // Initialize the store and get the list of backup labels
            this->backup = lstNew(sizeof(InfoBackupData));
            const StringList *backupLabelList = iniSectionKeyList(infoIni, backupCurrentSection);

            // For each backup label, store the information
            for (unsigned int backupLabelIdx = 0; backupLabelIdx < strLstSize(backupLabelList); backupLabelIdx++)
            {
                const String *backupLabelKey = strLstGet(backupLabelList, backupLabelIdx);
                const KeyValue *backupKv = jsonToKv(varStr(iniGet(infoIni, backupCurrentSection, backupLabelKey)));

                InfoBackupData infoBackupData =
                {
                    .backupLabel = strDup(backupLabelKey),
                    .backrestFormat = varIntForce(kvGet(backupKv, varNewStr(INFO_KEY_FORMAT_STR))),
                    .backrestVersion = varStr(kvGet(backupKv, varNewStr(INFO_KEY_VERSION_STR))),
                    .backupArchiveStart = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR))),
                    .backupArchiveStop = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR))),
                    .backupInfoRepoSize = varUInt64Force(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR))),
                    .backupInfoRepoSizeDelta = varUInt64Force(
                        kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR))),
                    .backupInfoSize = varUInt64Force(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR))),
                    .backupInfoSizeDelta = varUInt64Force(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR))),
                    .backupPrior = varStr(kvGet(backupKv, varNewStr(INFO_KEY_VERSION_STR))),
                    .backupReference = strLstNewVarLst(
                        varVarLst(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR)))),
                    .backupType = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_TYPE_STR))),
                    .backupPgId = cvtZToUInt(strPtr(varStr(kvGet(backupKv, varNewStr(INFO_KEY_DB_ID_STR))))),
                };

                // Add the backup data to the list
                lstAdd(this->backup, &infoBackupData);
            }
        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Get the list of keys (backup labels) from the backup current list
***********************************************************************************************************************************/
StringList *
infoBackupCurrentKeyGet(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (this->backupCurrent != NULL)
        {
            result = strLstNewVarLst(kvKeyList(this->backupCurrent));
            strLstMove(result, MEM_CONTEXT_OLD());
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(STRING_LIST, result);
}


/***********************************************************************************************************************************
Get a value of a key from a specific backup in the backup current list
***********************************************************************************************************************************/
const Variant *
infoBackupCurrentGet(const InfoBackup *this, const String *section, const String *key)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_BACKUP, this);
        FUNCTION_DEBUG_PARAM(STRING, section);
        FUNCTION_DEBUG_PARAM(STRING, key);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(section != NULL);
        FUNCTION_DEBUG_ASSERT(key != NULL);
    FUNCTION_DEBUG_END();

    const Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the section
        KeyValue *sectionKv = varKv(kvGet(this->backupCurrent, varNewStr(section)));

        // Section must exist to get the value
        if (sectionKv != NULL)
            result = kvGet(sectionKv, varNewStr(key));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(CONST_VARIANT, result);
}

/***********************************************************************************************************************************
Checks the backup info file's DB section against the PG version, system id, catolog and constrol version passed in and returns
the history id of the current PG database.
// ??? Should we still check that the file exists if it is required?
***********************************************************************************************************************************/
unsigned int
infoBackupCheckPg(
    const InfoBackup *this,
    unsigned int pgVersion,
    uint64_t pgSystemId,
    uint32_t pgCatalogVersion,
    uint32_t pgControlVersion)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_BACKUP, this);
        FUNCTION_DEBUG_PARAM(UINT, pgVersion);
        FUNCTION_DEBUG_PARAM(UINT64, pgSystemId);
        FUNCTION_DEBUG_PARAM(UINT32, pgCatalogVersion);
        FUNCTION_DEBUG_PARAM(UINT32, pgControlVersion);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    InfoPgData backupPg = infoPgDataCurrent(this->infoPg);

    if (backupPg.version != pgVersion || backupPg.systemId != pgSystemId)
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database version = %s, system-id %" PRIu64 " does not match backup version = %s, system-id = %" PRIu64 "\n"
            "HINT: is this the correct stanza?",
            strPtr(pgVersionToStr(pgVersion)), pgSystemId, strPtr(pgVersionToStr(backupPg.version)), backupPg.systemId)));

    if (backupPg.catalogVersion != pgCatalogVersion || backupPg.controlVersion != pgControlVersion)
    {
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database control-version = %" PRIu32 ", catalog-version %" PRIu32
            " does not match backup control-version = %" PRIu32 ", catalog-version = %" PRIu32 "\n"
            "HINT: this may be a symptom of database or repository corruption!",
            pgControlVersion, pgCatalogVersion, backupPg.controlVersion, backupPg.catalogVersion)));
    }

    FUNCTION_DEBUG_RESULT(UINT, backupPg.id);
}

/***********************************************************************************************************************************
Get PostgreSQL info
***********************************************************************************************************************************/
InfoPg *
infoBackupPg(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INFO_PG, this->infoPg);
}

/***********************************************************************************************************************************
Get pointer to list of current backups
***********************************************************************************************************************************/
InfoBackupData *
infoBackupDataList(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INFO_BACKUP_DATAP, this->backup);
}

/***********************************************************************************************************************************
Return a structure of the backup data from a specific index
***********************************************************************************************************************************/
InfoBackupData
infoBackupData(const InfoBackup *this, unsigned int backupDataIdx)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
        FUNCTION_DEBUG_PARAM(UINT, backupDataIdx);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INFO_BACKUP_DATA, *((InfoBackupData *)lstGet(this->backup, backupDataIdx)));
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoBackupFree(InfoBackup *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_BACKUP, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
