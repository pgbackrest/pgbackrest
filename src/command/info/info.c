/***********************************************************************************************************************************
Info Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/info/info.h"
#include "command/lock.h"
#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Naming convention: <sectionname>_KEY_<keyname>_VAR. If the key exists in multiple sections, then <sectionname>_ is omitted.
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MIN_VAR,                          "min");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MAX_VAR,                          "max");
VARIANT_STRDEF_STATIC(BACKREST_KEY_FORMAT_VAR,                      "format");
VARIANT_STRDEF_STATIC(BACKREST_KEY_VERSION_VAR,                     "version");
VARIANT_STRDEF_STATIC(BACKUP_KEY_ANNOTATION_VAR,                    "annotation");
VARIANT_STRDEF_STATIC(BACKUP_KEY_BACKREST_VAR,                      "backrest");
VARIANT_STRDEF_STATIC(BACKUP_KEY_ERROR_VAR,                         "error");
VARIANT_STRDEF_STATIC(BACKUP_KEY_ERROR_LIST_VAR,                    "error-list");
VARIANT_STRDEF_STATIC(BACKUP_KEY_DATABASE_REF_VAR,                  "database-ref");
VARIANT_STRDEF_STATIC(BACKUP_KEY_INFO_VAR,                          "info");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LABEL_VAR,                         "label");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LINK_VAR,                          "link");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LSN_VAR,                           "lsn");
VARIANT_STRDEF_STATIC(BACKUP_KEY_PRIOR_VAR,                         "prior");
VARIANT_STRDEF_STATIC(BACKUP_KEY_REFERENCE_VAR,                     "reference");
VARIANT_STRDEF_STATIC(BACKUP_KEY_TABLESPACE_VAR,                    "tablespace");
VARIANT_STRDEF_STATIC(BACKUP_KEY_TIMESTAMP_VAR,                     "timestamp");
VARIANT_STRDEF_STATIC(BACKUP_KEY_TYPE_VAR,                          "type");
VARIANT_STRDEF_STATIC(DB_KEY_ID_VAR,                                "id");
VARIANT_STRDEF_STATIC(DB_KEY_SYSTEM_ID_VAR,                         "system-id");
VARIANT_STRDEF_STATIC(DB_KEY_VERSION_VAR,                           "version");
VARIANT_STRDEF_STATIC(INFO_KEY_REPOSITORY_VAR,                      "repository");
VARIANT_STRDEF_STATIC(KEY_ARCHIVE_VAR,                              "archive");
VARIANT_STRDEF_STATIC(KEY_CIPHER_VAR,                               "cipher");
VARIANT_STRDEF_STATIC(KEY_DATABASE_VAR,                             "database");
VARIANT_STRDEF_STATIC(KEY_DELTA_VAR,                                "delta");
VARIANT_STRDEF_STATIC(KEY_DELTA_MAP_VAR,                            "delta-map");
VARIANT_STRDEF_STATIC(KEY_DESTINATION_VAR,                          "destination");
VARIANT_STRDEF_STATIC(KEY_NAME_VAR,                                 "name");
VARIANT_STRDEF_STATIC(KEY_OID_VAR,                                  "oid");
VARIANT_STRDEF_STATIC(KEY_REPO_KEY_VAR,                             "repo-key");
VARIANT_STRDEF_STATIC(KEY_SIZE_VAR,                                 "size");
VARIANT_STRDEF_STATIC(KEY_SIZE_MAP_VAR,                             "size-map");
VARIANT_STRDEF_STATIC(KEY_START_VAR,                                "start");
VARIANT_STRDEF_STATIC(KEY_STOP_VAR,                                 "stop");
VARIANT_STRDEF_STATIC(REPO_KEY_KEY_VAR,                             "key");
VARIANT_STRDEF_STATIC(STANZA_KEY_BACKUP_VAR,                        "backup");
VARIANT_STRDEF_STATIC(STANZA_KEY_REPO_VAR,                          "repo");
VARIANT_STRDEF_STATIC(STANZA_KEY_STATUS_VAR,                        "status");
VARIANT_STRDEF_STATIC(STANZA_KEY_DB_VAR,                            "db");
VARIANT_STRDEF_STATIC(STATUS_KEY_CODE_VAR,                          "code");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_VAR,                          "lock");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_VAR,                   "backup");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_HELD_VAR,              "held");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_PERCENT_COMPLETE_VAR,  "pct-cplt");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_SIZE_COMPLETE_VAR,     "size-cplt");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_SIZE_VAR,              "size");
VARIANT_STRDEF_STATIC(STATUS_KEY_MESSAGE_VAR,                       "message");

#define INFO_STANZA_STATUS_OK                                       "ok"
#define INFO_STANZA_STATUS_ERROR                                    "error"
#define INFO_STANZA_MIXED                                           "mixed"

#define INFO_STANZA_STATUS_CODE_OK                                  0
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_OK_STR,                    "ok");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH                 1
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR,   "missing stanza path");
#define INFO_STANZA_STATUS_CODE_NO_BACKUP                           2
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR,             "no valid backups");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA                 3
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR,   "missing stanza data");
#define INFO_STANZA_STATUS_CODE_MIXED                               4
// If the cipher or status of the stanza is different across repos, then the overall cipher or status message is mixed
STRING_STATIC(INFO_STANZA_MESSAGE_MIXED_STR,                        "different across repos");
#define INFO_STANZA_STATUS_CODE_PG_MISMATCH                         5
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_PG_MISMATCH_STR,           "database mismatch across repos");
#define INFO_STANZA_STATUS_CODE_BACKUP_MISSING                      6
STRING_STATIC(INFO_STANZA_STATUS_CODE_BACKUP_MISSING_STR,           "requested backup not found");
#define INFO_STANZA_STATUS_CODE_OTHER                               99
#define INFO_STANZA_STATUS_MESSAGE_OTHER                            "other"
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_OTHER_STR,                 INFO_STANZA_STATUS_MESSAGE_OTHER);
STRING_STATIC(INFO_STANZA_INVALID_STR,                              "[invalid]");

#define INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP                      "backup/expire running"

/***********************************************************************************************************************************
Data types and structures
***********************************************************************************************************************************/
// Repository information for a stanza
typedef struct InfoRepoData
{
    unsigned int key;                                               // User-defined repo key
    CipherType cipher;                                              // Encryption type
    const String *cipherPass;                                       // Passphrase if the repo is encrypted (else NULL)
    int stanzaStatus;                                               // Status code of the stanza on this repo
    unsigned int backupIdx;                                         // Index of the next backup that may be a candidate for sorting
    InfoBackup *backupInfo;                                         // Contents of the backup.info file of the stanza on this repo
    InfoArchive *archiveInfo;                                       // Contents of the archive.info file of the stanza on this repo
    Manifest *manifest;                                             // Contents of manifest if backup requested and is on this repo
    String *error;                                                  // Formatted error
} InfoRepoData;

#define FUNCTION_LOG_INFO_REPO_DATA_TYPE                                                                                           \
    InfoRepoData *
#define FUNCTION_LOG_INFO_REPO_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    objNameToLog(value, "InfoRepoData", buffer, bufferSize)

// Stanza with repository list of information for each repository
typedef struct InfoStanzaRepo
{
    const String *name;                                             // Name of the stanza
    uint64_t currentPgSystemId;                                     // Current postgres system id for the stanza
    unsigned int currentPgVersion;                                  // Current postgres version for the stanza
    bool backupLockHeld;                                            // Is backup lock held on the system where info command is run?
    uint64_t sizeComplete;                                          // Completed size of the backup in bytes
    uint64_t size;                                                  // Total size of the backup in bytes
    InfoRepoData *repoList;                                         // List of configured repositories
} InfoStanzaRepo;

#define FUNCTION_LOG_INFO_STANZA_REPO_TYPE                                                                                         \
    InfoStanzaRepo *
#define FUNCTION_LOG_INFO_STANZA_REPO_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "InfoStanzaRepo", buffer, bufferSize)

// Group all databases with the same system-id and version together regardless of db-id or repo
typedef struct DbGroup
{
    uint64_t systemId;                                              // Postgres database system id
    const String *version;                                          // Postgres database version
    bool current;                                                   // Is this the current postgres database?
    String *archiveMin;                                             // Minimum WAL found for this database over all repositories
    String *archiveMax;                                             // Maximum WAL found for this database over all repositories
    VariantList *backupList;                                        // List of backups found for this database over all repositories
} DbGroup;

#define FUNCTION_LOG_DB_GROUP_TYPE                                                                                                 \
    DbGroup *
#define FUNCTION_LOG_DB_GROUP_FORMAT(value, buffer, bufferSize)                                                                    \
    objNameToLog(value, "DbGroup", buffer, bufferSize)

