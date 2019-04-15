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
#include "common/regExp.h"
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

STRING_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR,            "backup-info-repo-size");
STRING_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR,      "backup-info-repo-size-delta");
STRING_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR,                 "backup-info-size");
STRING_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR,           "backup-info-size-delta");
STRING_STATIC(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR,                 "backup-reference");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    MemContext *memContext;                                         // Mem context
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
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoBackup")
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
                const KeyValue *backupKv = varKv(jsonToVar(varStr(iniGet(infoIni, backupCurrentSection, backupLabelKey))));

                InfoBackupData infoBackupData =
                {
                    .backrestFormat = (unsigned int)varUInt64(kvGet(backupKv, varNewStr(INFO_KEY_FORMAT_STR))),
                    .backrestVersion = varStrForce(kvGet(backupKv, varNewStr(INFO_KEY_VERSION_STR))),
                    .backupInfoRepoSize = varUInt64(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR))),
                    .backupInfoRepoSizeDelta = varUInt64(
                        kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR))),
                    .backupInfoSize = varUInt64(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR))),
                    .backupInfoSizeDelta = varUInt64(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR))),
                    .backupLabel = strDup(backupLabelKey),
                    .backupPgId = cvtZToUInt(strPtr(varStrForce(kvGet(backupKv, varNewStr(INFO_KEY_DB_ID_STR))))),
                    .backupTimestampStart = varUInt64(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR))),
                    .backupTimestampStop= varUInt64(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR))),
                    .backupType = varStrForce(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_TYPE_STR))),

                    // Possible NULL values
                    .backupArchiveStart = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR))),
                    .backupArchiveStop = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR))),
                    .backupPrior = varStr(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_BACKUP_PRIOR_STR))),
                    .backupReference = (kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR)) != NULL ?
                        strLstNewVarLst(varVarLst(kvGet(backupKv, varNewStr(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR)))) :
                        NULL),

                    // Options
                    .optionArchiveCheck = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_STR))),
                    .optionArchiveCopy = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_STR))),
                    .optionBackupStandby = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_STR))),
                    .optionChecksumPage = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_STR))),
                    .optionCompress = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_COMPRESS_STR))),
                    .optionHardlink = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_HARDLINK_STR))),
                    .optionOnline = varBool(kvGet(backupKv, varNewStr(INFO_MANIFEST_KEY_OPT_ONLINE_STR))),
                };

                // Add the backup data to the list
                lstAdd(this->backup, &infoBackupData);
            }
        }
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
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
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT32, pgCatalogVersion);
        FUNCTION_LOG_PARAM(UINT32, pgControlVersion);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

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

    FUNCTION_LOG_RETURN(UINT, backupPg.id);
}

/***********************************************************************************************************************************
Get PostgreSQL info
***********************************************************************************************************************************/
InfoPg *
infoBackupPg(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->infoPg);
}

/***********************************************************************************************************************************
Get total current backups
***********************************************************************************************************************************/
unsigned int
infoBackupDataTotal(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((this->backup == NULL ? 0 : lstSize(this->backup)));
}

/***********************************************************************************************************************************
Return a structure of the backup data from a specific index
***********************************************************************************************************************************/
InfoBackupData
infoBackupData(const InfoBackup *this, unsigned int backupDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, backupDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_BACKUP_DATA, *((InfoBackupData *)lstGet(this->backup, backupDataIdx)));
}
/***********************************************************************************************************************************
Delete a backup from the current backup
***********************************************************************************************************************************/
void
infoBackupDataDelete(const InfoBackup *this, const String *backupDeleteLabel)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(STRING, backupDeleteLabel);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    for (unsigned int idx = 0; idx < infoBackupDataTotal(this); idx++)
    {
        InfoBackupData backupData = infoBackupData(this, idx);
        if (strCmp(backupData.backupLabel, backupDeleteLabel) == 0)
        {
            lstRemove(this->backup, idx);
            // CSHANG Here we need to remove it from the ini
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return a list of current backup labels, applying a regex filter if provided and sorting in reverse if requested
***********************************************************************************************************************************/
StringList *
infoBackupDataLabelList(const InfoBackup *this, InfoBackupDataLabelListParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(BOOL, param.reverse);
        FUNCTION_LOG_PARAM(STRING, param.filter);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();

        // For each backup label, compare it to the filter (if any) and sort it for return
        for (unsigned int backupLabelIdx = 0; backupLabelIdx < infoBackupDataTotal(this); backupLabelIdx++)
        {
            InfoBackupData backupData = infoBackupData(this, backupLabelIdx);
            if (param.filter == NULL || regExpMatchOne(param.filter, backupData.backupLabel))
            {
                if (!param.reverse)
                    strLstAdd(result, backupData.backupLabel);
                else
                    strLstInsert(result, 0, backupData.backupLabel);
            }
        }

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
infoBackupDataToLog(const InfoBackupData *this)
{
    return strNewFmt("{label: %s, pgId: %u}", strPtr(this->backupLabel), this->backupPgId);
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoBackupFree(InfoBackup *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
