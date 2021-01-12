/***********************************************************************************************************************************
Info Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h> // cshang remove

#include "command/archive/common.h"
#include "command/info/info.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/lock.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "common/crypto/common.h"
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
STRING_STATIC(CFGOPTVAL_INFO_OUTPUT_TEXT_STR,                       "text");

// Naming convention: <sectionname>_KEY_<keyname>_VAR. If the key exists in multiple sections, then <sectionname>_ is omitted.
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MIN_VAR,                          "min");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MAX_VAR,                          "max");
VARIANT_STRDEF_STATIC(BACKREST_KEY_FORMAT_VAR,                      "format");
VARIANT_STRDEF_STATIC(BACKREST_KEY_VERSION_VAR,                     "version");
VARIANT_STRDEF_STATIC(BACKUP_KEY_BACKREST_VAR,                      "backrest");
VARIANT_STRDEF_STATIC(BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR,           "checksum-page-error");
VARIANT_STRDEF_STATIC(BACKUP_KEY_DATABASE_REF_VAR,                  "database-ref");
VARIANT_STRDEF_STATIC(BACKUP_KEY_INFO_VAR,                          "info");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LABEL_VAR,                         "label");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LINK_VAR,                          "link");
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
VARIANT_STRDEF_STATIC(KEY_DESTINATION_VAR,                          "destination");
VARIANT_STRDEF_STATIC(KEY_NAME_VAR,                                 "name");
VARIANT_STRDEF_STATIC(KEY_OID_VAR,                                  "oid");
VARIANT_STRDEF_STATIC(KEY_REPO_KEY_VAR,                             "repo-key");
VARIANT_STRDEF_STATIC(KEY_SIZE_VAR,                                 "size");
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

#define INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP                      "backup/expire running"
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP_STR,           INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP);

/***********************************************************************************************************************************
Data Types and Structures
***********************************************************************************************************************************/
// Repository information for a stanza
typedef struct InfoRepoData
{
    unsigned int key;                                               // User-defined repo key
    CipherType cipher;                                              // Encryption type (0 = none)
    const String *cipherPass;                                       // Passphrase if the repo is encrypted (else NULL)
    int stanzaStatus;                                               // Status code of the the stanza on this repo
    unsigned int backupIdx;                                         // Index of the next backup that may be a candidate for sorting
    InfoBackup *backupInfo;                                         // Contents of the backup.info file of the stanza on this repo
    InfoArchive *archiveInfo;                                       // Contents of the archive.info file of the stanza on this repo
} InfoRepoData;

#define FUNCTION_LOG_INFO_REPO_DATA_TYPE                                                                                           \
    InfoRepoData
#define FUNCTION_LOG_INFO_REPO_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(&value, "InfoRepoData", buffer, bufferSize)

// Stanza with repository list of information for each repository
typedef struct InfoStanzaRepo
{
    const String *name;                                             // Name of the stanza
    uint64_t currentPgSystemId;                                     // Current postgres system id for the stanza
    unsigned int currentPgVersion;                                  // Current postgres version for the stanza
    InfoRepoData *repoList;                                         // List of configured repositories
} InfoStanzaRepo;

#define FUNCTION_LOG_INFO_STANZA_REPO_TYPE                                                                                         \
    InfoStanzaRepo
#define FUNCTION_LOG_INFO_STANZA_REPO_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(&value, "InfoStanzaRepo", buffer, bufferSize)

// Group all databases with the same system-id and version together regardless of db-id or repo
typedef struct DbGroup
{
    uint64_t systemId;
    const String *version;
    bool current;
    String *archiveMin;
    String *archiveMax;
    VariantList *backupList;
} DbGroup;

#define FUNCTION_LOG_DB_GROUP_TYPE                                                                                                 \
    DbGroup
#define FUNCTION_LOG_DB_GROUP_FORMAT(value, buffer, bufferSize)                                                                    \
    objToLog(&value, "DbGroup", buffer, bufferSize)

/***********************************************************************************************************************************
Set the overall error status code and message for the stanza to the code and message passed.
***********************************************************************************************************************************/
static void
stanzaStatus(const int code, bool backupLockHeld, Variant *stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(BOOL, backupLockHeld);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
    FUNCTION_TEST_END();

    ASSERT(code >= 0 && code <= 5);
    ASSERT(stanzaInfo != NULL);

    KeyValue *statusKv = kvPutKv(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR);

    kvAdd(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));

    switch (code)
    {
        case INFO_STANZA_STATUS_CODE_OK:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_OK_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_NO_BACKUP:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_MIXED:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_MESSAGE_MIXED_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_PG_MISMATCH:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_PG_MISMATCH_STR));
            break;
        }
    }

    // Construct a specific lock part
    KeyValue *lockKv = kvPutKv(statusKv, STATUS_KEY_LOCK_VAR);
    KeyValue *backupLockKv = kvPutKv(lockKv, STATUS_KEY_LOCK_BACKUP_VAR);
    kvAdd(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR, VARBOOL(backupLockHeld));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the error status code and message for the stanza on the repo to the code and message passed.
***********************************************************************************************************************************/
static void
repoStanzaStatus(const int code, Variant *repoStanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(VARIANT, repoStanzaInfo);
    FUNCTION_TEST_END();

    ASSERT(code >= 0 && code <= 3);
    ASSERT(repoStanzaInfo != NULL);

    KeyValue *statusKv = kvPutKv(varKv(repoStanzaInfo), STANZA_KEY_STATUS_VAR);

    kvAdd(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));

    switch (code)
    {
        case INFO_STANZA_STATUS_CODE_OK:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_OK_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR));
            break;
        }
        case INFO_STANZA_STATUS_CODE_NO_BACKUP:
        {
            kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR));
            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the data for the archive section of the stanza for the database info from the backup.info file.
