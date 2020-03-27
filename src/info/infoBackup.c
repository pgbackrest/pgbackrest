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
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
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
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_PRIOR_VAR,             "backup-prior");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR,         "backup-reference");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR,   "backup-timestamp-start");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR,    "backup-timestamp-stop");
VARIANT_STRDEF_STATIC(INFO_BACKUP_KEY_BACKUP_TYPE_VAR,              "backup-type");
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
    FUNCTION_TEST_VOID();

    InfoBackup *this = memNew(sizeof(InfoBackup));

    *this = (InfoBackup)
    {
        .memContext = memContextCurrent(),
        .backup = lstNewP(sizeof(InfoBackupData), .comparator =  lstComparatorStr),
    };

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create new object without loading it from a file
***********************************************************************************************************************************/
InfoBackup *
infoBackupNew(unsigned int pgVersion, uint64_t pgSystemId, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    ASSERT(pgVersion > 0 && pgSystemId > 0);

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoBackup")
    {
        this = infoBackupNewInternal();

        // Initialize the pg data
        this->infoPg = infoPgNew(infoPgBackup, cipherPassSub);
        infoBackupPgSet(this, pgVersion, pgSystemId);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
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

    InfoBackup *infoBackup = (InfoBackup *)data;

    // Process current backup list
    if (strEq(section, INFO_BACKUP_SECTION_BACKUP_CURRENT_STR))
    {
        const KeyValue *backupKv = jsonToKv(value);

        MEM_CONTEXT_BEGIN(lstMemContext(infoBackup->backup))
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
                .backupPgId = cvtZToUInt(strPtr(varStrForce(kvGet(backupKv, INFO_KEY_DB_ID_VAR)))),

                // When reading timestamps, read as uint64 to ensure always positive value (guarantee no backups before 1970)
                .backupTimestampStart = (time_t)varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR)),
                .backupTimestampStop= (time_t)varUInt64(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR)),
                .backupType = varStrForce(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_TYPE_VAR)),

                // Possible NULL values
                .backupArchiveStart = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_START_VAR))),
                .backupArchiveStop = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP_VAR))),
                .backupPrior = strDup(varStr(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_PRIOR_VAR))),
                .backupReference =
                    kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR) != NULL ?
                        strLstNewVarLst(varVarLst(kvGet(backupKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR))) : NULL,

                // Options
                .optionArchiveCheck = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK_VAR)),
                .optionArchiveCopy = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ARCHIVE_COPY_VAR)),
                .optionBackupStandby = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_BACKUP_STANDBY_VAR)),
                .optionChecksumPage = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE_VAR)),
                .optionCompress = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_COMPRESS_VAR)),
                .optionHardlink = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_HARDLINK_VAR)),
                .optionOnline = varBool(kvGet(backupKv, INFO_BACKUP_KEY_OPT_ONLINE_VAR)),
            };

            // Add the backup data to the list
            lstAdd(infoBackup->backup, &infoBackupData);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

static InfoBackup *
infoBackupNewLoad(IoRead *read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoBackup")
    {
        this = infoBackupNewInternal();
        this->infoPg = infoPgNewLoad(read, infoPgBackup, infoBackupLoadCallback, this);
    }
    MEM_CONTEXT_NEW_END();

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

            KeyValue *backupDataKv = kvNew();
            kvPut(backupDataKv, VARSTR(INFO_KEY_FORMAT_STR), VARUINT(backupData.backrestFormat));
            kvPut(backupDataKv, VARSTR(INFO_KEY_VERSION_STR), VARSTR(backupData.backrestVersion));

            kvPut(backupDataKv, INFO_KEY_DB_ID_VAR, VARUINT(backupData.backupPgId));

            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_START_VAR, VARSTR(backupData.backupArchiveStart));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_ARCHIVE_STOP_VAR, VARSTR(backupData.backupArchiveStop));

            if (backupData.backupPrior != NULL)
                kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_PRIOR_VAR, VARSTR(backupData.backupPrior));

            if (backupData.backupReference != NULL)
            {
                kvPut(
                    backupDataKv, INFO_BACKUP_KEY_BACKUP_REFERENCE_VAR, varNewVarLst(varLstNewStrLst(backupData.backupReference)));
            }

            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_VAR, VARUINT64(backupData.backupInfoRepoSize));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_VAR, VARUINT64(backupData.backupInfoRepoSizeDelta));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_VAR, VARUINT64(backupData.backupInfoSize));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_VAR, VARUINT64(backupData.backupInfoSizeDelta));

            // When storing time_t treat as signed int to avoid casting
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_START_VAR, VARINT64(backupData.backupTimestampStart));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_TIMESTAMP_STOP_VAR, VARINT64(backupData.backupTimestampStop));
            kvPut(backupDataKv, INFO_BACKUP_KEY_BACKUP_TYPE_VAR, VARSTR(backupData.backupType));

            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_ARCHIVE_CHECK_VAR, VARBOOL(backupData.optionArchiveCheck));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_ARCHIVE_COPY_VAR, VARBOOL(backupData.optionArchiveCopy));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_BACKUP_STANDBY_VAR, VARBOOL(backupData.optionBackupStandby));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_CHECKSUM_PAGE_VAR, VARBOOL(backupData.optionChecksumPage));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_COMPRESS_VAR, VARBOOL(backupData.optionCompress));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_HARDLINK_VAR, VARBOOL(backupData.optionHardlink));
            kvPut(backupDataKv, INFO_BACKUP_KEY_OPT_ONLINE_VAR, VARBOOL(backupData.optionOnline));

            infoSaveValue(
                infoSaveData, INFO_BACKUP_SECTION_BACKUP_CURRENT_STR, backupData.backupLabel, jsonFromKv(backupDataKv));
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
infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
    FUNCTION_LOG_END();

    this->infoPg = infoPgSet(this->infoPg, infoPgBackup, pgVersion, pgSystemId);

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

    FUNCTION_TEST_RETURN(lstSize(this->backup));
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