/***********************************************************************************************************************************
Helper function for reporting errors
***********************************************************************************************************************************/
static void
infoStanzaErrorAdd(InfoRepoData *const repoList, const ErrorType *const type, const String *const message)
{
    repoList->stanzaStatus = INFO_STANZA_STATUS_CODE_OTHER;
    repoList->error = strNewFmt("[%s] %s", errorTypeName(type), strZ(message));

    // Free the info objects for this stanza since we cannot process it
    infoBackupFree(repoList->backupInfo);
    infoArchiveFree(repoList->archiveInfo);
    manifestFree(repoList->manifest);
    repoList->backupInfo = NULL;
    repoList->archiveInfo = NULL;
    repoList->manifest = NULL;
}

/***********************************************************************************************************************************
Set the overall error status code and message for the stanza to the code and message passed
***********************************************************************************************************************************/
static void
stanzaStatus(const int code, const InfoStanzaRepo *const stanzaData, const Variant *const stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(INFO_STANZA_REPO, stanzaData);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
    FUNCTION_TEST_END();

    ASSERT((code >= 0 && code <= 6) || code == 99);
    ASSERT(stanzaData != NULL);
    ASSERT(stanzaInfo != NULL);

    KeyValue *const statusKv = kvPutKv(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR);

    kvPut(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));

    switch (code)
    {
        case INFO_STANZA_STATUS_CODE_OK:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_OK_STR));
            break;

        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR));
            break;

        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR));
            break;

        case INFO_STANZA_STATUS_CODE_NO_BACKUP:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR));
            break;

        case INFO_STANZA_STATUS_CODE_MIXED:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_MESSAGE_MIXED_STR));
            break;

        case INFO_STANZA_STATUS_CODE_PG_MISMATCH:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_PG_MISMATCH_STR));
            break;

        case INFO_STANZA_STATUS_CODE_BACKUP_MISSING:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_CODE_BACKUP_MISSING_STR));
            break;

        case INFO_STANZA_STATUS_CODE_OTHER:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_OTHER_STR));
            break;
    }

    // Construct a specific lock part
    KeyValue *const lockKv = kvPutKv(statusKv, STATUS_KEY_LOCK_VAR);
    KeyValue *const backupLockKv = kvPutKv(lockKv, STATUS_KEY_LOCK_BACKUP_VAR);
    kvPut(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR, VARBOOL(stanzaData->backupLockHeld));

    if (stanzaData->size != 0)
    {
        kvPut(backupLockKv, STATUS_KEY_LOCK_BACKUP_SIZE_COMPLETE_VAR, VARUINT64(stanzaData->sizeComplete));
        kvPut(backupLockKv, STATUS_KEY_LOCK_BACKUP_SIZE_VAR, VARUINT64(stanzaData->size));

        if (cfgOptionStrId(cfgOptOutput) != CFGOPTVAL_OUTPUT_JSON)
        {
            kvPut(
                backupLockKv, STATUS_KEY_LOCK_BACKUP_PERCENT_COMPLETE_VAR,
                VARUINT((unsigned int)(((double)stanzaData->sizeComplete / (double)stanzaData->size) * 10000)));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the error status code and message for the stanza on the repo to the code and message passed
***********************************************************************************************************************************/
static void
repoStanzaStatus(const int code, const Variant *const repoStanzaInfo, const InfoRepoData *const repoData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(VARIANT, repoStanzaInfo);
        FUNCTION_TEST_PARAM(INFO_REPO_DATA, repoData);
    FUNCTION_TEST_END();

    ASSERT((code >= 0 && code <= 3) || code == 99 || code == 6);
    ASSERT(repoStanzaInfo != NULL);

    KeyValue *const statusKv = kvPutKv(varKv(repoStanzaInfo), STANZA_KEY_STATUS_VAR);

    kvPut(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));

    switch (code)
    {
        case INFO_STANZA_STATUS_CODE_OK:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_OK_STR));
            break;

        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR));
            break;

        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR));
            break;

        case INFO_STANZA_STATUS_CODE_NO_BACKUP:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR));
            break;

        case INFO_STANZA_STATUS_CODE_BACKUP_MISSING:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_CODE_BACKUP_MISSING_STR));
            break;

        case INFO_STANZA_STATUS_CODE_OTHER:
            kvPut(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(repoData->error));
            break;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the data for the archive section of the stanza for the database info from the backup.info file