***********************************************************************************************************************************/
static void
archiveDbList(
    const String *stanza, const InfoPgData *pgData, VariantList *archiveSection, const InfoArchive *info, bool currentDb,
    unsigned int repoIdx, unsigned int repoKey)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
        FUNCTION_TEST_PARAM(VARIANT_LIST, archiveSection);
        FUNCTION_TEST_PARAM(BOOL, currentDb);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(UINT, repoKey);
    FUNCTION_TEST_END();

    ASSERT(stanza != NULL);
    ASSERT(pgData != NULL);
    ASSERT(archiveSection != NULL);

    // With multiple DB versions, the backup.info history-id may not be the same as archive.info history-id, so the
    // archive path must be built by retrieving the archive id given the db version and system id of the backup.info file.
    // If there is no match, an error will be thrown.
    const String *archiveId = infoArchiveIdHistoryMatch(info, pgData->id, pgData->version, pgData->systemId);

    String *archivePath = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), strZ(archiveId));
    String *archiveStart = NULL;
    String *archiveStop = NULL;
    Variant *archiveInfo = varNewKv(kvNew());
    const Storage *storageRepo = storageRepoIdx(repoIdx);

    // Get a list of WAL directories in the archive repo from oldest to newest, if any exist
    StringList *walDir = strLstSort(
        storageListP(storageRepo, archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);

    if (strLstSize(walDir) > 0)
    {
        // Not every WAL dir has WAL files so check each
        for (unsigned int idx = 0; idx < strLstSize(walDir); idx++)
        {
            // Get a list of all WAL in this WAL dir
            StringList *list = storageListP(
                storageRepo, strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the oldest one as the archive start
            if (strLstSize(list) > 0)
            {
                // Sort the list from oldest to newest to get the oldest starting WAL archived for this DB
                list = strLstSort(list, sortOrderAsc);
                archiveStart = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }

        // Iterate through the directory list in the reverse so processing newest first. Cast comparison to an int for readability.
        for (unsigned int idx = strLstSize(walDir) - 1; (int)idx >= 0; idx--)
        {
            // Get a list of all WAL in this WAL dir
            StringList *list = storageListP(
                storageRepo, strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the newest one as the archive stop
            if (strLstSize(list) > 0)
            {
                // Sort the list from newest to oldest to get the newest ending WAL archived for this DB
                list = strLstSort(list, sortOrderDesc);
                archiveStop = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }
    }

    // If there is an archive or the database is the current database then store it
    if (currentDb || archiveStart != NULL)
    {
        // Add empty database section to archiveInfo and then fill in database id from the backup.info
        KeyValue *databaseInfo = kvPutKv(varKv(archiveInfo), KEY_DATABASE_VAR);

        kvAdd(databaseInfo, DB_KEY_ID_VAR, VARUINT(pgData->id));
        kvAdd(databaseInfo, KEY_REPO_KEY_VAR, VARUINT(repoKey));

        kvPut(varKv(archiveInfo), DB_KEY_ID_VAR, VARSTR(archiveId));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MIN_VAR, (archiveStart != NULL ? VARSTR(archiveStart) : (Variant *)NULL));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MAX_VAR, (archiveStop != NULL ? VARSTR(archiveStop) : (Variant *)NULL));

        varLstAdd(archiveSection, archiveInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Add the backup data to the backup section.
***********************************************************************************************************************************/
static void
backupListAdd(
    VariantList *backupSection, InfoBackupData *backupData, const String *backupLabel, InfoRepoData *repoData, unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);           // The section to add the backup data to
        FUNCTION_TEST_PARAM_P(INFO_BACKUP_DATA, backupData);        // The data for the backup
        FUNCTION_TEST_PARAM(STRING, backupLabel);                   // Backup label to filter if requested by the user
        FUNCTION_TEST_PARAM(INFO_REPO_DATA, repoData);              // The repo data where this backup is located
        FUNCTION_TEST_PARAM(UINT, repoIdx);                         // Internal index for the repo
    FUNCTION_TEST_END();

    ASSERT(backupSection != NULL);
    ASSERT(backupData != NULL);
    ASSERT(repoData != NULL);

    Variant *backupInfo = varNewKv(kvNew());

    // main keys
    kvPut(varKv(backupInfo), BACKUP_KEY_LABEL_VAR, VARSTR(backupData->backupLabel));
    kvPut(varKv(backupInfo), BACKUP_KEY_TYPE_VAR, VARSTR(backupData->backupType));
    kvPut(
        varKv(backupInfo), BACKUP_KEY_PRIOR_VAR,
        (backupData->backupPrior != NULL ? VARSTR(backupData->backupPrior) : NULL));
    kvPut(
        varKv(backupInfo), BACKUP_KEY_REFERENCE_VAR,
        (backupData->backupReference != NULL ? varNewVarLst(varLstNewStrLst(backupData->backupReference)) : NULL));

    // archive section
    KeyValue *archiveInfo = kvPutKv(varKv(backupInfo), KEY_ARCHIVE_VAR);

    kvAdd(
        archiveInfo, KEY_START_VAR,
        (backupData->backupArchiveStart != NULL ? VARSTR(backupData->backupArchiveStart) : NULL));
    kvAdd(
        archiveInfo, KEY_STOP_VAR,
        (backupData->backupArchiveStop != NULL ? VARSTR(backupData->backupArchiveStop) : NULL));

    // backrest section
    KeyValue *backrestInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_BACKREST_VAR);

    kvAdd(backrestInfo, BACKREST_KEY_FORMAT_VAR, VARUINT(backupData->backrestFormat));
    kvAdd(backrestInfo, BACKREST_KEY_VERSION_VAR, VARSTR(backupData->backrestVersion));

    // database section
    KeyValue *dbInfo = kvPutKv(varKv(backupInfo), KEY_DATABASE_VAR);

    kvAdd(dbInfo, DB_KEY_ID_VAR, VARUINT(backupData->backupPgId));
    kvAdd(dbInfo, KEY_REPO_KEY_VAR, VARUINT(repoData->key));

    // info section
    KeyValue *infoInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_INFO_VAR);

    kvAdd(infoInfo, KEY_SIZE_VAR, VARUINT64(backupData->backupInfoSize));
    kvAdd(infoInfo, KEY_DELTA_VAR, VARUINT64(backupData->backupInfoSizeDelta));

    // info:repository section
    KeyValue *repoInfo = kvPutKv(infoInfo, INFO_KEY_REPOSITORY_VAR);

    kvAdd(repoInfo, KEY_SIZE_VAR, VARUINT64(backupData->backupInfoRepoSize));
    kvAdd(repoInfo, KEY_DELTA_VAR, VARUINT64(backupData->backupInfoRepoSizeDelta));

    // timestamp section
    KeyValue *timeInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_TIMESTAMP_VAR);

    // time_t is generally a signed int so cast it to uint64 since it can never be negative (before 1970) in our system
    kvAdd(timeInfo, KEY_START_VAR, VARUINT64((uint64_t)backupData->backupTimestampStart));
    kvAdd(timeInfo, KEY_STOP_VAR, VARUINT64((uint64_t)backupData->backupTimestampStop));

    // If a backup label was specified and this is that label, then get the manifest
    if (backupLabel != NULL && strEq(backupData->backupLabel, backupLabel))
    {
        // Load the manifest file
        Manifest *manifest = manifestLoadFile(
            storageRepoIdx(repoIdx), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel)),
            repoData->cipher, infoPgCipherPass(infoBackupPg(repoData->backupInfo)));

        // Get the list of databases in this backup
        VariantList *databaseSection = varLstNew();

        for (unsigned int dbIdx = 0; dbIdx < manifestDbTotal(manifest); dbIdx++)
        {
            const ManifestDb *db = manifestDb(manifest, dbIdx);

            // Do not display template databases
            if (db->id > db->lastSystemId)
            {
                Variant *database = varNewKv(kvNew());
                kvPut(varKv(database), KEY_NAME_VAR, VARSTR(db->name));
                kvPut(varKv(database), KEY_OID_VAR, VARUINT64(db->id));
                varLstAdd(databaseSection, database);
            }
        }

        // Add the database section even if none found
        kvPut(varKv(backupInfo), BACKUP_KEY_DATABASE_REF_VAR, varNewVarLst(databaseSection));

        // Get symlinks and tablespaces
        VariantList *linkSection = varLstNew();
        VariantList *tablespaceSection = varLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);
            Variant *link = varNewKv(kvNew());
            Variant *tablespace = varNewKv(kvNew());

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
                        varKv(link), KEY_DESTINATION_VAR, varNewStr(strNewFmt("%s/%s", strZ(target->path),
                        strZ(target->file))));
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

        kvPut(varKv(backupInfo), BACKUP_KEY_LINK_VAR, (varLstSize(linkSection) > 0 ? varNewVarLst(linkSection) : NULL));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_TABLESPACE_VAR,
            (varLstSize(tablespaceSection) > 0 ? varNewVarLst(tablespaceSection) : NULL));

        // Get the list of files with an error in the page checksum
        VariantList *checksumPageErrorList = varLstNew();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile *file = manifestFile(manifest, fileIdx);

            if (file->checksumPageError)
                varLstAdd(checksumPageErrorList, varNewStr(manifestPathPg(file->name)));
        }

        kvPut(
            varKv(backupInfo), BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR,
            (varLstSize(checksumPageErrorList) > 0 ? varNewVarLst(checksumPageErrorList) : NULL));

        manifestFree(manifest);
    }

    varLstAdd(backupSection, backupInfo);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
