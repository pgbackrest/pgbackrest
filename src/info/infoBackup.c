/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/backup/common.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/ini.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_BACKUP_SECTION                                         "backup"
#define INFO_BACKUP_SECTION_BACKUP_CURRENT                          INFO_BACKUP_SECTION ":current"
    STRING_STATIC(INFO_BACKUP_SECTION_BACKUP_CURRENT_STR,           INFO_BACKUP_SECTION_BACKUP_CURRENT);

VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_ARCHIVE_START_VAR,     "backup-archive-start");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP_VAR,      "backup-archive-stop");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR,    "backup-info-repo-size");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR, "backup-info-repo-size-delta");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR,         "backup-info-size");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR,   "backup-info-size-delta");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_LSN_START_VAR,         "backup-lsn-start");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_LSN_STOP_VAR,          "backup-lsn-stop");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_PRIOR_VAR,             "backup-prior");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR,         "backup-reference");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR,   "backup-timestamp-start");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR,    "backup-timestamp-stop");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TYPE_VAR,              "backup-type");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_ERROR_VAR,             "backup-error");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK_VAR,        "option-archive-check");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_ARCHIVE_COPY_VAR,         "option-archive-copy");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_BACKUP_STANDBY_VAR,       "option-backup-standby");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE_VAR,        "option-checksum-page");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_COMPRESS_VAR,             "option-compress");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_HARDLINK_VAR,             "option-hardlink");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_OPT_ONLINE_VAR,               "option-online");

STRING_EXTERN(INFO_BACKUP_PATH_FILE_STR,                            INFO_BACKUP_PATH_FILE);
STRING_EXTERN(INFO_BACKUP_PATH_FILE_COPY_STR,                       INFO_BACKUP_PATH_FILE_COPY);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    InfoBackupPub pub;                                              // Publicly accessible variables
};

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoBackup *
infoBackupNewInternal(void)
{
    FUNCTION_TEST_VOID();

    InfoBackup *this = OBJ_NEW_ALLOC();

    *this = (InfoBackup)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .backup = lstNewP(sizeof(InfoBackupData), .comparator = lstComparatorStr),
        },
    };

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
InfoBackup *
infoBackupNew(unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT, pgCatalogVersion);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    ASSERT(pgVersion > 0 && pgSystemId > 0 && pgCatalogVersion > 0);

    InfoBackup *this = NULL;

    OBJ_NEW_BEGIN(InfoBackup)
    {
        this = infoBackupNewInternal();

        // Initialize the pg data
        this->pub.infoPg = infoPgNew(infoPgBackup, cipherPassSub);
        infoBackupPgSet(this, pgVersion, pgSystemId, pgCatalogVersion);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
static void
infoBackupLoadCallback(void *data, const String *section, const String *key, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoBackup *infoBackup = (InfoBackup *)data;

    // Process current backup list
    if (strEq(section, INFO_BACKUP_SECTION_BACKUP_CURRENT_STR))
    {
        const KeyValue *backupKv = varKv(value);

        MEM_CONTEXT_BEGIN(lstMemContext(infoBackup->pub.backup))
        {
            InfoBackupData infoBackupData =
            {
                .backrestFormat = varUIntForce(kvGet(backupKv, VARSTR(INFO_KEY_FORMAT_STR))),
                .backrestVersion = varStrForce(kvGet(backupKv, VARSTR(INFO_KEY_VERSION_STR))),
                .backupInfoRepoSize = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR)),
                .backupInfoRepoSizeDelta = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR)),
                .backupInfoSize = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR)),
                .backupInfoSizeDelta = varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR)),
                .backupLabel = strDup(key),
                .backupPgId = cvtZToUInt(strZ(varStrForce(kvGet(backupKv, INFO_KEY_DB_ID_VAR)))),

                // When reading timestamps, read as uint64 to ensure always positive value (guarantee no backups before 1970)
                .backupTimestampStart = (time_t)varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR)),
                .backupTimestampStop= (time_t)varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR)),
                .backupType = (BackupType)strIdFromStr(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TYPE_VAR))),

                // Possible NULL values
                .backupArchiveStart = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_START_VAR))),
                .backupArchiveStop = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP_VAR))),
                .backupLsnStart = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_LSN_START_VAR))),
                .backupLsnStop = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_LSN_STOP_VAR))),
                .backupPrior = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_PRIOR_VAR))),
                .backupReference =
                    kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR) != NULL ?
                        strLstNewVarLst(varVarLst(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR))) : NULL,

                // Report errors detected during the backup. The key may not exist in older versions.
                .backupError = varDup(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_ERROR_VAR)),

                // Options
                .optionArchiveCheck = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK_VAR)),
                .optionArchiveCopy = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ARCHIVE_COPY_VAR)),
                .optionBackupStandby = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_BACKUP_STANDBY_VAR)),
                .optionChecksumPage = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE_VAR)),
                .optionCompress = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_COMPRESS_VAR)),
                .optionHardlink = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_HARDLINK_VAR)),
                .optionOnline = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ONLINE_VAR)),
            };

            ASSERT(
                infoBackupData.backupType == backupTypeFull || infoBackupData.backupType == backupTypeDiff ||
                infoBackupData.backupType == backupTypeIncr);

            // Add the backup data to the list
            lstAdd(infoBackup->pub.backup, &infoBackupData);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