***********************************************************************************************************************************/
static void
archiveDbList(
    const String *const stanza, const InfoPgData *const pgData, VariantList *const archiveSection, const InfoArchive *const info,
    const bool currentDb, const unsigned int repoIdx, const unsigned int repoKey)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
        FUNCTION_TEST_PARAM(VARIANT_LIST, archiveSection);
        FUNCTION_TEST_PARAM(BOOL, currentDb);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(UINT, repoKey);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(stanza != NULL);
    ASSERT(pgData != NULL);
    ASSERT(archiveSection != NULL);

    // With multiple DB versions, the backup.info history-id may not be the same as archive.info history-id, so the archive path
    // must be built by retrieving the archive id given the db version and system id of the backup.info file. If there is no match,
    // an error will be thrown.
    const String *const archiveId = infoArchiveIdHistoryMatch(info, pgData->id, pgData->version, pgData->systemId);

    String *const archivePath = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), strZ(archiveId));
    const String *archiveStart = NULL;
    const String *archiveStop = NULL;
    Variant *const archiveInfo = varNewKv(kvNew());
    const Storage *const storageRepo = storageRepoIdx(repoIdx);

    // Get a list of WAL directories in the archive repo from oldest to newest, if any exist
    const StringList *const walDir = strLstSort(
        storageListP(storageRepo, archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);

    if (!strLstEmpty(walDir))
    {
        // Not every WAL dir has WAL files so check each
        for (unsigned int idx = 0; idx < strLstSize(walDir); idx++)
        {
            // Get a list of all WAL in this WAL dir and sort the list from oldest to newest to get the oldest starting WAL archived
            // for this db
            const StringList *const list = strLstSort(
                storageListP(
                    storageRepo, strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
                    .expression = WAL_SEGMENT_FILE_REGEXP_STR),
                sortOrderAsc);

            // If wal segments are found, get the oldest one as the archive start
            if (!strLstEmpty(list))
            {
                archiveStart = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }

        // Iterate through the directory list in reverse processing newest first. Cast comparison to an int for readability.
        for (unsigned int idx = strLstSize(walDir) - 1; (int)idx >= 0; idx--)
        {
            // Get a list of all WAL in this WAL dir and sort the list from newest to oldest to get the newest ending WAL archived
            // for this db
            const StringList *const list = strLstSort(
                storageListP(
                    storageRepo, strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
                    .expression = WAL_SEGMENT_FILE_REGEXP_STR),
                sortOrderDesc);

            // If wal segments are found, get the newest one as the archive stop
            if (!strLstEmpty(list))
            {
                archiveStop = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }
    }

    // If there is an archive or the database is the current database then store it
    if (currentDb || archiveStart != NULL)
    {
        // Add empty database section to archiveInfo and then fill in database id from the backup.info
        KeyValue *const databaseInfo = kvPutKv(varKv(archiveInfo), KEY_DATABASE_VAR);

        kvPut(databaseInfo, DB_KEY_ID_VAR, VARUINT(pgData->id));
        kvPut(databaseInfo, KEY_REPO_KEY_VAR, VARUINT(repoKey));

        kvPut(varKv(archiveInfo), DB_KEY_ID_VAR, VARSTR(archiveId));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MIN_VAR, (archiveStart != NULL ? VARSTR(archiveStart) : (Variant *)NULL));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MAX_VAR, (archiveStop != NULL ? VARSTR(archiveStop) : (Variant *)NULL));

        varLstAdd(archiveSection, archiveInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Add the backup data to the backup section
***********************************************************************************************************************************/
static void
backupListAdd(
    VariantList *backupSection, InfoBackupData *backupData, const String *backupLabel, InfoRepoData *repoData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);           // The section to add the backup data to
        FUNCTION_TEST_PARAM_P(INFO_BACKUP_DATA, backupData);        // The data for the backup
        FUNCTION_TEST_PARAM(STRING, backupLabel);                   // Backup label to filter if requested by the user
        FUNCTION_TEST_PARAM(INFO_REPO_DATA, repoData);              // The repo data where this backup is located
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(backupSection != NULL);
    ASSERT(backupData != NULL);
    ASSERT(repoData != NULL);

    Variant *const backupInfo = varNewKv(kvNew());

    // Flags used to decide what data to add
    const bool outputJson = cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON;

    // main keys
    kvPut(varKv(backupInfo), BACKUP_KEY_LABEL_VAR, VARSTR(backupData->backupLabel));
    kvPut(varKv(backupInfo), BACKUP_KEY_TYPE_VAR, VARSTR(strIdToStr(backupData->backupType)));
    kvPut(
        varKv(backupInfo), BACKUP_KEY_PRIOR_VAR,
        (backupData->backupPrior != NULL ? VARSTR(backupData->backupPrior) : NULL));
    kvPut(
        varKv(backupInfo), BACKUP_KEY_REFERENCE_VAR,
        (backupData->backupReference != NULL ? varNewVarLst(varLstNewStrLst(backupData->backupReference)) : NULL));

    // archive section
    KeyValue *const archiveInfo = kvPutKv(varKv(backupInfo), KEY_ARCHIVE_VAR);

    kvPut(
        archiveInfo, KEY_START_VAR,
        (backupData->backupArchiveStart != NULL ? VARSTR(backupData->backupArchiveStart) : NULL));
    kvPut(
        archiveInfo, KEY_STOP_VAR,
        (backupData->backupArchiveStop != NULL ? VARSTR(backupData->backupArchiveStop) : NULL));

    // backrest section
    KeyValue *const backrestInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_BACKREST_VAR);

    kvPut(backrestInfo, BACKREST_KEY_FORMAT_VAR, VARUINT(backupData->backrestFormat));
    kvPut(backrestInfo, BACKREST_KEY_VERSION_VAR, VARSTR(backupData->backrestVersion));

    // database section
    KeyValue *const dbInfo = kvPutKv(varKv(backupInfo), KEY_DATABASE_VAR);

    kvPut(dbInfo, DB_KEY_ID_VAR, VARUINT(backupData->backupPgId));
    kvPut(dbInfo, KEY_REPO_KEY_VAR, VARUINT(repoData->key));

    // info section
    KeyValue *const infoInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_INFO_VAR);

    kvPut(infoInfo, KEY_SIZE_VAR, VARUINT64(backupData->backupInfoSize));
    kvPut(infoInfo, KEY_DELTA_VAR, VARUINT64(backupData->backupInfoSizeDelta));

    // info:repository section
    KeyValue *const repoInfo = kvPutKv(infoInfo, INFO_KEY_REPOSITORY_VAR);

    if (backupData->backupInfoRepoSizeMap == NULL)
        kvPut(repoInfo, KEY_SIZE_VAR, VARUINT64(backupData->backupInfoRepoSize));

    kvPut(repoInfo, KEY_DELTA_VAR, VARUINT64(backupData->backupInfoRepoSizeDelta));

    if (outputJson && backupData->backupInfoRepoSizeMap != NULL)
    {
        kvPut(repoInfo, KEY_SIZE_MAP_VAR, backupData->backupInfoRepoSizeMap);
        kvPut(repoInfo, KEY_DELTA_MAP_VAR, backupData->backupInfoRepoSizeMapDelta);
    }

    // timestamp section
    KeyValue *const timeInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_TIMESTAMP_VAR);

    // time_t is generally a signed int so cast it to uint64 since it can never be negative (before 1970) in our system
    kvPut(timeInfo, KEY_START_VAR, VARUINT64((uint64_t)backupData->backupTimestampStart));
    kvPut(timeInfo, KEY_STOP_VAR, VARUINT64((uint64_t)backupData->backupTimestampStop));

    // Report errors only if the error status is known
    if (backupData->backupError != NULL)
        kvPut(varKv(backupInfo), BACKUP_KEY_ERROR_VAR, backupData->backupError);

    // Add start/stop backup lsn info to json output or --set text
    if ((outputJson || backupLabel != NULL) && backupData->backupLsnStart != NULL && backupData->backupLsnStop != NULL)
    {
        KeyValue *const lsnInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_LSN_VAR);

        kvPut(lsnInfo, KEY_START_VAR, VARSTR(backupData->backupLsnStart));
        kvPut(lsnInfo, KEY_STOP_VAR, VARSTR(backupData->backupLsnStop));
    }

    // Add annotations to json output or --set text
    if ((outputJson || backupLabel != NULL) && backupData->backupAnnotation != NULL)
        kvPut(varKv(backupInfo), BACKUP_KEY_ANNOTATION_VAR, backupData->backupAnnotation);

    // If a backup label was specified and this is that label, then get the data from the loaded manifest
    if (backupLabel != NULL)
    {
        // Get the list of databases in this backup
        VariantList *const databaseSection = varLstNew();

        for (unsigned int dbIdx = 0; dbIdx < manifestDbTotal(repoData->manifest); dbIdx++)
        {
            const ManifestDb *const db = manifestDb(repoData->manifest, dbIdx);

            // Do not display template databases
            if (!pgDbIsTemplate(db->name))
            {
                Variant *const database = varNewKv(kvNew());

                kvPut(varKv(database), KEY_NAME_VAR, VARSTR(db->name));
                kvPut(varKv(database), KEY_OID_VAR, VARUINT64(db->id));
                varLstAdd(databaseSection, database);
            }
        }

        // Add the database section even if none found
        kvPut(varKv(backupInfo), BACKUP_KEY_DATABASE_REF_VAR, varNewVarLst(databaseSection));

        // Get symlinks and tablespaces
        VariantList *const linkSection = varLstNew();
        VariantList *const tablespaceSection = varLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(repoData->manifest); targetIdx++)
        {
            const ManifestTarget *const target = manifestTarget(repoData->manifest, targetIdx);
            Variant *const link = varNewKv(kvNew());
            Variant *const tablespace = varNewKv(kvNew());

            if (target->type == manifestTargetTypeLink)
            {
                if (target->tablespaceName != NULL)
                {
                    kvPut(varKv(tablespace), KEY_NAME_VAR, VARSTR(target->tablespaceName));
                    kvPut(varKv(tablespace), KEY_DESTINATION_VAR, VARSTR(target->path));
                    kvPut(varKv(tablespace), KEY_OID_VAR, VARUINT64(target->tablespaceId));
                    varLstAdd(tablespaceSection, tablespace);
                }
                else if (target->file != NULL)
                {
                    kvPut(varKv(link), KEY_NAME_VAR, varNewStr(target->file));
                    kvPut(
                        varKv(link), KEY_DESTINATION_VAR,
                        varNewStr(strNewFmt("%s/%s", strZ(target->path), strZ(target->file))));

                    varLstAdd(linkSection, link);
                }
                else
                {
                    kvPut(varKv(link), KEY_NAME_VAR, VARSTR(manifestPathPg(target->name)));
                    kvPut(varKv(link), KEY_DESTINATION_VAR, VARSTR(target->path));
                    varLstAdd(linkSection, link);
                }
            }
        }

        kvPut(varKv(backupInfo), BACKUP_KEY_LINK_VAR, (!varLstEmpty(linkSection) ? varNewVarLst(linkSection) : NULL));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_TABLESPACE_VAR,
            (!varLstEmpty(tablespaceSection) ? varNewVarLst(tablespaceSection) : NULL));

        // Get the list of files with an error
        VariantList *const checksumPageErrorList = varLstNew();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(repoData->manifest); fileIdx++)
        {
            const ManifestFile file = manifestFile(repoData->manifest, fileIdx);

            if (file.checksumPageError)
                varLstAdd(checksumPageErrorList, varNewStr(manifestPathPg(file.name)));
        }

        if (!varLstEmpty(checksumPageErrorList))
        {
            kvPut(varKv(backupInfo), BACKUP_KEY_ERROR_LIST_VAR, varNewVarLst(checksumPageErrorList));

            // It is possible that backup-error is not set in backup.info but there are errors in manifest because backup-error was
            // added in a later version than manifest errors. However, it should not be possible for backup-error to be present but
            // false if there are errors in the manifest. In production this condition will be ignored and error set to true.
            ASSERT(
                kvGet(varKv(backupInfo), BACKUP_KEY_ERROR_VAR) == NULL || varBool(kvGet(varKv(backupInfo), BACKUP_KEY_ERROR_VAR)));
            kvPut(varKv(backupInfo), BACKUP_KEY_ERROR_VAR, BOOL_TRUE_VAR);
        }

        manifestFree(repoData->manifest);
        repoData->manifest = NULL;
    }

    varLstAdd(backupSection, backupInfo);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
