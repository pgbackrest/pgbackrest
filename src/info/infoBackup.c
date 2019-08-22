/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/info.h"
#include "info/infoBackup.h"
#include "info/infoManifest.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
??? INFO_BACKUP_SECTION should be in a separate include since it will also be used when reading the manifest
***********************************************************************************************************************************/
#define INFO_BACKUP_SECTION                                         "backup"
#define INFO_BACKUP_SECTION_BACKUP_CURRENT                          INFO_BACKUP_SECTION ":current"

VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR,       "backup-info-repo-size");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR, "backup-info-repo-size-delta");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR,            "backup-info-size");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR,      "backup-info-size-delta");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR,            "backup-reference");

STRING_EXTERN(INFO_BACKUP_PATH_FILE_STR,                            INFO_BACKUP_PATH_FILE);
STRING_EXTERN(INFO_BACKUP_PATH_FILE_COPY_STR,                       INFO_BACKUP_PATH_FILE_COPY);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
    List *backup;                                                   // List of current backups and their associated data
};

OBJECT_DEFINE_FREE(INFO_BACKUP);

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
static InfoBackup *
infoBackupNewInternal(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoBackup")
    {
        // Create object
        this = memNew(sizeof(InfoBackup));
        this->memContext = MEM_CONTEXT_NEW();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Create new object without loading it from a file
***********************************************************************************************************************************/
InfoBackup *
infoBackupNew(unsigned int pgVersion, uint64_t pgSystemId, const uint32_t pgControlVersion, const uint32_t pgCatalogVersion,
    CipherType cipherType, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
        FUNCTION_LOG_PARAM(UINT32, pgControlVersion);
        FUNCTION_LOG_PARAM(UINT32, pgCatalogVersion);
    FUNCTION_LOG_END();

    ASSERT(pgVersion > 0 && pgSystemId > 0 && pgControlVersion > 0 && pgCatalogVersion > 0);

    InfoBackup *this = infoBackupNewInternal();

    // Initialize the pg data
    this->infoPg = infoPgNew(cipherType, cipherPassSub);
    infoBackupPgSet(this, pgVersion, pgSystemId, pgControlVersion, pgCatalogVersion);

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
InfoBackup *
infoBackupNewLoad(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    InfoBackup *this = infoBackupNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        Ini *ini = NULL;

        // Catch file missing error and add backup-specific hints before rethrowing
        TRY_BEGIN()
        {
            this->infoPg = infoPgNewLoad(storage, fileName, infoPgBackup, cipherType, cipherPass, &ini);
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

        const String *backupCurrentSection = STRDEF(INFO_BACKUP_SECTION_BACKUP_CURRENT);

        // If there are current backups, then parse the json for each into a list object
        if (strLstExists(iniSectionList(ini), backupCurrentSection))
        {
            // Initialize the store and get the list of backup labels
            this->backup = lstNew(sizeof(InfoBackupData));
            const StringList *backupLabelList = iniSectionKeyList(ini, backupCurrentSection);

            // For each backup label, store the information
            for (unsigned int backupLabelIdx = 0; backupLabelIdx < strLstSize(backupLabelList); backupLabelIdx++)
            {
                const String *backupLabelKey = strLstGet(backupLabelList, backupLabelIdx);
                const KeyValue *backupKv = jsonToKv(iniGet(ini, backupCurrentSection, backupLabelKey));

                InfoBackupData infoBackupData =
                {
                    .backrestFormat = varUIntForce(kvGet(backupKv, VARSTR(INFO_KEY_FORMAT_STR))),
                    .backrestVersion = varStrForce(kvGet(backupKv, VARSTR(INFO_KEY_VERSION_STR))),
                    .backupInfoRepoSize = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR)),
                    .backupInfoRepoSizeDelta = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR)),
                    .backupInfoSize = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR)),
                    .backupInfoSizeDelta = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR)),
                    .backupLabel = strDup(backupLabelKey),
                    .backupPgId = cvtZToUInt(strPtr(varStrForce(kvGet(backupKv, INFO_KEY_DB_ID_VAR)))),
                    .backupTimestampStart = varUInt64(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_VAR)),
                    .backupTimestampStop= varUInt64(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_VAR)),
                    .backupType = varStrForce(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_TYPE_VAR)),

                    // Possible NULL values
                    .backupArchiveStart = varStr(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_VAR)),
                    .backupArchiveStop = varStr(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR)),
                    .backupPrior = varStr(kvGet(backupKv, INFO_MANIFEST_KEY_BACKUP_PRIOR_VAR)),
                    .backupReference =
                        kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR) != NULL ?
                            strLstNewVarLst(varVarLst(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR))) : NULL,

                    // Options
                    .optionArchiveCheck = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_VAR)),
                    .optionArchiveCopy = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_VAR)),
                    .optionBackupStandby = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_VAR)),
                    .optionChecksumPage = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_VAR)),
                    .optionCompress = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_COMPRESS_VAR)),
                    .optionHardlink = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_HARDLINK_VAR)),
                    .optionOnline = varBool(kvGet(backupKv, INFO_MANIFEST_KEY_OPT_ONLINE_VAR)),
                };

                // Add the backup data to the list
                lstAdd(this->backup, &infoBackupData);
            }
        }

        iniFree(ini);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoBackupSave(
    InfoBackup *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Ini *ini = iniNew();

        // Set the backup current section
        for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(this); backupIdx++)
        {
            InfoBackupData backupData = infoBackupData(this, backupIdx);

            KeyValue *backupDataKv = kvNew();
            kvPut(backupDataKv, VARSTR(INFO_KEY_FORMAT_STR), VARUINT(backupData.backrestFormat));
            kvPut(backupDataKv, VARSTR(INFO_KEY_VERSION_STR), VARSTR(backupData.backrestVersion));

            kvPut(backupDataKv, INFO_KEY_DB_ID_VAR, VARUINT(backupData.backupPgId));

            kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_VAR, VARSTR(backupData.backupArchiveStart));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR, VARSTR(backupData.backupArchiveStop));

            if (backupData.backupPrior != NULL)
                kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_PRIOR_VAR, VARSTR(backupData.backupPrior));

            if (backupData.backupReference != NULL)
            {
                kvPut(
                    backupDataKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR, varNewVarLst(varLstNewStrLst(backupData.backupReference)));
            }

            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR, VARUINT64(backupData.backupInfoRepoSize));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR, VARUINT64(backupData.backupInfoRepoSizeDelta));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR, VARUINT64(backupData.backupInfoSize));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR, VARUINT64(backupData.backupInfoSizeDelta));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_VAR, VARUINT64(backupData.backupTimestampStart));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_VAR, VARUINT64(backupData.backupTimestampStop));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_BACKUP_TYPE_VAR, VARSTR(backupData.backupType));

            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_VAR, VARBOOL(backupData.optionArchiveCheck));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_VAR, VARBOOL(backupData.optionArchiveCopy));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_VAR, VARBOOL(backupData.optionBackupStandby));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_VAR, VARBOOL(backupData.optionChecksumPage));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_COMPRESS_VAR, VARBOOL(backupData.optionCompress));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_HARDLINK_VAR, VARBOOL(backupData.optionHardlink));
            kvPut(backupDataKv, INFO_MANIFEST_KEY_OPT_ONLINE_VAR, VARBOOL(backupData.optionOnline));

            iniSet(ini, STRDEF(INFO_BACKUP_SECTION_BACKUP_CURRENT), backupData.backupLabel, jsonFromKv(backupDataKv, 0));
        }

        infoPgSave(infoBackupPg(this), ini, storage, fileName, infoPgBackup, cipherType, cipherPass);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