For each current backup in the backup.info file of the stanza, set the data for the backup section.
***********************************************************************************************************************************/
static void
backupList(
    VariantList *backupSection, InfoStanzaRepo *stanzaData, const String *backupLabel, unsigned int repoIdxStart,
    unsigned int repoIdxMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);           // The section to add the backup data to
        FUNCTION_TEST_PARAM_P(INFO_STANZA_REPO, stanzaData);        // The data for the stanza
        FUNCTION_TEST_PARAM(STRING, backupLabel);                   // Backup label to filter if requested by the user
        FUNCTION_TEST_PARAM(UINT, repoIdxStart);                    // The start index of the repo array to begin checking
        FUNCTION_TEST_PARAM(UINT, repoIdxMax);                      // The index beyond the last repo index to check
    FUNCTION_TEST_END();

    ASSERT(backupSection != NULL);
    ASSERT(stanzaData != NULL);

    unsigned int backupNextRepoIdx = 0;
    unsigned int backupTotal = 0;
    unsigned int backupTotalProcessed = 0;

    // Get the number of backups to be processed
    for (unsigned int repoIdx = repoIdxStart; repoIdx < repoIdxMax; repoIdx++)
    {
        InfoRepoData *repoData = &stanzaData->repoList[repoIdx];
        if (repoData->backupInfo != NULL && infoBackupDataTotal(repoData->backupInfo) > 0)
            backupTotal += infoBackupDataTotal(repoData->backupInfo);
    }

    // Process any backups
    while (backupTotalProcessed < backupTotal)
    {
        time_t backupNextTime = 0;

        // Backups are sorted for each repo, so iterate over the lists to create a single list ordered by backup-timestamp-stop
        for (unsigned int repoIdx = repoIdxStart; repoIdx < repoIdxMax; repoIdx++)
        {
            InfoRepoData *repoData = &stanzaData->repoList[repoIdx];

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

        InfoRepoData *repoData = &stanzaData->repoList[backupNextRepoIdx];
        InfoBackupData backupData = infoBackupData(repoData->backupInfo, repoData->backupIdx);
        repoData->backupIdx++;

        // Add the backup data to the backup section
        backupListAdd(backupSection, &backupData, backupLabel, repoData, backupNextRepoIdx);

        backupTotalProcessed++;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the stanza data for each stanza found in the repo.
***********************************************************************************************************************************/
static VariantList *
stanzaInfoList(List *stanzaRepoList, const String *backupLabel, unsigned int repoIdxStart, unsigned int repoIdxMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, stanzaRepoList);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(UINT, repoIdxStart);
        FUNCTION_TEST_PARAM(UINT, repoIdxMax);
    FUNCTION_TEST_END();

    ASSERT(stanzaRepoList != NULL);

    VariantList *result = varLstNew();

    // Sort the list of stanzas
    stanzaRepoList = lstSort(stanzaRepoList, sortOrderAsc);

    // Process each stanza requested
    for (unsigned int idx = 0; idx < lstSize(stanzaRepoList); idx++)
    {
        InfoStanzaRepo *stanzaData = lstGet(stanzaRepoList, idx);

        // Create the stanzaInfo and section variables
        Variant *stanzaInfo = varNewKv(kvNew());
        VariantList *dbSection = varLstNew();
        VariantList *backupSection = varLstNew();
        VariantList *archiveSection = varLstNew();
        VariantList *repoSection = varLstNew();

        int stanzaStatusCode = -1;
        unsigned int stanzaCipherType = 0;
        bool checkBackupLock = false;

        // Set the stanza name and initialize the overall stanza variables
        kvPut(varKv(stanzaInfo), KEY_NAME_VAR, VARSTR(stanzaData->name));

        // Get the stanza for each requested repo
        for (unsigned int repoIdx = repoIdxStart; repoIdx < repoIdxMax; repoIdx++)
        {
            InfoRepoData *repoData = &stanzaData->repoList[repoIdx];

            Variant *repoInfo = varNewKv(kvNew());
            kvPut(varKv(repoInfo), REPO_KEY_KEY_VAR, VARUINT(repoData->key));
            kvPut(varKv(repoInfo), KEY_CIPHER_VAR, VARSTR(cipherTypeName(repoData->cipher)));

            // If the stanza on this repo has the default status of ok but the backupInfo was not read, then the stanza exists on
            // other repos but not this one
            if (repoData->stanzaStatus == INFO_STANZA_STATUS_CODE_OK && repoData->backupInfo == NULL)
                repoData->stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH;

            // If the backup.info file has been read, then get the backup and archive information on this repo
            if (repoData->backupInfo != NULL)
            {
                // If the backup.info file exists, get the database history information (oldest to newest) and corresponding archive
                for (unsigned int pgIdx = infoPgDataTotal(infoBackupPg(repoData->backupInfo)) - 1; (int)pgIdx >= 0; pgIdx--)
                {
                    InfoPgData pgData = infoPgData(infoBackupPg(repoData->backupInfo), pgIdx);
                    Variant *pgInfo = varNewKv(kvNew());

                    kvPut(varKv(pgInfo), DB_KEY_ID_VAR, VARUINT(pgData.id));
                    kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
                    kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));
                    kvPut(varKv(pgInfo), KEY_REPO_KEY_VAR, VARUINT(repoData->key));

                    varLstAdd(dbSection, pgInfo);

                    // Get the archive info for the DB from the archive.info file
                    archiveDbList(
                        stanzaData->name, &pgData, archiveSection, repoData->archiveInfo, (pgIdx == 0 ? true : false), repoIdx,
                        repoData->key);
                }

                // Set stanza status if the current db sections do not match across repos
                InfoPgData backupInfoCurrentPg = infoPgData(
                    infoBackupPg(repoData->backupInfo), infoPgDataCurrentId(infoBackupPg(repoData->backupInfo)));

                // The current PG system and version must match across repos for the stanza, if not, a failure may have occurred
                // during an upgrade or the repo may have been disabled during the stanza upgrade to protect from error propagation
                if (stanzaData->currentPgVersion != backupInfoCurrentPg.version ||
                    stanzaData->currentPgSystemId != backupInfoCurrentPg.systemId)
                {
                    stanzaStatusCode = INFO_STANZA_STATUS_CODE_PG_MISMATCH;
                }
            }

            // If the stanza has been created successfully on at least one repo, then check for a lock on the PG server
            if (repoData->stanzaStatus == INFO_STANZA_STATUS_CODE_OK)
            {
                checkBackupLock = true;

                // If there are no current backups on this repo then set status to no backup
                if (infoBackupDataTotal(repoData->backupInfo) == 0)
                    repoData->stanzaStatus = INFO_STANZA_STATUS_CODE_NO_BACKUP;
            }

            // Track the status over all repos if the status for the stanza has not already been determined
            if (stanzaStatusCode != INFO_STANZA_STATUS_CODE_PG_MISMATCH)
            {
                if (repoIdx == repoIdxStart)
                    stanzaStatusCode = repoData->stanzaStatus;
                else
                {
                    stanzaStatusCode =
                        stanzaStatusCode != repoData->stanzaStatus ? INFO_STANZA_STATUS_CODE_MIXED : repoData->stanzaStatus;
                }
            }

            // Track cipher type over all repos
            if (repoIdx == repoIdxStart)
                stanzaCipherType = repoData->cipher;
            else
                stanzaCipherType = stanzaCipherType != repoData->cipher ? INFO_STANZA_STATUS_CODE_MIXED : repoData->cipher;

            // Add the status of the stanza on the repo to the repo section, and the repo to the repo array
            repoStanzaStatus(repoData->stanzaStatus, repoInfo);
            varLstAdd(repoSection, repoInfo);

            // Add the database history, backup, archive and repo arrays to the stanza info
            kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
            kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));
            kvPut(varKv(stanzaInfo), STANZA_KEY_REPO_VAR, varNewVarLst(repoSection));
        }

        // Get a sorted list of the data for all existing backups for this stanza over all repos
        backupList(backupSection, stanzaData, backupLabel, repoIdxStart, repoIdxMax);
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));

        static bool backupLockHeld = false;

        // If the stanza is OK on at least one repo, then check if there's a local backup running
        if (checkBackupLock)
        {
            // Try to acquire a lock. If not possible, assume another backup or expire is already running.
            backupLockHeld = !lockAcquire(
                cfgOptionStr(cfgOptLockPath), stanzaData->name, cfgOptionStr(cfgOptExecId), lockTypeBackup, 0, false);

            // Immediately release the lock acquired
            lockRelease(!backupLockHeld);
        }

        // Set the overall stanza status
        stanzaStatus(stanzaStatusCode, backupLockHeld, stanzaInfo);

        // Set the overall cipher type
        if (stanzaCipherType != INFO_STANZA_STATUS_CODE_MIXED)
            kvPut(varKv(stanzaInfo), KEY_CIPHER_VAR, VARSTR(cipherTypeName(stanzaCipherType)));
        else
            kvPut(varKv(stanzaInfo), KEY_CIPHER_VAR, VARSTRDEF(INFO_STANZA_MIXED));

        varLstAdd(result, stanzaInfo);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Format the text output for archive and backups for a database group of a stanza.