For each current backup in the backup.info file of the stanza, set the data for the backup section
***********************************************************************************************************************************/
static void
backupList(
    VariantList *const backupSection, InfoStanzaRepo *const stanzaData, const String *const backupLabel,
    const unsigned int repoIdxMin, const unsigned int repoIdxMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);           // The section to add the backup data to
        FUNCTION_TEST_PARAM(INFO_STANZA_REPO, stanzaData);          // The data for the stanza
        FUNCTION_TEST_PARAM(STRING, backupLabel);                   // Backup label to filter if requested by the user
        FUNCTION_TEST_PARAM(UINT, repoIdxMin);                      // The start index of the repo array to begin checking
        FUNCTION_TEST_PARAM(UINT, repoIdxMax);                      // The index of the last repo to check
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(backupSection != NULL);
    ASSERT(stanzaData != NULL);

    unsigned int backupNextRepoIdx = 0;
    unsigned int backupTotal = 0;
    unsigned int backupTotalProcessed = 0;

    // Get the number of backups to be processed
    for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
    {
        const InfoRepoData *const repoData = &stanzaData->repoList[repoIdx];

        if (repoData->backupInfo != NULL && infoBackupDataTotal(repoData->backupInfo) > 0)
            backupTotal += infoBackupDataTotal(repoData->backupInfo);
    }

    // Process any backups
    while (backupTotalProcessed < backupTotal)
    {
        time_t backupNextTime = 0;

        // Backups are sorted for each repo, so iterate over the lists to create a single list ordered by backup-timestamp-stop
        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            const InfoRepoData *const repoData = &stanzaData->repoList[repoIdx];

            // If there are current backups on this repo for this stanza and the end of this backup list has not been reached
            // determine the next backup for display
            if (repoData->backupInfo != NULL && infoBackupDataTotal(repoData->backupInfo) > 0 &&
                repoData->backupIdx < infoBackupDataTotal(repoData->backupInfo))
            {
                InfoBackupData backupData = infoBackupData(repoData->backupInfo, repoData->backupIdx);

                // See if this backup should be next in the list, ordering from oldest to newest
                if (backupNextTime == 0 || backupData.backupTimestampStop < backupNextTime)
                {
                    backupNextTime = backupData.backupTimestampStop;
                    backupNextRepoIdx = repoIdx;
                }
            }
        }

        InfoRepoData *const repoData = &stanzaData->repoList[backupNextRepoIdx];
        InfoBackupData backupData = infoBackupData(repoData->backupInfo, repoData->backupIdx);
        repoData->backupIdx++;
        backupTotalProcessed++;

        // Don't add the backup data to the backup section if a backup label was specified but this is not it
        if (backupLabel != NULL && !strEq(backupData.backupLabel, backupLabel))
            continue;

        // Add the backup data to the backup section
        if (!cfgOptionTest(cfgOptType) || cfgOptionStrId(cfgOptType) == backupData.backupType)
            backupListAdd(backupSection, &backupData, backupLabel, repoData);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the stanza data for each stanza found in the repo
***********************************************************************************************************************************/
static VariantList *
stanzaInfoList(
    List *const stanzaRepoList, const String *const backupLabel, const unsigned int repoIdxMin, const unsigned int repoIdxMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, stanzaRepoList);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(UINT, repoIdxMin);
        FUNCTION_TEST_PARAM(UINT, repoIdxMax);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(stanzaRepoList != NULL);

    VariantList *const result = varLstNew();

    // Sort the list of stanzas
    lstSort(stanzaRepoList, sortOrderAsc);

    // Process each stanza requested
    for (unsigned int idx = 0; idx < lstSize(stanzaRepoList); idx++)
    {
        InfoStanzaRepo *const stanzaData = lstGet(stanzaRepoList, idx);

        // Create the stanzaInfo and section variables
        Variant *const stanzaInfo = varNewKv(kvNew());
        VariantList *const dbSection = varLstNew();
        VariantList *const backupSection = varLstNew();
        VariantList *const archiveSection = varLstNew();
        VariantList *const repoSection = varLstNew();

        int stanzaStatusCode = -1;
        uint64_t stanzaCipherType = cipherTypeNone;

        // Set the stanza name and initialize the overall stanza variables
        kvPut(varKv(stanzaInfo), KEY_NAME_VAR, VARSTR(stanzaData->name));

        // Get the stanza for each requested repo
        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            InfoRepoData *const repoData = &stanzaData->repoList[repoIdx];

            Variant *const repoInfo = varNewKv(kvNew());
            kvPut(varKv(repoInfo), REPO_KEY_KEY_VAR, VARUINT(repoData->key));
            kvPut(varKv(repoInfo), KEY_CIPHER_VAR, VARSTR(strIdToStr(repoData->cipher)));

            // If the stanza on this repo has the default status of ok but the backupInfo was not read, then the stanza exists on
            // other repos but not this one
            if (repoData->stanzaStatus == INFO_STANZA_STATUS_CODE_OK && repoData->backupInfo == NULL)
                repoData->stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH;

            TRY_BEGIN()
            {
                // If the backup.info file has been read, then get the backup and archive information on this repo
                if (repoData->backupInfo != NULL)
                {
                    // If the backup.info file exists, get the database history information (oldest to newest) and corresponding
                    // archive
                    for (unsigned int pgIdx = infoPgDataTotal(infoBackupPg(repoData->backupInfo)) - 1; (int)pgIdx >= 0; pgIdx--)
                    {
                        const InfoPgData pgData = infoPgData(infoBackupPg(repoData->backupInfo), pgIdx);
                        Variant *const pgInfo = varNewKv(kvNew());

                        kvPut(varKv(pgInfo), DB_KEY_ID_VAR, VARUINT(pgData.id));
                        kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
                        kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));
                        kvPut(varKv(pgInfo), KEY_REPO_KEY_VAR, VARUINT(repoData->key));

                        varLstAdd(dbSection, pgInfo);

                        // Get the archive info for the DB from the archive.info file
                        archiveDbList(
                            stanzaData->name, &pgData, archiveSection, repoData->archiveInfo, (pgIdx == 0 ? true : false),
                            repoIdx, repoData->key);
                    }

                    // Set stanza status if the current db sections do not match across repos
                    const InfoPgData backupInfoCurrentPg = infoPgData(
                        infoBackupPg(repoData->backupInfo), infoPgDataCurrentId(infoBackupPg(repoData->backupInfo)));

                    // The current PG system and version must match across repos for the stanza, if not, a failure may have occurred
                    // during an upgrade or the repo may have been disabled during the stanza upgrade to protect from error
                    // propagation
                    if (stanzaData->currentPgVersion != backupInfoCurrentPg.version ||
                        stanzaData->currentPgSystemId != backupInfoCurrentPg.systemId)
                    {
                        stanzaStatusCode = INFO_STANZA_STATUS_CODE_PG_MISMATCH;
                    }
                }
            }
            CATCH_ANY()
            {
                infoStanzaErrorAdd(repoData, errorType(), STR(errorMessage()));
            }
            TRY_END();

            // If there are no current backups on this repo then set status to no backup
            if (repoData->stanzaStatus == INFO_STANZA_STATUS_CODE_OK && infoBackupDataTotal(repoData->backupInfo) == 0)
                repoData->stanzaStatus = INFO_STANZA_STATUS_CODE_NO_BACKUP;

            // Track the status over all repos if the status for the stanza has not already been determined
            if (stanzaStatusCode != INFO_STANZA_STATUS_CODE_PG_MISMATCH)
            {
                if (repoIdx == repoIdxMin)
                    stanzaStatusCode = repoData->stanzaStatus;
                else
                {
                    stanzaStatusCode =
                        stanzaStatusCode != repoData->stanzaStatus ? INFO_STANZA_STATUS_CODE_MIXED : repoData->stanzaStatus;
                }
            }

            // Track cipher type over all repos
            if (repoIdx == repoIdxMin)
                stanzaCipherType = repoData->cipher;
            else
                stanzaCipherType = stanzaCipherType != repoData->cipher ? INFO_STANZA_STATUS_CODE_MIXED : repoData->cipher;

            // Add the status of the stanza on the repo to the repo section, and the repo to the repo array
            repoStanzaStatus(repoData->stanzaStatus, repoInfo, repoData);
            varLstAdd(repoSection, repoInfo);

            // Add the database history, backup, archive and repo arrays to the stanza info
            kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
            kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));
            kvPut(varKv(stanzaInfo), STANZA_KEY_REPO_VAR, varNewVarLst(repoSection));
        }

        // Get a sorted list of the data for all existing backups for this stanza over all repos
        backupList(backupSection, stanzaData, backupLabel, repoIdxMin, repoIdxMax);
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));

        // Set the overall stanza status
        stanzaStatus(stanzaStatusCode, stanzaData, stanzaInfo);

        // Set the overall cipher type
        if (stanzaCipherType != INFO_STANZA_STATUS_CODE_MIXED)
            kvPut(varKv(stanzaInfo), KEY_CIPHER_VAR, VARSTR(strIdToStr(stanzaCipherType)));
        else
            kvPut(varKv(stanzaInfo), KEY_CIPHER_VAR, VARSTRDEF(INFO_STANZA_MIXED));

        varLstAdd(result, stanzaInfo);
    }

    FUNCTION_TEST_RETURN(VARIANT_LIST, result);
}