InfoBackup *
infoBackupNewLoad(IoRead *read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    InfoBackup *this = NULL;

    OBJ_NEW_BEGIN(InfoBackup)
    {
        this = infoBackupNewInternal();
        this->pub.infoPg = infoPgNewLoad(read, infoPgBackup, infoBackupLoadCallback, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
static void
infoBackupSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(infoSaveData != NULL);

    InfoBackup *infoBackup = (InfoBackup *)data;

    if (infoSaveSection(infoSaveData, INFO_BACKUP_SECTION_BACKUP_CURRENT_STR, sectionNext))
    {
        // Set the backup current section
        for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
        {
            InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

            jsonWriteUInt(jsonWriteKeyZ(json, INFO_KEY_FORMAT), backupData.backrestFormat);
            jsonWriteStr(jsonWriteKeyZ(json, INFO_KEY_VERSION), backupData.backrestVersion);

            jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_ARCHIVE_START_VAR)), backupData.backupArchiveStart);
            jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP_VAR)), backupData.backupArchiveStop);

            // Do not save backup-error if it was not loaded. This prevents backups that were added before the backup-error flag
            // was introduced from being saved with an incorrect value.
            if (backupData.backupError != NULL)
                jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_ERROR_VAR)), varBool(backupData.backupError));

            jsonWriteUInt64(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR)), backupData.backupInfoRepoSize);
            jsonWriteUInt64(
                jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR)), backupData.backupInfoRepoSizeDelta);
            jsonWriteUInt64(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR)), backupData.backupInfoSize);
            jsonWriteUInt64(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR)), backupData.backupInfoSizeDelta);

            if (backupData.backupLsnStart != NULL)
                jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_LSN_START_VAR)), backupData.backupLsnStart);

            if (backupData.backupLsnStop != NULL)
                jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_LSN_STOP_VAR)), backupData.backupLsnStop);

            if (backupData.backupPrior != NULL)
                jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_PRIOR_VAR)), backupData.backupPrior);

            if (backupData.backupReference != NULL)
                jsonWriteStrLst(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR)), backupData.backupReference);

            // When storing time_t treat as signed int to avoid casting
            jsonWriteInt64(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR)), backupData.backupTimestampStart);
            jsonWriteInt64(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR)), backupData.backupTimestampStop);

            jsonWriteStr(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_BACKUP_TYPE_VAR)), strIdToStr(backupData.backupType));
            jsonWriteUInt(jsonWriteKey(json, varStr(INFO_KEY_DB_ID_VAR)), backupData.backupPgId);

            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK_VAR)), backupData.optionArchiveCheck);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_ARCHIVE_COPY_VAR)), backupData.optionArchiveCopy);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_BACKUP_STANDBY_VAR)), backupData.optionBackupStandby);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE_VAR)), backupData.optionChecksumPage);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_COMPRESS_VAR)), backupData.optionCompress);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_HARDLINK_VAR)), backupData.optionHardlink);
            jsonWriteBool(jsonWriteKey(json, varStr(INFO_BACKUP_KEY_OPT_ONLINE_VAR)), backupData.optionOnline);

            infoSaveValueBuf(
                infoSaveData, INFO_BACKUP_SECTION_BACKUP_CURRENT_STR, backupData.backupLabel,
                jsonWriteResult(jsonWriteObjectEnd(json)));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
infoBackupSave(InfoBackup *this, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        infoPgSave(infoBackupPg(this), write, infoBackupSaveCallback, this);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
InfoBackup *
infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT, pgCatalogVersion);
    FUNCTION_LOG_END();

    this->pub.infoPg = infoPgSet(infoBackupPg(this), infoPgBackup, pgVersion, pgSystemId, pgCatalogVersion);

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/**********************************************************************************************************************************/
InfoBackupData
infoBackupData(const InfoBackup *this, unsigned int backupDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, backupDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_BACKUP_DATA, *(InfoBackupData *)lstGet(this->pub.backup, backupDataIdx));
}