***********************************************************************************************************************************/
static void
formatTextBackup(const DbGroup *dbGroup, String *resultStr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB_GROUP, dbGroup);
        FUNCTION_TEST_PARAM(STRING, resultStr);
    FUNCTION_TEST_END();

    ASSERT(dbGroup != NULL);

    strCatFmt(resultStr, "\n        wal archive min/max (%s): ", strZ(dbGroup->version));

    // Get the archive min/max if there are any archives for the database
    if (dbGroup->archiveMin != NULL)
        strCatFmt(resultStr, "%s/%s\n", strZ(dbGroup->archiveMin), strZ(dbGroup->archiveMax));
    else
        strCatZ(resultStr, "none present\n");

    for (unsigned int backupIdx = 0; backupIdx < varLstSize(dbGroup->backupList); backupIdx++)
    {
        KeyValue *backupInfo = varKv(varLstGet(dbGroup->backupList, backupIdx));

        strCatFmt(
            resultStr, "\n        %s backup: %s\n", strZ(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR))),
            strZ(varStr(kvGet(backupInfo, BACKUP_KEY_LABEL_VAR))));

        KeyValue *timestampInfo = varKv(kvGet(backupInfo, BACKUP_KEY_TIMESTAMP_VAR));

        // Get and format the backup start/stop time
        char timeBufferStart[20];
        char timeBufferStop[20];
        time_t timeStart = (time_t)varUInt64(kvGet(timestampInfo, KEY_START_VAR));
        time_t timeStop = (time_t)varUInt64(kvGet(timestampInfo, KEY_STOP_VAR));

        strftime(timeBufferStart, sizeof(timeBufferStart), "%Y-%m-%d %H:%M:%S", localtime(&timeStart));
        strftime(timeBufferStop, sizeof(timeBufferStop), "%Y-%m-%d %H:%M:%S", localtime(&timeStop));

        strCatFmt(
            resultStr, "            timestamp start/stop: %s / %s\n", timeBufferStart, timeBufferStop);
        strCatZ(resultStr, "            wal start/stop: ");

        KeyValue *archiveBackupInfo = varKv(kvGet(backupInfo, KEY_ARCHIVE_VAR));

        if (kvGet(archiveBackupInfo, KEY_START_VAR) != NULL &&
            kvGet(archiveBackupInfo, KEY_STOP_VAR) != NULL)
        {
            strCatFmt(
                resultStr, "%s / %s\n", strZ(varStr(kvGet(archiveBackupInfo, KEY_START_VAR))),
                strZ(varStr(kvGet(archiveBackupInfo, KEY_STOP_VAR))));
        }
        else
            strCatZ(resultStr, "n/a\n");

        KeyValue *info = varKv(kvGet(backupInfo, BACKUP_KEY_INFO_VAR));

        strCatFmt(
            resultStr, "            database size: %s, backup size: %s\n",
            strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_SIZE_VAR)))),
            strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_DELTA_VAR)))));

        KeyValue *repoInfo = varKv(kvGet(info, INFO_KEY_REPOSITORY_VAR));

        strCatFmt(
            resultStr, "            repository: %u, repository size: %s, repository backup size: %s\n",
            varUInt(kvGet(varKv(kvGet(backupInfo, KEY_DATABASE_VAR)), KEY_REPO_KEY_VAR)),
            strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_SIZE_VAR)))),
            strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_DELTA_VAR)))));

        if (kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR) != NULL)
        {
            StringList *referenceList = strLstNewVarLst(varVarLst(kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR)));
            strCatFmt(resultStr, "            backup reference list: %s\n", strZ(strLstJoin(referenceList, ", ")));
        }

        if (kvGet(backupInfo, BACKUP_KEY_DATABASE_REF_VAR) != NULL)
        {
            VariantList *dbSection = kvGetList(backupInfo, BACKUP_KEY_DATABASE_REF_VAR);
            strCatZ(resultStr, "            database list:");

            if (varLstSize(dbSection) == 0)
                strCatZ(resultStr, " none\n");
            else
            {
                for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
                {
                    KeyValue *db = varKv(varLstGet(dbSection, dbIdx));
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
            VariantList *linkSection = kvGetList(backupInfo, BACKUP_KEY_LINK_VAR);
            strCatZ(resultStr, "            symlinks:\n");

            for (unsigned int linkIdx = 0; linkIdx < varLstSize(linkSection); linkIdx++)
            {
                KeyValue *link = varKv(varLstGet(linkSection, linkIdx));

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
            VariantList *tablespaceSection = kvGetList(backupInfo, BACKUP_KEY_TABLESPACE_VAR);
            strCatZ(resultStr, "            tablespaces:\n");

            for (unsigned int tblIdx = 0; tblIdx < varLstSize(tablespaceSection); tblIdx++)
            {
                KeyValue *tablespace = varKv(varLstGet(tablespaceSection, tblIdx));

                strCatFmt(
                    resultStr, "                %s (%s) => %s", strZ(varStr(kvGet(tablespace, KEY_NAME_VAR))),
                    strZ(varStrForce(kvGet(tablespace, KEY_OID_VAR))),
                    strZ(varStr(kvGet(tablespace, KEY_DESTINATION_VAR))));

                if (tblIdx != varLstSize(tablespaceSection) - 1)
                    strCat(resultStr, LF_STR);
            }

            strCat(resultStr, LF_STR);
        }

        if (kvGet(backupInfo, BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR) != NULL)
        {
            StringList *checksumPageErrorList = strLstNewVarLst(
                varVarLst(kvGet(backupInfo, BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR)));

            strCatFmt(
                resultStr, "            page checksum error: %s\n",
                strZ(strLstJoin(checksumPageErrorList, ", ")));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}
/***********************************************************************************************************************************
Format the text output for each database of the stanza.
***********************************************************************************************************************************/
static void
formatTextDb(
    const KeyValue *stanzaInfo, String *resultStr, const String *currentPgVersion, uint64_t currentPgSystemId,
    const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, stanzaInfo);
        FUNCTION_TEST_PARAM(STRING, resultStr);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(STRING, currentPgVersion);
        FUNCTION_TEST_PARAM(UINT64, currentPgSystemId);
    FUNCTION_TEST_END();

    ASSERT(stanzaInfo != NULL);
    ASSERT(currentPgVersion != NULL);

    VariantList *dbSection = kvGetList(stanzaInfo, STANZA_KEY_DB_VAR);
    VariantList *archiveSection = kvGetList(stanzaInfo, KEY_ARCHIVE_VAR);
    VariantList *backupSection = kvGetList(stanzaInfo, STANZA_KEY_BACKUP_VAR);

    List *dbGroupList = lstNewP(sizeof(DbGroup));

    // For each database update the corresponding archive info
    for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
    {
        KeyValue *pgInfo = varKv(varLstGet(dbSection, dbIdx));
        uint64_t dbSysId = varUInt64(kvGet(pgInfo, DB_KEY_SYSTEM_ID_VAR));
        const String *dbVersion = varStr(kvGet(pgInfo, DB_KEY_VERSION_VAR));
        unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
        unsigned int dbRepoKey = varUInt(kvGet(pgInfo, KEY_REPO_KEY_VAR));

        DbGroup *dbGroup = NULL;
        for (unsigned int dbGrpIdx = 0; dbGrpIdx < lstSize(dbGroupList); dbGrpIdx++)
        {
            DbGroup *dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);
            if (dbGroupInfo->systemId == dbSysId && strEq(dbGroupInfo->version, dbVersion))
            {
                dbGroup = dbGroupInfo;
                break;
            }
        }

        // If the group was not found, then add it
        if (dbGroup == NULL)
        {
            DbGroup dbGroupInfo =
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
            KeyValue *archiveInfo = varKv(varLstGet(archiveSection, archiveIdx));
            KeyValue *archiveDbInfo = varKv(kvGet(archiveInfo, KEY_DATABASE_VAR));
            unsigned int archiveDbId = varUInt(kvGet(archiveDbInfo, DB_KEY_ID_VAR));
            unsigned int archiveRepoKey = varUInt(kvGet(archiveDbInfo, KEY_REPO_KEY_VAR));

            // If there are archives and the min is less than that for this database group, then update the group
            if (archiveDbId == dbId && archiveRepoKey == dbRepoKey && varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR)) != NULL)
            {
                if (dbGroup->archiveMin == NULL ||
                    strCmp(dbGroup->archiveMin, varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR))) > 0)
                {
                    dbGroup->archiveMin = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR));
                }

                if (dbGroup->archiveMax == NULL ||
                    strCmp(dbGroup->archiveMax, varStr(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR))) < 0)
                {
                    dbGroup->archiveMax = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR));
                }
            }
        }
    }

    unsigned int backupDbGrpIdxMin = 0;
    unsigned int backupDbGrpIdxMax = lstSize(dbGroupList);

    // For every backup (oldest to newest) for the stanza, add it to the database group based on system-id and version
    for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
    {
        KeyValue *backupInfo = varKv(varLstGet(backupSection, backupIdx));

        // If a backup label was specified but this is not it then continue
        if (backupLabel != NULL && !strEq(varStr(kvGet(backupInfo, BACKUP_KEY_LABEL_VAR)), backupLabel))
            continue;

        KeyValue *backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
        unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));
        unsigned int backupRepoKey = varUInt(kvGet(backupDbInfo, KEY_REPO_KEY_VAR));

        // Find the database group this backup belongs to and add it
        for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
        {
            KeyValue *pgInfo = varKv(varLstGet(dbSection, dbIdx));

            unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
            unsigned int dbRepoKey = varUInt(kvGet(pgInfo, KEY_REPO_KEY_VAR));

            if (backupDbId == dbId && backupRepoKey == dbRepoKey)
            {
                for (unsigned int dbGrpIdx = 0; dbGrpIdx < lstSize(dbGroupList); dbGrpIdx++)
                {
                    DbGroup *dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);
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

    String *resultCurrent = strNew("\n    db (current)");
    bool displayCurrent = false;

    for (unsigned int dbGrpIdx = backupDbGrpIdxMin; dbGrpIdx < backupDbGrpIdxMax; dbGrpIdx++)
    {
        DbGroup *dbGroupInfo = lstGet(dbGroupList, dbGrpIdx);

        // Sort the results based on current or prior and only show the prior if it has archives or backups
        if (dbGroupInfo->current)
        {
            formatTextBackup(dbGroupInfo, resultCurrent);
            displayCurrent = true;
        }
        else if (dbGroupInfo->archiveMin != NULL || varLstSize(dbGroupInfo->backupList) > 0)
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
infoUpdateStanza(const Storage *storage, InfoStanzaRepo *stanzaRepo, unsigned int repoIdx, bool stanzaExists)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storage);
        FUNCTION_TEST_PARAM_P(INFO_STANZA_REPO, stanzaRepo);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(BOOL, stanzaExists);
    FUNCTION_TEST_END();

    ASSERT(storage != NULL);
    ASSERT(stanzaRepo != NULL);

    InfoBackup *info = NULL;
    volatile int stanzaStatus = INFO_STANZA_STATUS_CODE_OK;

    // If the stanza exists, attempt to get the backup.info file
    if (stanzaExists)
    {

        // Catch certain errors
        TRY_BEGIN()
        {
            // Attempt to load the backup info file
            info = infoBackupLoadFile(
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

        // If backup.info was found, then get the archive.info file, which must exist if the backup.info exists, else throw error
        if (info != NULL)
        {
            stanzaRepo->repoList[repoIdx].archiveInfo = infoArchiveLoadFile(
                storage, strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanzaRepo->name), INFO_ARCHIVE_FILE),
                stanzaRepo->repoList[repoIdx].cipher, stanzaRepo->repoList[repoIdx].cipherPass);
        }

        stanzaRepo->repoList[repoIdx].stanzaStatus = stanzaStatus;
    }
    else
        stanzaRepo->repoList[repoIdx].stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH;

    stanzaRepo->repoList[repoIdx].backupInfo = info;

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
Render the information for the stanza based on the command parameters.
***********************************************************************************************************************************/
static String *
infoRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get stanza if specified
        const String *stanza = cfgOptionStrNull(cfgOptStanza);

        // Initialize the list of stanzas on all repos
        List *stanzaRepoList = lstNewP(sizeof(InfoStanzaRepo), .sortOrder = sortOrderAsc, .comparator = lstComparatorStr);

        // Get the backup label if specified
        const String *backupLabel = cfgOptionStrNull(cfgOptSet);

        // Since the --set option depends on the --stanza option, the parser will error before this if the backup label is
        // specified but a stanza is not
        if (backupLabel != NULL)
        {
            if (!strEq(cfgOptionStr(cfgOptOutput), CFGOPTVAL_INFO_OUTPUT_TEXT_STR))
                THROW(ConfigError, "option '" CFGOPT_SET "' is currently only valid for text output");

            if (!(cfgOptionTest(cfgOptRepo)))
                THROW(OptionRequiredError, "option '" CFGOPT_REPO "' is required when specifying a backup set");
        }

        // Initialize the repo index
        unsigned int repoIdxStart = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo);
        unsigned int repoTotal = repoIdxMax;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxStart = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxStart + 1;
        }

        for (unsigned int repoIdx = repoIdxStart; repoIdx < repoIdxMax; repoIdx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *storageRepo = storageRepoIdx(repoIdx);

            // If a backup set was specified, see if the manifest exists
            if (backupLabel != NULL)
            {

                if (!storageExistsP(storageRepo, strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel))))
                {
                    THROW_FMT(
                        FileMissingError, "manifest does not exist for backup '%s'\n"
                        "HINT: is the backup listed when running the info command with --stanza option only?", strZ(backupLabel));
                }
            }

            // Get a list of stanzas in the backup directory
            StringList *stanzaNameList = strLstSort(storageListP(storageRepo, STORAGE_PATH_BACKUP_STR), sortOrderAsc);

            // All stanzas will be "found" if they are in the storage list
            bool stanzaExists = true;

            if (stanza != NULL)
            {
                // If a specific stanza was requested and it is not on this repo, then stanzaExists flag will be reset to false
                if (strLstSize(stanzaNameList) == 0 || !strLstExists(stanzaNameList, stanza))
                    stanzaExists = false;

                // Narrow the list to only the requested stanza
                strLstFree(stanzaNameList);
                stanzaNameList = strLstNew();
                strLstAdd(stanzaNameList, stanza);
            }

            // Process each stanza
            for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaNameList); stanzaIdx++)
            {
                String *stanzaName = strLstGet(stanzaNameList, stanzaIdx);

                // Get the stanza if it is already in the list
                InfoStanzaRepo *stanzaRepo = lstFind(stanzaRepoList, &stanzaName);

                // If the stanza was already added to the array, then update this repo for the stanza, else the stanza has not yet
                // been added to the list, so add it
                if (stanzaRepo != NULL)
                    infoUpdateStanza(storageRepo, stanzaRepo, repoIdx, stanzaExists);
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
                        stanzaRepo.repoList[repoListIdx] = (InfoRepoData){0};
                        stanzaRepo.repoList[repoListIdx].key = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoListIdx);
                        stanzaRepo.repoList[repoListIdx].cipher = cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoListIdx));
                        stanzaRepo.repoList[repoListIdx].cipherPass = cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoListIdx);
                    }

                    // Update the info for this repo
                    infoUpdateStanza(storageRepo, &stanzaRepo, repoIdx, stanzaExists);
                    lstAdd(stanzaRepoList, &stanzaRepo);
                }
            }
        }

        VariantList *infoList = varLstNew();
        String *resultStr = strNew("");

        // If the backup storage exists, then search for and process any stanzas
        if (lstSize(stanzaRepoList) > 0)
            infoList = stanzaInfoList(stanzaRepoList, backupLabel, repoIdxStart, repoIdxMax);

        // Format text output
        if (strEq(cfgOptionStr(cfgOptOutput), CFGOPTVAL_INFO_OUTPUT_TEXT_STR))
        {
            // Process any stanza directories
            if  (varLstSize(infoList) > 0)
            {
                for (unsigned int stanzaIdx = 0; stanzaIdx < varLstSize(infoList); stanzaIdx++)
                {
                    KeyValue *stanzaInfo = varKv(varLstGet(infoList, stanzaIdx));
                    const String *stanzaName = varStr(kvGet(stanzaInfo, KEY_NAME_VAR));

                    // Add a carriage return between stanzas
                    if (stanzaIdx > 0)
                        strCatFmt(resultStr, "\n");

                    // Stanza name and status
                    strCatFmt(resultStr, "stanza: %s\n    status: ", strZ(stanzaName));

                    // If an error has occurred, provide the information that is available and move onto next stanza
                    KeyValue *stanzaStatus = varKv(kvGet(stanzaInfo, STANZA_KEY_STATUS_VAR));
                    int statusCode = varInt(kvGet(stanzaStatus, STATUS_KEY_CODE_VAR));

                    // Get the lock info
                    KeyValue *lockKv = varKv(kvGet(stanzaStatus, STATUS_KEY_LOCK_VAR));
                    KeyValue *backupLockKv = varKv(kvGet(lockKv, STATUS_KEY_LOCK_BACKUP_VAR));
                    bool backupLockHeld = varBool(kvGet(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR));

                    if (statusCode != INFO_STANZA_STATUS_CODE_OK)
                    {
                        // Update the overall stanza status and change displayed status if backup lock is found
                        if (statusCode == INFO_STANZA_STATUS_CODE_MIXED || statusCode == INFO_STANZA_STATUS_CODE_PG_MISMATCH)
                        {
                            strCatFmt(
                                resultStr, "%s%s\n",
                                statusCode == INFO_STANZA_STATUS_CODE_MIXED ? INFO_STANZA_MIXED :
                                    strZ(strNewFmt(INFO_STANZA_STATUS_ERROR " (%s)",
                                    strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))))),
                                backupLockHeld == true ? " (" INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP ")" : "");

                            // Output the status per repo
                            VariantList *repoSection = kvGetList(stanzaInfo, STANZA_KEY_REPO_VAR);
                            for (unsigned int repoIdx = 0; repoIdx < varLstSize(repoSection); repoIdx++)
                            {
                                KeyValue *repoInfo = varKv(varLstGet(repoSection, repoIdx));
                                strCatFmt(
                                    resultStr, "        repo%u: ", varUInt(kvGet(repoInfo, REPO_KEY_KEY_VAR)));
                                KeyValue *repoStatus = varKv(kvGet(repoInfo, STANZA_KEY_STATUS_VAR));
                                strCatFmt(
                                    resultStr, "%s",
                                    varInt(kvGet(repoStatus, STATUS_KEY_CODE_VAR)) == INFO_STANZA_STATUS_CODE_OK ?
                                    INFO_STANZA_STATUS_OK "\n" : strZ(strNewFmt(INFO_STANZA_STATUS_ERROR " (%s)\n",
                                    strZ(varStr(kvGet(repoStatus, STATUS_KEY_MESSAGE_VAR))))));
                            }
                        }
                        else
                        {
                            strCatFmt(
                                resultStr, "%s (%s%s\n", INFO_STANZA_STATUS_ERROR,
                                strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))),
                                backupLockHeld == true ? ", " INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP ")" : ")");
                        }
                    }
                    else
                    {
                        // Change displayed status if backup lock is found
                        if (backupLockHeld)
                        {
                            strCatFmt(
                                resultStr, "%s (%s)\n", INFO_STANZA_STATUS_OK, INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP);
                        }
                        else
                            strCatFmt(resultStr, "%s\n", INFO_STANZA_STATUS_OK);
                    }