/***********************************************************************************************************************************
Format the text output for archive and backups for a database group of a stanza
***********************************************************************************************************************************/
// Helper to format date/time with timezone offset
static String *
formatTextBackupDateTime(const time_t epoch)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME, epoch);
    FUNCTION_TEST_END();

    // Construct date/time
    String *const result = strNew();
    struct tm timePart;
    char timeBuffer[20];

    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime_r(&epoch, &timePart));
    strCatZ(result, timeBuffer);

    // Add timezone offset. Since this is not directly available in Posix-compliant APIs, use the difference between gmtime_r() and
    // localtime_r to determine the offset. Handle minute offsets when present.
    struct tm timePartGm;

    gmtime_r(&epoch, &timePartGm);

    timePart.tm_isdst = -1;
    timePartGm.tm_isdst = -1;
    time_t offset = mktime(&timePart) - mktime(&timePartGm);

    if (offset >= 0)
        strCatChr(result, '+');
    else
    {
        offset *= -1;
        strCatChr(result, '-');
    }

    const unsigned int minute = (unsigned int)(offset / 60);
    const unsigned int hour = minute / 60;

    strCatFmt(result, "%02u", hour);

    if (minute % 60 != 0)
        strCatFmt(result, ":%02u", minute - (hour * 60));

    FUNCTION_TEST_RETURN(STRING, result);
}