/**********************************************************************************************************************************/
InfoBackupData *
infoBackupDataByLabel(const InfoBackup *this, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(backupLabel != NULL);

    FUNCTION_TEST_RETURN(lstFind(this->backup, &backupLabel));
}

/***********************************************************************************************************************************
Add a backup to the current list
***********************************************************************************************************************************/
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

        // Calculate backup sizes and references
        uint64_t backupSize = 0;
        uint64_t backupSizeDelta = 0;
        uint64_t backupRepoSize = 0;
        uint64_t backupRepoSizeDelta = 0;
        StringList *referenceList = strLstNew();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile *file = manifestFile(manifest, fileIdx);

            backupSize += file->size;
            backupRepoSize += file->sizeRepo > 0 ? file->sizeRepo : file->size;

            // If a reference to a file exists, then it is in a previous backup and the delta calculation was already done
            if (file->reference != NULL)
                strLstAddIfMissing(referenceList, file->reference);
            else
            {
                backupSizeDelta += file->size;
                backupRepoSizeDelta += file->sizeRepo > 0 ? file->sizeRepo : file->size;
            }
        }

        MEM_CONTEXT_BEGIN(lstMemContext(this->backup))
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
                .backupType = backupTypeStr(manData->backupType),

                .backupArchiveStart = strDup(manData->archiveStart),
                .backupArchiveStop = strDup(manData->archiveStop),

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
            lstAdd(this->backup, &infoBackupData);

            // Ensure the list is sorted ascending by the backupLabel
            lstSort(this->backup, sortOrderAsc);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
            lstRemoveIdx(this->backup, idx);
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
Helper function to load backup info files
***********************************************************************************************************************************/
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
        const String *fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strPtr(loadData->fileName));

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
        FUNCTION_LOG_PARAM(ENUM, cipherType);
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
        const char *fileNamePath = strPtr(storagePathP(storage, fileName));

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

/***********************************************************************************************************************************
Load backup info and update it by adding valid backups from the repo or removing backups no longer in the repo
***********************************************************************************************************************************/
InfoBackup *
infoBackupLoadFileReconstruct(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
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
            String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabel));

            // If the manifest does not exist on disk and this backup has not already been deleted from the current list in the
            // infoBackup object, then remove it and its dependencies
            if (!storageExistsP(storage, manifestFileName) && infoBackupDataByLabel(infoBackup, backupLabel) != NULL)
            {
                StringList *backupList = strLstSort(infoBackupDataDependentList(infoBackup, backupLabel), sortOrderDesc);
                for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
                {
                    String *removeBackup = strLstGet(backupList, backupIdx);

                    LOG_WARN_FMT("backup '%s' missing manifest removed from " INFO_BACKUP_FILE, strPtr(removeBackup));

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
                String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabel));

                // Check if a completed backup exists (backup.manifest only - ignore .copy)
                if (storageExistsP(storage, manifestFileName))
                {
                    bool found = false;
                    const Manifest *manifest = manifestLoadFile(
                        storage, manifestFileName, cipherType, infoPgCipherPass(infoBackup->infoPg));
                    const ManifestData *manData = manifestData(manifest);

                    // If the pg data for the manifest exists in the history, then add it to current, but if something doesn't match
                    // then warn that the backup is not valid
                    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoBackup->infoPg); pgIdx++)
                    {
                        InfoPgData pgHistory = infoPgData(infoBackup->infoPg, pgIdx);

                        // If there is an exact match with the history, system and version and there is no backup-prior dependency
                        // or there is a backup-prior and it is in the list, then add this backup to the current backup list
                        if (manData->pgId == pgHistory.id && manData->pgSystemId == pgHistory.systemId &&
                            manData->pgVersion == pgHistory.version &&
                            (manData->backupLabelPrior == NULL ||
                                infoBackupDataByLabel(infoBackup, manData->backupLabelPrior) != NULL))
                        {
                            LOG_WARN_FMT("backup '%s' found in repository added to " INFO_BACKUP_FILE, strPtr(backupLabel));
                            infoBackupDataAdd(infoBackup, manifest);
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        LOG_WARN_FMT("invalid backup '%s' cannot be added to current backups", strPtr(manData->backupLabel));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, infoBackup);
}

/***********************************************************************************************************************************
Helper function to save backup info files
***********************************************************************************************************************************/
void
infoBackupSaveFile(
    InfoBackup *infoBackup, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Save the file
        IoWrite *write = storageWriteIo(storageNewWriteP(storage, fileName));
        cipherBlockFilterGroupAdd(ioWriteFilterGroup(write), cipherType, cipherModeEncrypt, cipherPass);
        infoBackupSave(infoBackup, write);

        // Make a copy of the file
        storageCopy(
            storageNewReadP(storage, fileName), storageNewWriteP(storage, strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
infoBackupDataToLog(const InfoBackupData *this)
{
    return strNewFmt("{label: %s, pgId: %u}", strPtr(this->backupLabel), this->backupPgId);
}
