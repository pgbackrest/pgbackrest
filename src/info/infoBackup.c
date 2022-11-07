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

    FUNCTION_TEST_RETURN(INFO_BACKUP, this);
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

    OBJ_NEW_BEGIN(InfoBackup, .childQty = MEM_CONTEXT_QTY_MAX)
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
#define INFO_BACKUP_SECTION                                         "backup"
#define INFO_BACKUP_SECTION_BACKUP_CURRENT                          INFO_BACKUP_SECTION ":current"

#define INFO_BACKUP_KEY_BACKUP_ANNOTATION                           "backup-annotation"
#define INFO_BACKUP_KEY_BACKUP_ARCHIVE_START                        "backup-archive-start"
#define INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP                         "backup-archive-stop"
#define INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE                       "backup-info-repo-size"
#define INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA                 "backup-info-repo-size-delta"
#define INFO_BACKUP_KEY_BACKUP_INFO_SIZE                            "backup-info-size"
#define INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA                      "backup-info-size-delta"
#define INFO_BACKUP_KEY_BACKUP_LSN_START                            "backup-lsn-start"
#define INFO_BACKUP_KEY_BACKUP_LSN_STOP                             "backup-lsn-stop"
#define INFO_BACKUP_KEY_BACKUP_PRIOR                                STRID5("backup-prior", 0x93d3286e1558c220)
#define INFO_BACKUP_KEY_BACKUP_REFERENCE                            "backup-reference"
#define INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START                      "backup-timestamp-start"
#define INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP                       "backup-timestamp-stop"
#define INFO_BACKUP_KEY_BACKUP_TYPE                                 STRID5("backup-type", 0x1619a6e1558c220)
#define INFO_BACKUP_KEY_BACKUP_ERROR                                STRID5("backup-error", 0x93e522ee1558c220)
#define INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK                           "option-archive-check"
#define INFO_BACKUP_KEY_OPT_ARCHIVE_COPY                            "option-archive-copy"
#define INFO_BACKUP_KEY_OPT_BACKUP_STANDBY                          "option-backup-standby"
#define INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE                           "option-checksum-page"
#define INFO_BACKUP_KEY_OPT_COMPRESS                                "option-compress"
#define INFO_BACKUP_KEY_OPT_HARDLINK                                "option-hardlink"
#define INFO_BACKUP_KEY_OPT_ONLINE                                  "option-online"