/**********************************************************************************************************************************/
void
infoBackupDataAdd(const InfoBackup *this, const Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const ManifestData *manData = manifestData(manifest);

        // Calculate backup sizes, references and report errors
        uint64_t backupSize = 0;
        uint64_t backupSizeDelta = 0;
        uint64_t backupRepoSize = 0;
        uint64_t backupRepoSizeDelta = 0;
        StringList *referenceList = strLstNew();
        bool backupError = false;

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile file = manifestFile(manifest, fileIdx);

            backupSize += file.size;
            backupRepoSize += file.sizeRepo > 0 ? file.sizeRepo : file.size;

            // If a reference to a file exists, then it is in a previous backup and the delta calculation was already done
            if (file.reference != NULL)
                strLstAddIfMissing(referenceList, file.reference);
            else
            {
                backupSizeDelta += file.size;
                backupRepoSizeDelta += file.sizeRepo > 0 ? file.sizeRepo : file.size;
            }

            // Is there an error in the file?
            if (file.checksumPageError)
                backupError = true;
        }

        MEM_CONTEXT_BEGIN(lstMemContext(this->pub.backup))
        {
            InfoBackupData infoBackupData =
            {
                .backupLabel = strDup(manData->backupLabel),
                .backrestFormat = REPOSITORY_FORMAT,
                .backrestVersion = strDup(manData->backrestVersion),
                .backupInfoRepoSize = backupRepoSize,
                .backupInfoRepoSizeDelta = backupRepoSizeDelta,
                .backupInfoSize = backupSize,
                .backupInfoSizeDelta = backupSizeDelta,
                .backupPgId = manData->pgId,
                .backupTimestampStart = manData->backupTimestampStart,
                .backupTimestampStop= manData->backupTimestampStop,
                .backupType = manData->backupType,
                .backupError = varNewBool(backupError),

                .backupArchiveStart = strDup(manData->archiveStart),
                .backupArchiveStop = strDup(manData->archiveStop),
                .backupLsnStart = strDup(manData->lsnStart),
                .backupLsnStop = strDup(manData->lsnStop),

                .optionArchiveCheck = manData->backupOptionArchiveCheck,
                .optionArchiveCopy = manData->backupOptionArchiveCopy,
                .optionBackupStandby = manData->backupOptionStandby != NULL ? varBool(manData->backupOptionStandby) : false,
                .optionChecksumPage = manData->backupOptionChecksumPage != NULL ?
                    varBool(manData->backupOptionChecksumPage) : false,
                .optionCompress = manData->backupOptionCompressType != compressTypeNone,
                .optionHardlink = manData->backupOptionHardLink,
                .optionOnline = manData->backupOptionOnline,
            };

            if (manData->backupType != backupTypeFull)
            {
                strLstSort(referenceList, sortOrderAsc);
                infoBackupData.backupReference = strLstDup(referenceList);
                infoBackupData.backupPrior = strDup(manData->backupLabelPrior);
            }

            // Add the backup data to the current backup list
            lstAdd(this->pub.backup, &infoBackupData);

            // Ensure the list is sorted ascending by the backupLabel
            lstSort(this->pub.backup, sortOrderAsc);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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
            lstRemoveIdx(this->pub.backup, idx);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
StringList *
infoBackupDataDependentList(const InfoBackup *this, const String *backupLabel)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(backupLabel != NULL);

    // Return the given label as the only dependency or the given label and a list of labels that depend on it
    StringList *result = strLstNew();
    strLstAdd(result, backupLabel);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // For each backup label from oldest to newest in the current section, add each dependency to the list
        for (unsigned int backupLabelIdx = 0; backupLabelIdx < infoBackupDataTotal(this); backupLabelIdx++)
        {
            InfoBackupData backupData = infoBackupData(this, backupLabelIdx);

            // If the backupPrior is in the dependency chain add the label to the list
            if (backupData.backupPrior != NULL && strLstExists(result, backupData.backupPrior))
                strLstAdd(result, backupData.backupLabel);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
typedef struct InfoBackupLoadFileData
{
    MemContext *memContext;                                         // Mem context
    const Storage *storage;                                         // Storage to load from
    const String *fileName;                                         // Base filename
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    InfoBackup *infoBackup;                                         // Loaded infoBackup object
} InfoBackupLoadFileData;

static bool
infoBackupLoadFileCallback(void *data, unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    InfoBackupLoadFileData *loadData = (InfoBackupLoadFileData *)data;
    bool result = false;

    if (try < 2)
    {
        // Construct filename based on try
        const String *fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strZ(loadData->fileName));

        // Attempt to load the file
        IoRead *read = storageReadIo(storageNewReadP(loadData->storage, fileName));
        cipherBlockFilterGroupAdd(ioReadFilterGroup(read), loadData->cipherType, cipherModeDecrypt, loadData->cipherPass);

        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            loadData->infoBackup = infoBackupNewLoad(read);
            result = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

InfoBackup *
infoBackupLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    InfoBackupLoadFileData data =
    {
        .memContext = memContextCurrent(),
        .storage = storage,
        .fileName = fileName,
        .cipherType = cipherType,
        .cipherPass = cipherPass,
    };

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *fileNamePath = strZ(storagePathP(storage, fileName));

        TRY_BEGIN()
        {
            infoLoad(
                strNewFmt("unable to load info file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
                infoBackupLoadFileCallback, &data);
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
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, data.infoBackup);
}

/**********************************************************************************************************************************/
InfoBackup *
infoBackupLoadFileReconstruct(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    InfoBackup *infoBackup = infoBackupLoadFile(storage, fileName, cipherType, cipherPass);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get a list of backups in the repo
        StringList *backupList = strLstSort(
            storageListP(
                storage, STORAGE_REPO_BACKUP_STR,
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderAsc);

        // Get the list of current backups and remove backups from current that are no longer in the repository
        StringList *backupCurrentList = strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc);

        for (unsigned int backupCurrIdx = 0; backupCurrIdx < strLstSize(backupCurrentList); backupCurrIdx++)
        {
            String *backupLabel = strLstGet(backupCurrentList, backupCurrIdx);
            String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

            // If the manifest does not exist on disk and this backup has not already been deleted from the current list in the
            // infoBackup object, then remove it and its dependencies
            if (!storageExistsP(storage, manifestFileName) && infoBackupLabelExists(infoBackup, backupLabel))
            {
                StringList *backupList = strLstSort(infoBackupDataDependentList(infoBackup, backupLabel), sortOrderDesc);

                for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
                {
                    String *removeBackup = strLstGet(backupList, backupIdx);

                    LOG_WARN_FMT("backup '%s' missing manifest removed from " INFO_BACKUP_FILE, strZ(removeBackup));

                    infoBackupDataDelete(infoBackup, removeBackup);
                }
            }
        }

        // Get the updated current list of backups from backup.info
        backupCurrentList = strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc);

        // For each backup in the repo, check if it exists in backup.info
        for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
        {
            String *backupLabel = strLstGet(backupList, backupIdx);

            // If it does not exist in the list of current backups, then if it is valid, add it
            if (!strLstExists(backupCurrentList, backupLabel))
            {
                String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

                // Check if a completed backup exists (backup.manifest only - ignore .copy)
                if (storageExistsP(storage, manifestFileName))
                {
                    bool found = false;
                    const Manifest *manifest = manifestLoadFile(
                        storage, manifestFileName, cipherType, infoPgCipherPass(infoBackupPg(infoBackup)));
                    const ManifestData *manData = manifestData(manifest);

                    // If the pg data for the manifest exists in the history, then add it to current, but if something doesn't match
                    // then warn that the backup is not valid
                    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoBackupPg(infoBackup)); pgIdx++)
                    {
                        InfoPgData pgHistory = infoPgData(infoBackupPg(infoBackup), pgIdx);

                        // If there is an exact match with the history, system and version and there is no backup-prior dependency
                        // or there is a backup-prior and it is in the list, then add this backup to the current backup list
                        if (manData->pgId == pgHistory.id && manData->pgSystemId == pgHistory.systemId &&
                            manData->pgVersion == pgHistory.version &&
                            (manData->backupLabelPrior == NULL || infoBackupLabelExists(infoBackup, manData->backupLabelPrior)))
                        {
                            LOG_WARN_FMT("backup '%s' found in repository added to " INFO_BACKUP_FILE, strZ(backupLabel));
                            infoBackupDataAdd(infoBackup, manifest);
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        LOG_WARN_FMT("invalid backup '%s' cannot be added to current backups", strZ(manData->backupLabel));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, infoBackup);
}

/**********************************************************************************************************************************/
void
infoBackupSaveFile(
    InfoBackup *infoBackup, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write output into a buffer since it needs to be saved to storage twice
        Buffer *buffer = bufNew(ioBufferSize());
        IoWrite *write = ioBufferWriteNew(buffer);
        cipherBlockFilterGroupAdd(ioWriteFilterGroup(write), cipherType, cipherModeEncrypt, cipherPass);
        infoBackupSave(infoBackup, write);

        // Save the file and make a copy
        storagePutP(storageNewWriteP(storage, fileName), buffer);
        storagePutP(storageNewWriteP(storage, strNewFmt("%s" INFO_COPY_EXT, strZ(fileName))), buffer);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
infoBackupDataToLog(const InfoBackupData *this)
{
    return strNewFmt("{label: %s, pgId: %u}", strZ(this->backupLabel), this->backupPgId);
}