Set the infoPg data
***********************************************************************************************************************************/
InfoBackup *
infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, uint32_t pgControlVersion, uint32_t pgCatalogVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT32, pgControlVersion);
        FUNCTION_LOG_PARAM(UINT32, pgCatalogVersion);
    FUNCTION_LOG_END();

    this->infoPg = infoPgSet(this->infoPg, infoPgBackup, pgVersion, pgSystemId, pgControlVersion, pgCatalogVersion);

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
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
Delete a backup from the current backup list
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
            lstRemove(this->backup, idx);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return a list of current backup labels, applying a regex expression if provided
***********************************************************************************************************************************/
StringList *
infoBackupDataLabelList(const InfoBackup *this, const String *expression)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Return a 0 sized list if no current backups or none matching the filter
    StringList *result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepare regexp if an expression was passed
        RegExp *regExp = (expression == NULL) ? NULL : regExpNew(expression);

        // For each backup label, compare it to the filter (if any) and sort it for return
        for (unsigned int backupLabelIdx = 0; backupLabelIdx < infoBackupDataTotal(this); backupLabelIdx++)
        {
            InfoBackupData backupData = infoBackupData(this, backupLabelIdx);

            if (regExp == NULL || regExpMatch(regExp, backupData.backupLabel))
            {
                strLstAdd(result, backupData.backupLabel);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Return the cipher passphrase
***********************************************************************************************************************************/
const String *
infoBackupCipherPass(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoPgCipherPass(this->infoPg));
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
infoBackupDataToLog(const InfoBackupData *this)
{
    return strNewFmt("{label: %s, pgId: %u}", strPtr(this->backupLabel), this->backupPgId);
}