static void
infoBackupLoadCallback(void *data, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoBackup *const infoBackup = (InfoBackup *)data;

    // Process current backup list
    if (strEqZ(section, INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        MEM_CONTEXT_BEGIN(lstMemContext(infoBackup->pub.backup))
        {
            JsonRead *const json = jsonReadNew(value);
            jsonReadObjectBegin(json);

            InfoBackupData info =
            {
                .backupLabel = strDup(key),

            };

            // Format and version
            info.backrestFormat = jsonReadUInt(jsonReadKeyRequireZ(json, INFO_KEY_FORMAT));
            info.backrestVersion = jsonReadStr(jsonReadKeyRequireZ(json, INFO_KEY_VERSION));

            // Annotation
            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_ANNOTATION))
                info.backupAnnotation = jsonReadVar(json);

            // Archive start/stop
            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_ARCHIVE_START))
                info.backupArchiveStart = jsonReadStr(json);

            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP))
                info.backupArchiveStop = jsonReadStr(json);

            // Report errors detected during the backup. The key may not exist in older versions.
            if (jsonReadKeyExpectStrId(json, INFO_BACKUP_KEY_BACKUP_ERROR))
                info.backupError = varNewBool(jsonReadBool(json));

            // Size info
            info.backupInfoRepoSize = jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE));
            info.backupInfoRepoSizeDelta = jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA));
            info.backupInfoSize = jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_INFO_SIZE));
            info.backupInfoSizeDelta = jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA));

            // Lsn start/stop
            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_LSN_START))
                info.backupLsnStart = jsonReadStr(json);

            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_LSN_STOP))
                info.backupLsnStop = jsonReadStr(json);

            // Prior backup
            if (jsonReadKeyExpectStrId(json, INFO_BACKUP_KEY_BACKUP_PRIOR))
                info.backupPrior = jsonReadStr(json);

            // Reference
            if (jsonReadKeyExpectZ(json, INFO_BACKUP_KEY_BACKUP_REFERENCE))
                info.backupReference = jsonReadStrLst(json);

            // Read timestamps as uint64 to ensure a positive value (guarantee no backups before 1970)
            info.backupTimestampStart = (time_t)jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START));
            info.backupTimestampStop = (time_t)jsonReadUInt64(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP));

            // backup type
            info.backupType = (BackupType)jsonReadStrId(jsonReadKeyRequireStrId(json, INFO_BACKUP_KEY_BACKUP_TYPE));
            ASSERT(info.backupType == backupTypeFull || info.backupType == backupTypeDiff || info.backupType == backupTypeIncr);

            // Database id
            info.backupPgId = jsonReadUInt(jsonReadKeyRequireZ(json, INFO_KEY_DB_ID));

                // Options
            info.optionArchiveCheck = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK));
            info.optionArchiveCopy = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_ARCHIVE_COPY));
            info.optionBackupStandby = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_BACKUP_STANDBY));
            info.optionChecksumPage = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE));
            info.optionCompress = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_COMPRESS));
            info.optionHardlink = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_HARDLINK));
            info.optionOnline = jsonReadBool(jsonReadKeyRequireZ(json, INFO_BACKUP_KEY_OPT_ONLINE));

            // Add the backup data to the list
            lstAdd(infoBackup->pub.backup, &info);
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

    OBJ_NEW_BEGIN(InfoBackup, .childQty = MEM_CONTEXT_QTY_MAX)
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
infoBackupSaveCallback(void *const data, const String *const sectionNext, InfoSave *const infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(infoSaveData != NULL);

    InfoBackup *const infoBackup = (InfoBackup *)data;

    if (infoSaveSection(infoSaveData, INFO_BACKUP_SECTION_BACKUP_CURRENT, sectionNext))
    {
        // Set the backup current section
        for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
        {
            const InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

            jsonWriteUInt(jsonWriteKeyZ(json, INFO_KEY_FORMAT), backupData.backrestFormat);
            jsonWriteStr(jsonWriteKeyZ(json, INFO_KEY_VERSION), backupData.backrestVersion);

            if (backupData.backupAnnotation != NULL)
                jsonWriteVar(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_ANNOTATION), backupData.backupAnnotation);

            jsonWriteStr(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_ARCHIVE_START), backupData.backupArchiveStart);
            jsonWriteStr(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP), backupData.backupArchiveStop);

            // Do not save backup-error if it was not loaded. This prevents backups that were added before the backup-error flag
            // was introduced from being saved with an incorrect value.
            if (backupData.backupError != NULL)
                jsonWriteBool(jsonWriteKeyStrId(json, INFO_BACKUP_KEY_BACKUP_ERROR), varBool(backupData.backupError));

            jsonWriteUInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE), backupData.backupInfoRepoSize);
            jsonWriteUInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA), backupData.backupInfoRepoSizeDelta);
            jsonWriteUInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_INFO_SIZE), backupData.backupInfoSize);
            jsonWriteUInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA), backupData.backupInfoSizeDelta);

            if (backupData.backupLsnStart != NULL)
                jsonWriteStr(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_LSN_START), backupData.backupLsnStart);

            if (backupData.backupLsnStop != NULL)
                jsonWriteStr(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_LSN_STOP), backupData.backupLsnStop);

            if (backupData.backupPrior != NULL)
                jsonWriteStr(jsonWriteKeyStrId(json, INFO_BACKUP_KEY_BACKUP_PRIOR), backupData.backupPrior);

            if (backupData.backupReference != NULL)
                jsonWriteStrLst(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_REFERENCE), backupData.backupReference);

            // When storing time_t treat as signed int to avoid casting
            jsonWriteInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START), backupData.backupTimestampStart);
            jsonWriteInt64(jsonWriteKeyZ(json, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP), backupData.backupTimestampStop);

            jsonWriteStrId(jsonWriteKeyStrId(json, INFO_BACKUP_KEY_BACKUP_TYPE), backupData.backupType);
            jsonWriteUInt(jsonWriteKeyZ(json, INFO_KEY_DB_ID), backupData.backupPgId);

            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK), backupData.optionArchiveCheck);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_ARCHIVE_COPY), backupData.optionArchiveCopy);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_BACKUP_STANDBY), backupData.optionBackupStandby);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE), backupData.optionChecksumPage);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_COMPRESS), backupData.optionCompress);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_HARDLINK), backupData.optionHardlink);
            jsonWriteBool(jsonWriteKeyZ(json, INFO_BACKUP_KEY_OPT_ONLINE), backupData.optionOnline);

            infoSaveValue(
                infoSaveData, INFO_BACKUP_SECTION_BACKUP_CURRENT, strZ(backupData.backupLabel),
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
        bool backupError = false;

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile file = manifestFile(manifest, fileIdx);

            backupSize += file.size;
            backupRepoSize += file.sizeRepo > 0 ? file.sizeRepo : file.size;

            // If a reference to a file exists, then it is in a previous backup and the delta calculation was already done
            if (file.reference == NULL)
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

                .backupAnnotation = varDup(manData->annotation),
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
                // This list may not be sorted for manifests created before the reference list was added
                infoBackupData.backupReference = strLstSort(strLstDup(manifestReferenceList(manifest)), sortOrderAsc);
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
infoBackupDataAnnotationSet(const InfoBackup *const this, const String *const backupLabel, const KeyValue *const newAnnotationKv)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(KEY_VALUE, newAnnotationKv);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(infoBackupLabelExists(this, backupLabel));
    ASSERT(newAnnotationKv != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.backup))
    {
        // Get data for specified backup
        InfoBackupData *const infoBackupData = lstFind(this->pub.backup, &backupLabel);

        // Create annotation if it does not exist
        if (infoBackupData->backupAnnotation == NULL)
            infoBackupData->backupAnnotation = varNewKv(kvNew());

        // Update annotations
        KeyValue *const annotationKv = varKv(infoBackupData->backupAnnotation);
        const VariantList *const newAnnotationKeyList = kvKeyList(newAnnotationKv);

        for (unsigned int keyIdx = 0; keyIdx < varLstSize(newAnnotationKeyList); keyIdx++)
        {
            const Variant *const newKey = varLstGet(newAnnotationKeyList, keyIdx);
            const Variant *const newValue = kvGet(newAnnotationKv, newKey);

            // If value is empty remove the key
            if (strEmpty(varStr(newValue)))
            {
                kvRemove(annotationKv, newKey);
            }
            // Else put new key/value (this will overwrite an existing value)
            else
                kvPut(annotationKv, newKey, newValue);
        }

        // Clean field if there are no annotations left
        if (varLstSize(kvKeyList(annotationKv)) == 0)
            infoBackupData->backupAnnotation = NULL;
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
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
infoBackupLoadFileCallback(void *const data, const unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    InfoBackupLoadFileData *const loadData = data;
    bool result = false;

    if (try < 2)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Construct filename based on try
            const String *const fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strZ(loadData->fileName));

            // Attempt to load the file
            IoRead *const read = storageReadIo(storageNewReadP(loadData->storage, fileName));
            cipherBlockFilterGroupAdd(ioReadFilterGroup(read), loadData->cipherType, cipherModeDecrypt, loadData->cipherPass);

            MEM_CONTEXT_BEGIN(loadData->memContext)
            {
                loadData->infoBackup = infoBackupNewLoad(read);
                result = true;
            }
            MEM_CONTEXT_END();
        }
        MEM_CONTEXT_TEMP_END();
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