// CSHANG This is a hold over from the old code where we did not bother to display the cipher if stanza was not found - should we change in master? It will still look lke completely new code anyway so not sure it is worth it...so I propose we just always show the cipher
                    // Add cipher type if the stanza is found on at least one repo
                    if (statusCode != INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH)
                    {
                        strCatFmt(
                            resultStr, "    cipher: %s\n", strZ(varStr(kvGet(stanzaInfo, KEY_CIPHER_VAR))));

                        // If the cipher is mixed across repos for this stanza then display the per-repo cipher type
                        if (strEq(varStr(kvGet(stanzaInfo, KEY_CIPHER_VAR)), STRDEF(INFO_STANZA_MIXED)))
                        {
                            VariantList *repoSection = kvGetList(stanzaInfo, STANZA_KEY_REPO_VAR);
                            for (unsigned int repoIdx = 0; repoIdx < varLstSize(repoSection); repoIdx++)
                            {
                                KeyValue *repoInfo = varKv(varLstGet(repoSection, repoIdx));
                                strCatFmt(
                                    resultStr, "        repo%u: %s\n", varUInt(kvGet(repoInfo, REPO_KEY_KEY_VAR)),
                                    strZ(varStr(kvGet(repoInfo, KEY_CIPHER_VAR))));
                            }
                        }
                    }

                    if (varLstSize(kvGetList(stanzaInfo, STANZA_KEY_DB_VAR)) > 0)
                    {
                        // Get the current database for this stanza
                        InfoStanzaRepo *stanzaRepo = lstFind(stanzaRepoList, &stanzaName);

                        formatTextDb(
                            stanzaInfo, resultStr, pgVersionToStr(stanzaRepo->currentPgVersion), stanzaRepo->currentPgSystemId,
                            backupLabel);
                    }
                }
            }
            else
                resultStr = strNew("No stanzas exist in the repository.\n");
        }
        // Format json output
        else
            resultStr = jsonFromVar(varNewVarLst(infoList));

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
void
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