static void
formatTextBackup(const DbGroup *const dbGroup, String *const resultStr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB_GROUP, dbGroup);
        FUNCTION_TEST_PARAM(STRING, resultStr);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(dbGroup != NULL);

    strCatFmt(resultStr, "\n        wal archive min/max (%s): ", strZ(dbGroup->version));

    // Get the archive min/max if there are any archives for the database
    if (dbGroup->archiveMin != NULL)
        strCatFmt(resultStr, "%s/%s\n", strZ(dbGroup->archiveMin), strZ(dbGroup->archiveMax));
    else
        strCatZ(resultStr, "none present\n");

    for (unsigned int backupIdx = 0; backupIdx < varLstSize(dbGroup->backupList); backupIdx++)
    {
        const KeyValue *const backupInfo = varKv(varLstGet(dbGroup->backupList, backupIdx));

        strCatFmt(
            resultStr, "\n        %s backup: %s\n", strZ(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR))),
            strZ(varStr(kvGet(backupInfo, BACKUP_KEY_LABEL_VAR))));

        // Get and format backup start/stop time
        const KeyValue *const timestampInfo = varKv(kvGet(backupInfo, BACKUP_KEY_TIMESTAMP_VAR));

        strCatFmt(
            resultStr, "            timestamp start/stop: %s / %s\n",
            strZ(formatTextBackupDateTime((time_t)varUInt64(kvGet(timestampInfo, KEY_START_VAR)))),
            strZ(formatTextBackupDateTime((time_t)varUInt64(kvGet(timestampInfo, KEY_STOP_VAR)))));
        strCatZ(resultStr, "            wal start/stop: ");

        // Get and format WAL start/stop
        const KeyValue *const archiveBackupInfo = varKv(kvGet(backupInfo, KEY_ARCHIVE_VAR));

        if (kvGet(archiveBackupInfo, KEY_START_VAR) != NULL && kvGet(archiveBackupInfo, KEY_STOP_VAR) != NULL)
        {
            strCatFmt(
                resultStr, "%s / %s\n", strZ(varStr(kvGet(archiveBackupInfo, KEY_START_VAR))),
                strZ(varStr(kvGet(archiveBackupInfo, KEY_STOP_VAR))));
        }
        else
            strCatZ(resultStr, "n/a\n");

        const KeyValue *const lsnInfo = varKv(kvGet(backupInfo, BACKUP_KEY_LSN_VAR));

        if (lsnInfo != NULL)
        {
            strCatFmt(
                resultStr, "            lsn start/stop: %s / %s\n",
                strZ(varStr(kvGet(lsnInfo, KEY_START_VAR))),
                strZ(varStr(kvGet(lsnInfo, KEY_STOP_VAR))));
        }

        const KeyValue *const info = varKv(kvGet(backupInfo, BACKUP_KEY_INFO_VAR));

        strCatFmt(
            resultStr, "            database size: %s, database backup size: %s\n",
            strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_SIZE_VAR)))),
            strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_DELTA_VAR)))));

        const KeyValue *const repoInfo = varKv(kvGet(info, INFO_KEY_REPOSITORY_VAR));
        const Variant *const repoSizeInfo = kvGet(repoInfo, KEY_SIZE_VAR);

        strCatFmt(resultStr, "            repo%u: ", varUInt(kvGet(varKv(kvGet(backupInfo, KEY_DATABASE_VAR)), KEY_REPO_KEY_VAR)));

        if (repoSizeInfo != NULL)
            strCatFmt(resultStr, "backup set size: %s, ", strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_SIZE_VAR)))));

        strCatFmt(resultStr, "backup size: %s\n", strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_DELTA_VAR)))));

        if (kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR) != NULL)
        {
            const StringList *const referenceList = strLstNewVarLst(varVarLst(kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR)));
            strCatFmt(resultStr, "            backup reference list: %s\n", strZ(strLstJoin(referenceList, ", ")));
        }

        if (kvGet(backupInfo, BACKUP_KEY_DATABASE_REF_VAR) != NULL)
        {
            const VariantList *const dbSection = kvGetList(backupInfo, BACKUP_KEY_DATABASE_REF_VAR);

            strCatZ(resultStr, "            database list:");

            if (varLstEmpty(dbSection))
                strCatZ(resultStr, " none\n");
            else
            {
                for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
                {
                    const KeyValue *const db = varKv(varLstGet(dbSection, dbIdx));

                    strCatFmt(
                        resultStr, " %s (%s)", strZ(varStr(kvGet(db, KEY_NAME_VAR))),
                        strZ(varStrForce(kvGet(db, KEY_OID_VAR))));

                    if (dbIdx != varLstSize(dbSection) - 1)
                        strCatZ(resultStr, ",");
                }

                strCat(resultStr, LF_STR);
            }
        }

        if (kvGet(backupInfo, BACKUP_KEY_LINK_VAR) != NULL)
        {
            const VariantList *const linkSection = kvGetList(backupInfo, BACKUP_KEY_LINK_VAR);

            strCatZ(resultStr, "            symlinks:\n");

            for (unsigned int linkIdx = 0; linkIdx < varLstSize(linkSection); linkIdx++)
            {
                const KeyValue *const link = varKv(varLstGet(linkSection, linkIdx));

                strCatFmt(
                    resultStr, "                %s => %s", strZ(varStr(kvGet(link, KEY_NAME_VAR))),
                    strZ(varStr(kvGet(link, KEY_DESTINATION_VAR))));

                if (linkIdx != varLstSize(linkSection) - 1)
                    strCat(resultStr, LF_STR);
            }

            strCat(resultStr, LF_STR);
        }

        if (kvGet(backupInfo, BACKUP_KEY_TABLESPACE_VAR) != NULL)
        {
            const VariantList *const tablespaceSection = kvGetList(backupInfo, BACKUP_KEY_TABLESPACE_VAR);

            strCatZ(resultStr, "            tablespaces:\n");

            for (unsigned int tblIdx = 0; tblIdx < varLstSize(tablespaceSection); tblIdx++)
            {
                const KeyValue *const tablespace = varKv(varLstGet(tablespaceSection, tblIdx));

                strCatFmt(
                    resultStr, "                %s (%s) => %s", strZ(varStr(kvGet(tablespace, KEY_NAME_VAR))),
                    strZ(varStrForce(kvGet(tablespace, KEY_OID_VAR))),
                    strZ(varStr(kvGet(tablespace, KEY_DESTINATION_VAR))));

                if (tblIdx != varLstSize(tablespaceSection) - 1)
                    strCat(resultStr, LF_STR);
            }

            strCat(resultStr, LF_STR);
        }

        // If errors were detected during the backup
        if (kvGet(backupInfo, BACKUP_KEY_ERROR_VAR) != NULL && varBool(kvGet(backupInfo, BACKUP_KEY_ERROR_VAR)))
        {
            // Output error list if present
            if (kvGet(backupInfo, BACKUP_KEY_ERROR_LIST_VAR) != NULL)
            {
                const StringList *const checksumPageErrorList = strLstNewVarLst(
                    varVarLst(kvGet(backupInfo, BACKUP_KEY_ERROR_LIST_VAR)));

                strCatFmt(resultStr, "            error list: %s\n", strZ(strLstJoin(checksumPageErrorList, ", ")));
            }
            // Else output a general message
            else
                strCatZ(resultStr, "            error(s) detected during backup\n");
        }

        // Annotations metadata
        if (kvGet(backupInfo, BACKUP_KEY_ANNOTATION_VAR) != NULL)
        {
            const KeyValue *const annotationKv = varKv(kvGet(backupInfo, BACKUP_KEY_ANNOTATION_VAR));
            const StringList *const annotationKeyList = strLstNewVarLst(kvKeyList(annotationKv));
            String *const annotationStr = strNew();

            for (unsigned int keyIdx = 0; keyIdx < strLstSize(annotationKeyList); keyIdx++)
            {
                const String *const key = strLstGet(annotationKeyList, keyIdx);
                const String *const value = varStr(kvGet(annotationKv, VARSTR(key)));
                ASSERT(value != NULL);

                strCatFmt(annotationStr, "                %s: %s\n", strZ(key), strZ(value));
            }

            strCatFmt(resultStr, "            annotation(s)\n%s", strZ(annotationStr));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Format the text output for each database of the stanza
***********************************************************************************************************************************/
static void
formatTextDb(
    const KeyValue *const stanzaInfo, String *const resultStr, const String *const currentPgVersion,
    const uint64_t currentPgSystemId, const String *const backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, stanzaInfo);
        FUNCTION_TEST_PARAM(STRING, resultStr);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(STRING, currentPgVersion);
        FUNCTION_TEST_PARAM(UINT64, currentPgSystemId);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(stanzaInfo != NULL);
    ASSERT(currentPgVersion != NULL);

    const VariantList *const dbSection = kvGetList(stanzaInfo, STANZA_KEY_DB_VAR);
    const VariantList *const archiveSection = kvGetList(stanzaInfo, KEY_ARCHIVE_VAR);
    const VariantList *const backupSection = kvGetList(stanzaInfo, STANZA_KEY_BACKUP_VAR);

    List *const dbGroupList = lstNewP(sizeof(DbGroup));

    // For each database update the corresponding archive info
    for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
    {
        const KeyValue *const pgInfo = varKv(varLstGet(dbSection, dbIdx));
        const uint64_t dbSysId = varUInt64(kvGet(pgInfo, DB_KEY_SYSTEM_ID_VAR));
        const String *const dbVersion = varStr(kvGet(pgInfo, DB_KEY_VERSION_VAR));
        const unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
        const unsigned int dbRepoKey = varUInt(kvGet(pgInfo, KEY_REPO_KEY_VAR));

        DbGroup *dbGroup = NULL;

        for (unsigned int dbGrpIdx = 0; dbGrpIdx < lstSize(dbGroupList); dbGrpIdx++)
        {
            DbGroup *const dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);

            if (dbGroupInfo->systemId == dbSysId && strEq(dbGroupInfo->version, dbVersion))
            {
                dbGroup = dbGroupInfo;
                break;
            }
        }

        // If the group was not found, then add it
        if (dbGroup == NULL)
        {
            const DbGroup dbGroupInfo =
            {
                .systemId = dbSysId,
                .version = dbVersion,
                .current = (currentPgSystemId == dbSysId && strEq(currentPgVersion, dbVersion)),
                .archiveMin = NULL,
                .archiveMax = NULL,
                .backupList = varLstNew(),
            };

            lstAdd(dbGroupList, &dbGroupInfo);

            dbGroup = lstGetLast(dbGroupList);
        }

        // For each archive of this stanza, update the archive min/max for this database group if necessary
        for (unsigned int archiveIdx = 0; archiveIdx < varLstSize(archiveSection); archiveIdx++)
        {
            const KeyValue *const archiveInfo = varKv(varLstGet(archiveSection, archiveIdx));
            const KeyValue *const archiveDbInfo = varKv(kvGet(archiveInfo, KEY_DATABASE_VAR));
            const unsigned int archiveDbId = varUInt(kvGet(archiveDbInfo, DB_KEY_ID_VAR));
            const unsigned int archiveRepoKey = varUInt(kvGet(archiveDbInfo, KEY_REPO_KEY_VAR));

            // If there are archives and the min is less than that for this database group, then update the group
            if (archiveDbId == dbId && archiveRepoKey == dbRepoKey && varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR)) != NULL)
            {
                // Although archives should continue to increment over system-id/version with different db-ids, there may be cases
                // where an archived WAL may exist on both, and if the archive id on a later db is less than a prior instance of
                // the same PG, then ensure it is updated as the min. Any need to error should not be handled in the info command.
                if (dbGroup->archiveMin == NULL || strCmp(dbGroup->archiveMin, varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR))) > 0)
                    dbGroup->archiveMin = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR));

                if (dbGroup->archiveMax == NULL || strCmp(dbGroup->archiveMax, varStr(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR))) < 0)
                    dbGroup->archiveMax = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR));
            }
        }
    }

    unsigned int backupDbGrpIdxMin = 0;
    unsigned int backupDbGrpIdxMax = lstSize(dbGroupList);

    // For every backup (oldest to newest) for the stanza, add it to the database group based on system-id and version
    for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
    {
        const KeyValue *const backupInfo = varKv(varLstGet(backupSection, backupIdx));
        const KeyValue *const backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
        const unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));
        const unsigned int backupRepoKey = varUInt(kvGet(backupDbInfo, KEY_REPO_KEY_VAR));

        // Find the database group this backup belongs to and add it
        for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
        {
            const KeyValue *const pgInfo = varKv(varLstGet(dbSection, dbIdx));
            const unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
            const unsigned int dbRepoKey = varUInt(kvGet(pgInfo, KEY_REPO_KEY_VAR));

            if (backupDbId == dbId && backupRepoKey == dbRepoKey)
            {
                for (unsigned int dbGrpIdx = 0; dbGrpIdx < lstSize(dbGroupList); dbGrpIdx++)
                {
                    const DbGroup *const dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);

                    if (dbGroupInfo->systemId == varUInt64(kvGet(pgInfo, DB_KEY_SYSTEM_ID_VAR)) &&
                        strEq(dbGroupInfo->version, varStr(kvGet(pgInfo, DB_KEY_VERSION_VAR))))
                    {
                        varLstAdd(dbGroupInfo->backupList, varLstGet(backupSection, backupIdx));

                        // If we're only looking for one backup, then narrow the db group iterators
                        if (backupLabel != NULL)
                        {
                            backupDbGrpIdxMin = dbGrpIdx;
                            backupDbGrpIdxMax = dbGrpIdx + 1;
                        }

                        dbGrpIdx = lstSize(dbGroupList);
                    }
                }

                dbIdx = varLstSize(dbSection);
            }
        }
    }

    String *const resultCurrent = strCatZ(strNew(), "\n    db (current)");
    bool displayCurrent = false;

    for (unsigned int dbGrpIdx = backupDbGrpIdxMin; dbGrpIdx < backupDbGrpIdxMax; dbGrpIdx++)
    {
        const DbGroup *const dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);

        // Sort the results based on current or prior and only show the prior if it has archives or backups
        if (dbGroupInfo->current)
        {
            formatTextBackup(dbGroupInfo, resultCurrent);
            displayCurrent = true;
        }
        else if (dbGroupInfo->archiveMin != NULL || !varLstEmpty(dbGroupInfo->backupList))
        {
            strCatZ(resultStr, "\n    db (prior)");
            formatTextBackup(dbGroupInfo, resultStr);
        }
    }

    // Add the current results to the end if necessary (e.g. current not displayed if a specified backup label is only in prior)
    if (displayCurrent == true)
        strCat(resultStr, resultCurrent);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the backup and archive info files on the specified repo for the stanza
***********************************************************************************************************************************/
static void
infoUpdateStanza(
    const Storage *const storage, InfoStanzaRepo *const stanzaRepo, const unsigned int repoIdx, const bool stanzaExists,
    const String *const backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storage);
        FUNCTION_TEST_PARAM(INFO_STANZA_REPO, stanzaRepo);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(BOOL, stanzaExists);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(storage != NULL);
    ASSERT(stanzaRepo != NULL);

    volatile int stanzaStatus = INFO_STANZA_STATUS_CODE_OK;

    // If the stanza exists, attempt to get the info files
    if (stanzaExists)
    {
        TRY_BEGIN()
        {
            // Catch certain errors
            TRY_BEGIN()
            {
                // Attempt to load the backup info file
                stanzaRepo->repoList[repoIdx].backupInfo = infoBackupLoadFile(
                    storage, strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanzaRepo->name), INFO_BACKUP_FILE),
                    stanzaRepo->repoList[repoIdx].cipher, stanzaRepo->repoList[repoIdx].cipherPass);
            }
            CATCH(FileMissingError)
            {
                // If there is no backup.info then set the status to indicate missing
                stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA;
            }
            CATCH(CryptoError)
            {
                // If a reason for the error is due to a an encryption error, add a hint
                THROW_FMT(
                    CryptoError,
                    "%s\n"
                    "HINT: use option --stanza if encryption settings are different for the stanza than the global settings.",
                    errorMessage());
            }
            TRY_END();

            // If backup.info was found, then get the archive.info file, which must exist if the backup.info exists, else the failed
            // load will throw an error which will be trapped and recorded
            if (stanzaRepo->repoList[repoIdx].backupInfo != NULL)
            {
                stanzaRepo->repoList[repoIdx].archiveInfo = infoArchiveLoadFile(
                    storage, strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanzaRepo->name), INFO_ARCHIVE_FILE),
                    stanzaRepo->repoList[repoIdx].cipher, stanzaRepo->repoList[repoIdx].cipherPass);

                // If a specific backup exists on this repo then attempt to load the manifest
                if (backupLabel != NULL)
                {
                    stanzaRepo->repoList[repoIdx].manifest = manifestLoadFile(
                        storage, strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel)),
                        stanzaRepo->repoList[repoIdx].cipher,
                        infoPgCipherPass(infoBackupPg(stanzaRepo->repoList[repoIdx].backupInfo)));
                }

                // If there is a valid backup lock for this stanza then backup/expire must be running
                const LockReadResult lockResult = cmdLockRead(lockTypeBackup, stanzaRepo->name, repoIdx);

                if (lockResult.status == lockReadStatusValid)
                {
                    stanzaRepo->backupLockHeld = true;

                    if (lockResult.data.size != NULL)
                    {
                        ASSERT(lockResult.data.size != NULL);

                        stanzaRepo->sizeComplete += varUInt64(lockResult.data.sizeComplete);
                        stanzaRepo->size += varUInt64(lockResult.data.size);
                    }
                }
            }

            stanzaRepo->repoList[repoIdx].stanzaStatus = stanzaStatus;
        }
        CATCH_ANY()
        {
            infoStanzaErrorAdd(&stanzaRepo->repoList[repoIdx], errorType(), STR(errorMessage()));
        }
        TRY_END();
    }
    else
        stanzaRepo->repoList[repoIdx].stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH;

    // If the backup.info and therefore archive.info exist, and the currentPg has not been set for the stanza, then set it
    if (stanzaRepo->currentPgVersion == 0 && stanzaRepo->repoList[repoIdx].backupInfo != NULL)
    {
        InfoPgData backupInfoCurrentPg = infoPgData(
            infoBackupPg(stanzaRepo->repoList[repoIdx].backupInfo),
            infoPgDataCurrentId(infoBackupPg(stanzaRepo->repoList[repoIdx].backupInfo)));

        stanzaRepo->currentPgVersion = backupInfoCurrentPg.version;
        stanzaRepo->currentPgSystemId = backupInfoCurrentPg.systemId;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Render the information for the stanza based on the command parameters
***********************************************************************************************************************************/
static String *
infoRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get stanza if specified
        const String *const stanza = cfgOptionStrNull(cfgOptStanza);

        // Initialize the list of stanzas on all repos
        List *const stanzaRepoList = lstNewP(sizeof(InfoStanzaRepo), .sortOrder = sortOrderAsc, .comparator = lstComparatorStr);

        // Get the backup label if specified
        const String *const backupLabel = cfgOptionStrNull(cfgOptSet);
        bool backupFound = false;

        // Initialize the repo index
        unsigned int repoIdxMin = 0;
        const unsigned int repoTotal = cfgOptionGroupIdxTotal(cfgOptGrpRepo);
        unsigned int repoIdxMax = repoTotal - 1;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxMin = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxMin;
        }

        // Initialize error reporting at the repo level
        InfoRepoData *const repoErrorList = memNew(repoTotal * sizeof(InfoRepoData));
        bool repoError = false;

        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            // Initialize the error list on this repo
            repoErrorList[repoIdx] = (InfoRepoData)
            {
                .key = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                .cipher = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx),
                .cipherPass = cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx),
                .error = NULL,
            };

            // Initialize backup label indicator
            const String *backupExistsOnRepo = NULL;

            // Catch any repo errors
            TRY_BEGIN()
            {
                // Get the repo storage in case it is remote and encryption settings need to be pulled down
                const Storage *const storageRepo = storageRepoIdx(repoIdx);

                // If a backup set was specified, see if the manifest exists
                if (backupLabel != NULL)
                {
                    // If the backup exists on this repo, set the global indicator that we found it on at least one repo and
                    // set the exists label for later loading of the manifest
                    if (storageExistsP(storageRepo, strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel))))
                    {
                        backupFound = true;
                        backupExistsOnRepo = backupLabel;
                    }
                }

                // Get a list of stanzas in the backup directory
                StringList *stanzaNameList = strLstSort(
                    storageListP(storageRepo, STORAGE_PATH_BACKUP_STR), sortOrderAsc);

                // All stanzas will be "found" if they are in the storage list
                bool stanzaExists = true;

                if (stanza != NULL)
                {
                    // If a specific stanza was requested and it is not on this repo, then stanzaExists flag will be reset to false
                    if (strLstEmpty(stanzaNameList) || !strLstExists(stanzaNameList, stanza))
                        stanzaExists = false;

                    // Narrow the list to only the requested stanza
                    strLstFree(stanzaNameList);
                    stanzaNameList = strLstNew();
                    strLstAdd(stanzaNameList, stanza);
                }

                // Process each stanza
                for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaNameList); stanzaIdx++)
                {
                    const String *const stanzaName = strLstGet(stanzaNameList, stanzaIdx);

                    // Get the stanza if it is already in the list
                    InfoStanzaRepo *const stanzaRepo = lstFind(stanzaRepoList, &stanzaName);

                    // If the stanza was already added to the array, then update this repo for the stanza, else the stanza has not
                    // yet been added to the list, so add it
                    if (stanzaRepo != NULL)
                        infoUpdateStanza(storageRepo, stanzaRepo, repoIdx, stanzaExists, backupExistsOnRepo);
                    else
                    {
                        InfoStanzaRepo stanzaRepo =
                        {
                            .name = stanzaName,
                            .currentPgVersion = 0,
                            .currentPgSystemId = 0,
                            .repoList = memNew(repoTotal * sizeof(InfoRepoData)),
                        };

                        // Initialize all the repos
                        for (unsigned int repoListIdx = 0; repoListIdx < repoTotal; repoListIdx++)
                        {
                            stanzaRepo.repoList[repoListIdx] = (InfoRepoData)
                            {
                                .key = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoListIdx),
                                .cipher = cfgOptionIdxStrId(cfgOptRepoCipherType, repoListIdx),
                                .cipherPass = cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoListIdx),
                                .error = NULL,
                            };
                        }

                        // Update the info for this repo
                        infoUpdateStanza(storageRepo, &stanzaRepo, repoIdx, stanzaExists, backupExistsOnRepo);
                        lstAdd(stanzaRepoList, &stanzaRepo);
                    }
                }
            }
            CATCH_ANY()
            {
                // At this point, stanza-level errors were caught and stored in the stanza structure, any errors caught here are due
                // to higher level problems (e.g. repo inaccessible, invalid permissions on the repo, etc) and will be reported
                // later if there are valid stanzas. If there are no valid stanzas after the loop exits, then these errors will be
                // reported with a stanza named "[invalid]".
                infoStanzaErrorAdd(&repoErrorList[repoIdx], errorType(), STR(errorMessage()));
                repoError = true;
            }
            TRY_END();
        }

        VariantList *infoList = varLstNew();
        String *resultStr = strNew();

        // Record any repository-level errors with each stanza -- if there are no stanzas and one was not requested, then create an
        // "[invalid]" one for reporting
        if (repoError)
        {
            if (lstEmpty(stanzaRepoList))
            {
                const InfoStanzaRepo stanzaRepo =
                {
                    .name = stanza != NULL ? stanza : INFO_STANZA_INVALID_STR,
                    .currentPgVersion = 0,
                    .currentPgSystemId = 0,
                    .repoList = repoErrorList,
                };

                lstAdd(stanzaRepoList, &stanzaRepo);
            }
            else
            {
                // For each stanza report the repos that were in error
                for (unsigned int idx = 0; idx < lstSize(stanzaRepoList); idx++)
                {
                    const InfoStanzaRepo *const stanzaData = lstGet(stanzaRepoList, idx);

                    for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
                    {
                        if (repoErrorList[repoIdx].error != NULL)
                            stanzaData->repoList[repoIdx] = repoErrorList[repoIdx];
                    }
                }
            }
        }

        // If a backup label was requested but it was not found on any repo, report the error here rather than individually to avoid
        // listing each repo as "requested backup not found"
        if (backupLabel != NULL && !backupFound)
        {
            // Get the stanza record and update each repo to indicate backup not found where there is not already an error status
            // so that errors on other repositories will be displayed and not overwritten
            const InfoStanzaRepo *const stanzaRepo = lstFind(stanzaRepoList, &stanza);

            for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
            {
                if (stanzaRepo->repoList[repoIdx].stanzaStatus == INFO_STANZA_STATUS_CODE_OK)
                {
                    stanzaRepo->repoList[repoIdx].stanzaStatus = INFO_STANZA_STATUS_CODE_BACKUP_MISSING;
                    infoBackupFree(stanzaRepo->repoList[repoIdx].backupInfo);
                    stanzaRepo->repoList[repoIdx].backupInfo = NULL;
                }
            }
        }

        // If the backup storage exists, then search for and process any stanzas
        if (!lstEmpty(stanzaRepoList))
            infoList = stanzaInfoList(stanzaRepoList, backupLabel, repoIdxMin, repoIdxMax);

        // Format text output
        if (cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_TEXT)
        {
            // Process any stanza directories
            if (!varLstEmpty(infoList))
            {
                for (unsigned int stanzaIdx = 0; stanzaIdx < varLstSize(infoList); stanzaIdx++)
                {
                    const KeyValue *const stanzaInfo = varKv(varLstGet(infoList, stanzaIdx));
                    const String *const stanzaName = varStr(kvGet(stanzaInfo, KEY_NAME_VAR));

                    // Add a carriage return between stanzas
                    if (stanzaIdx > 0)
                        strCatZ(resultStr, "\n");

                    // Stanza name and status
                    strCatFmt(resultStr, "stanza: %s\n    status: ", strZ(stanzaName));

                    // If an error has occurred, provide the information that is available and move onto next stanza
                    const KeyValue *const stanzaStatus = varKv(kvGet(stanzaInfo, STANZA_KEY_STATUS_VAR));
                    const int statusCode = varInt(kvGet(stanzaStatus, STATUS_KEY_CODE_VAR));

                    // Get the lock info
                    const KeyValue *const lockKv = varKv(kvGet(stanzaStatus, STATUS_KEY_LOCK_VAR));
                    const KeyValue *const backupLockKv = varKv(kvGet(lockKv, STATUS_KEY_LOCK_BACKUP_VAR));
                    const bool backupLockHeld = varBool(kvGet(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR));
                    const Variant *const percentComplete = kvGet(backupLockKv, STATUS_KEY_LOCK_BACKUP_PERCENT_COMPLETE_VAR);
                    const String *const percentCompleteStr =
                        percentComplete != NULL ?
                            strNewFmt(" - %u.%02u%% complete", varUInt(percentComplete) / 100, varUInt(percentComplete) % 100) :
                            EMPTY_STR;

                    if (statusCode != INFO_STANZA_STATUS_CODE_OK)
                    {
                        // Update the overall stanza status and change displayed status if backup lock is found
                        if (statusCode == INFO_STANZA_STATUS_CODE_MIXED || statusCode == INFO_STANZA_STATUS_CODE_PG_MISMATCH ||
                            statusCode == INFO_STANZA_STATUS_CODE_OTHER)
                        {
                            // Stanza status
                            strCatFmt(
                                resultStr, "%s%s\n",
                                statusCode == INFO_STANZA_STATUS_CODE_MIXED ?
                                    INFO_STANZA_MIXED :
                                    zNewFmt(
                                        INFO_STANZA_STATUS_ERROR " (%s)",
                                        strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR)))),
                                backupLockHeld == true ?
                                    zNewFmt(" (" INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP "%s)", strZ(percentCompleteStr)) : "");

                            // Output the status per repo
                            const VariantList *const repoSection = kvGetList(stanzaInfo, STANZA_KEY_REPO_VAR);
                            const bool multiRepo = varLstSize(repoSection) > 1;
                            const char *const formatSpacer = multiRepo ? "               " : "            ";

                            for (unsigned int repoIdx = 0; repoIdx < varLstSize(repoSection); repoIdx++)
                            {
                                const KeyValue *const repoInfo = varKv(varLstGet(repoSection, repoIdx));
                                const KeyValue *const repoStatus = varKv(kvGet(repoInfo, STANZA_KEY_STATUS_VAR));

                                // If more than one repo configured, then add the repo status per repo
                                if (multiRepo)
                                    strCatFmt(resultStr, "        repo%u: ", varUInt(kvGet(repoInfo, REPO_KEY_KEY_VAR)));

                                if (varInt(kvGet(repoStatus, STATUS_KEY_CODE_VAR)) == INFO_STANZA_STATUS_CODE_OK)
                                    strCatZ(resultStr, INFO_STANZA_STATUS_OK "\n");
                                else
                                {
                                    if (varInt(kvGet(repoStatus, STATUS_KEY_CODE_VAR)) == INFO_STANZA_STATUS_CODE_OTHER)
                                    {
                                        const StringList *const repoError = strLstNewSplit(
                                            varStr(kvGet(repoStatus, STATUS_KEY_MESSAGE_VAR)), STRDEF("\n"));

                                        strCatFmt(
                                            resultStr, "%s%s%s\n",
                                            multiRepo ? INFO_STANZA_STATUS_ERROR " (" INFO_STANZA_STATUS_MESSAGE_OTHER ")\n" : "",
                                            formatSpacer, strZ(strLstJoin(repoError, zNewFmt("\n%s", formatSpacer))));
                                    }
                                    else
                                    {
                                        strCatFmt(
                                            resultStr, INFO_STANZA_STATUS_ERROR " (%s)\n",
                                            strZ(varStr(kvGet(repoStatus, STATUS_KEY_MESSAGE_VAR))));
                                    }
                                }
                            }
                        }
                        else
                        {
                            strCatFmt(
                                resultStr, "%s (%s%s\n", INFO_STANZA_STATUS_ERROR,
                                strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))),
                                backupLockHeld == true ?
                                    zNewFmt(", " INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP "%s)", strZ(percentCompleteStr)) : ")");
                        }
                    }
                    else
                    {
                        // Change displayed status if backup lock is found
                        if (backupLockHeld)
                        {
                            strCatFmt(
                                resultStr, "%s (%s%s)\n", INFO_STANZA_STATUS_OK, INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP,
                                strZ(percentCompleteStr));
                        }
                        else
                            strCatFmt(resultStr, "%s\n", INFO_STANZA_STATUS_OK);
                    }

                    // Add cipher type if the stanza is found on at least one repo
                    if (statusCode != INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH)
                    {
                        strCatFmt(resultStr, "    cipher: %s\n", strZ(varStr(kvGet(stanzaInfo, KEY_CIPHER_VAR))));

                        // If the cipher is mixed across repos for this stanza then display the per-repo cipher type
                        if (strEq(varStr(kvGet(stanzaInfo, KEY_CIPHER_VAR)), STRDEF(INFO_STANZA_MIXED)))
                        {
                            const VariantList *const repoSection = kvGetList(stanzaInfo, STANZA_KEY_REPO_VAR);

                            for (unsigned int repoIdx = 0; repoIdx < varLstSize(repoSection); repoIdx++)
                            {
                                const KeyValue *const repoInfo = varKv(varLstGet(repoSection, repoIdx));

                                strCatFmt(
                                    resultStr, "        repo%u: %s\n", varUInt(kvGet(repoInfo, REPO_KEY_KEY_VAR)),
                                    strZ(varStr(kvGet(repoInfo, KEY_CIPHER_VAR))));
                            }
                        }
                    }

                    // Get the current database for this stanza
                    if (!varLstEmpty(kvGetList(stanzaInfo, STANZA_KEY_DB_VAR)))
                    {
                        const InfoStanzaRepo *const stanzaRepo = lstFind(stanzaRepoList, &stanzaName);

                        formatTextDb(
                            stanzaInfo, resultStr, pgVersionToStr(stanzaRepo->currentPgVersion), stanzaRepo->currentPgSystemId,
                            backupLabel);
                    }
                }
            }
            else
                resultStr = strNewZ("No stanzas exist in the repository.\n");
        }
        // Format json output
        else
        {
            ASSERT(cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON);
            resultStr = jsonFromVar(varNewVarLst(infoList));
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(resultStr);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdInfo(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioFdWriteOneStr(STDOUT_FILENO, infoRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
