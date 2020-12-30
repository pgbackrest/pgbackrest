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
VARIANT_STRDEF_STATIC(KEY_DATABASE_VAR,                             "database");
VARIANT_STRDEF_STATIC(KEY_DELTA_VAR,                                "delta");
VARIANT_STRDEF_STATIC(KEY_DESTINATION_VAR,                          "destination");
VARIANT_STRDEF_STATIC(KEY_NAME_VAR,                                 "name");
VARIANT_STRDEF_STATIC(KEY_OID_VAR,                                  "oid");
VARIANT_STRDEF_STATIC(KEY_SIZE_VAR,                                 "size");
VARIANT_STRDEF_STATIC(KEY_START_VAR,                                "start");
VARIANT_STRDEF_STATIC(KEY_STOP_VAR,                                 "stop");
VARIANT_STRDEF_STATIC(STANZA_KEY_BACKUP_VAR,                        "backup");
VARIANT_STRDEF_STATIC(STANZA_KEY_CIPHER_VAR,                        "cipher");
VARIANT_STRDEF_STATIC(STANZA_KEY_STATUS_VAR,                        "status");
VARIANT_STRDEF_STATIC(STANZA_KEY_DB_VAR,                            "db");
VARIANT_STRDEF_STATIC(STATUS_KEY_CODE_VAR,                          "code");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_VAR,                          "lock");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_VAR,                   "backup");
VARIANT_STRDEF_STATIC(STATUS_KEY_LOCK_BACKUP_HELD_VAR,              "held");
VARIANT_STRDEF_STATIC(STATUS_KEY_MESSAGE_VAR,                       "message");

#define INFO_STANZA_STATUS_OK                                       "ok"
#define INFO_STANZA_STATUS_ERROR                                    "error"

#define INFO_STANZA_STATUS_CODE_OK                                  0
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_OK_STR,                    "ok");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH                 1
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR,   "missing stanza path");
#define INFO_STANZA_STATUS_CODE_NO_BACKUP                           2
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR,             "no valid backups");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA                 3
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR,   "missing stanza data");

STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP_STR,           "backup/expire running");

typedef struct InfoRepoData
{
    unsigned int key;                                               // User-entered repo key, 0 when stanza is not found on the repo
    CipherType cipher;
    const String *cipherPass;
    bool stanzaExists;
    int stanzaStatus;
    InfoBackup *backupInfo;
    InfoArchive *archiveInfo;
} InfoRepoData;

#define FUNCTION_LOG_INFO_STANZA_REPO_TYPE                                                                                         \
    InfoStanzaRepo
#define FUNCTION_LOG_INFO_STANZA_REPO_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(&value, "InfoStanzaRepo", buffer, bufferSize)

typedef struct InfoStanzaRepo
{
    const String *name;                                             // Name of the stanza
    InfoRepoData *repoList;                                         // List of configured repositories
} InfoStanzaRepo;

/***********************************************************************************************************************************
Set error status code and message for the stanza to the code and message passed.
***********************************************************************************************************************************/
static void
stanzaStatus(const int code, bool backupLockHeld, Variant *stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(BOOL, backupLockHeld);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
    FUNCTION_TEST_END();

    ASSERT(code >= 0 && code <= 3);
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
    }

    // Construct a specific lock part
    KeyValue *lockKv = kvPutKv(statusKv, STATUS_KEY_LOCK_VAR);
    KeyValue *backupLockKv = kvPutKv(lockKv, STATUS_KEY_LOCK_BACKUP_VAR);
    kvAdd(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR, VARBOOL(backupLockHeld));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the data for the archive section of the stanza for the database info from the backup.info file.
***********************************************************************************************************************************/
static void
archiveDbList(const String *stanza, const InfoPgData *pgData, VariantList *archiveSection, const InfoArchive *info, bool currentDb)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
        FUNCTION_TEST_PARAM(VARIANT_LIST, archiveSection);
        FUNCTION_TEST_PARAM(BOOL, currentDb);
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

    // Get a list of WAL directories in the archive repo from oldest to newest, if any exist
    StringList *walDir = strLstSort(
        storageListP(storageRepo(), archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);

    if (strLstSize(walDir) > 0)
    {
        // Not every WAL dir has WAL files so check each
        for (unsigned int idx = 0; idx < strLstSize(walDir); idx++)
        {
            // Get a list of all WAL in this WAL dir
            StringList *list = storageListP(
                storageRepo(), strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
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
                storageRepo(), strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, idx))),
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

        kvPut(varKv(archiveInfo), DB_KEY_ID_VAR, VARSTR(archiveId));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MIN_VAR, (archiveStart != NULL ? VARSTR(archiveStart) : (Variant *)NULL));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MAX_VAR, (archiveStop != NULL ? VARSTR(archiveStop) : (Variant *)NULL));

        varLstAdd(archiveSection, archiveInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
For each current backup in the backup.info file of the stanza, set the data for the backup section.
***********************************************************************************************************************************/
static void
backupList(VariantList *backupSection, InfoBackup *info, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);
        FUNCTION_TEST_PARAM(INFO_BACKUP, info);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(backupSection != NULL);
    ASSERT(info != NULL);

    // For each current backup, get the label and corresponding data and build the backup section
    for (unsigned int keyIdx = 0; keyIdx < infoBackupDataTotal(info); keyIdx++)
    {
        // Get the backup data
        InfoBackupData backupData = infoBackupData(info, keyIdx);

        Variant *backupInfo = varNewKv(kvNew());

        // main keys
        kvPut(varKv(backupInfo), BACKUP_KEY_LABEL_VAR, VARSTR(backupData.backupLabel));
        kvPut(varKv(backupInfo), BACKUP_KEY_TYPE_VAR, VARSTR(backupData.backupType));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_PRIOR_VAR,
            (backupData.backupPrior != NULL ? VARSTR(backupData.backupPrior) : NULL));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_REFERENCE_VAR,
            (backupData.backupReference != NULL ? varNewVarLst(varLstNewStrLst(backupData.backupReference)) : NULL));

        // archive section
        KeyValue *archiveInfo = kvPutKv(varKv(backupInfo), KEY_ARCHIVE_VAR);

        kvAdd(
            archiveInfo, KEY_START_VAR,
            (backupData.backupArchiveStart != NULL ? VARSTR(backupData.backupArchiveStart) : NULL));
        kvAdd(
            archiveInfo, KEY_STOP_VAR,
            (backupData.backupArchiveStop != NULL ? VARSTR(backupData.backupArchiveStop) : NULL));

        // backrest section
        KeyValue *backrestInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_BACKREST_VAR);

        kvAdd(backrestInfo, BACKREST_KEY_FORMAT_VAR, VARUINT(backupData.backrestFormat));
        kvAdd(backrestInfo, BACKREST_KEY_VERSION_VAR, VARSTR(backupData.backrestVersion));

        // database section
        KeyValue *dbInfo = kvPutKv(varKv(backupInfo), KEY_DATABASE_VAR);

        kvAdd(dbInfo, DB_KEY_ID_VAR, VARUINT(backupData.backupPgId));

        // info section
        KeyValue *infoInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_INFO_VAR);

        kvAdd(infoInfo, KEY_SIZE_VAR, VARUINT64(backupData.backupInfoSize));
        kvAdd(infoInfo, KEY_DELTA_VAR, VARUINT64(backupData.backupInfoSizeDelta));

        // info:repository section
        KeyValue *repoInfo = kvPutKv(infoInfo, INFO_KEY_REPOSITORY_VAR);

        kvAdd(repoInfo, KEY_SIZE_VAR, VARUINT64(backupData.backupInfoRepoSize));
        kvAdd(repoInfo, KEY_DELTA_VAR, VARUINT64(backupData.backupInfoRepoSizeDelta));

        // timestamp section
        KeyValue *timeInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_TIMESTAMP_VAR);

        // time_t is generally a signed int so cast it to uint64 since it can never be negative (before 1970) in our system
        kvAdd(timeInfo, KEY_START_VAR, VARUINT64((uint64_t)backupData.backupTimestampStart));
        kvAdd(timeInfo, KEY_STOP_VAR, VARUINT64((uint64_t)backupData.backupTimestampStop));

        // If a backup label was specified and this is that label, then get the manifest
        if (backupLabel != NULL && strEq(backupData.backupLabel, backupLabel))
        {
            // Load the manifest file
            const Manifest *manifest = manifestLoadFile(
                storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel)),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(info)));

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
        }

        varLstAdd(backupSection, backupInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the stanza data for each stanza found in the repo.
***********************************************************************************************************************************/
static VariantList *
stanzaInfoList(
    const String *stanza, List *stanzaRepoList, const String *backupLabel, unsigned int repoIdx, unsigned int repoIdxMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(LIST, stanzaRepoList);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(UINT, repoIdxMax);
    FUNCTION_TEST_END();

    ASSERT(stanzaRepoList != NULL);

    VariantList *result = varLstNew();

    // Sort the list of stanzas
    stanzaRepoList = lstSort(stanzaRepoList, sortOrderAsc);

    // For each stanza
    for (unsigned int idx = 0; idx < lstSize(stanzaRepoList); idx++)
    {
        InfoStanzaRepo *stanzaData = lstGet(stanzaRepoList, idx);
// printf("stanzaList Idx: %u, stanzaName: %s, repoIdx: %u, repoIdxMax %u\n", idx, strZ(stanzaData->name), repoIdx, repoIdxMax);fflush(stdout); // cshang remove

        // Create the stanzaInfo and section variables
        Variant *stanzaInfo = varNewKv(kvNew());
        VariantList *dbSection = varLstNew();
        VariantList *backupSection = varLstNew();
        VariantList *archiveSection = varLstNew();

        // Set the stanza name
        kvPut(varKv(stanzaInfo), KEY_NAME_VAR, VARSTR(stanzaData->name));

        for (; repoIdx < repoIdxMax; repoIdx++)
        {
// printf("SIZE REPOLIST: %s,stanzaList Idx: %u, stanzaName: %s, repoIdx: %u, repoIdxMax %u\n", stanzaData->repoList == NULL ? "NULL" : "SOMETHING",  idx, strZ(stanzaData->name), repoIdx, repoIdxMax);fflush(stdout); // cshang remove

            InfoRepoData *repoData = &stanzaData->repoList[repoIdx];

            if (repoData->stanzaStatus != INFO_STANZA_STATUS_CODE_OK)
            {
// CSHANG Combine these into a function:
                // Add the database history, backup and archive sections to the stanza info
                kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
                kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));
                kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));

                stanzaStatus(repoData->stanzaStatus, false, stanzaInfo);
// CSHANG This also has to change to be the repo based cipher not the global, so this is temporary
                if (repoData->stanzaStatus != INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH)
                {
                    if (repoData->cipherPass == NULL)
                        kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_NONE_STR));
                    else
                        kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_AES_256_CBC_STR));
                }

                varLstAdd(result, stanzaInfo);

                // No further processing of this stanza on this repo so continue to next repo
                continue;
            }
            // InfoRepoData *repoData = lstGet(stanzaData->repoList, repoIdx); // cshang Does not work because repoList is not of List * type, instead it is InfoRepoData *
// CSHANG This comment is wrong - we are getting the list as oldest to newest (MODIFIED 12/22/20)

// CSHANG The backupinfo must exist at this point since status checked and repo skipped if it didn't
            // Determine if encryption is enabled by checking for a cipher passphrase.  This is not ideal since it does not tell us
            // what type of encryption is in use, but to figure that out we need a way to query the (possibly) remote repo to find
            // out.  No such mechanism exists so this will have to do for now.  Probably the easiest thing to do is store the
            // cipher type in the info file.
// CSHANG NEED TO CHANGE THE cipher handling
            if (infoPgCipherPass(infoBackupPg(repoData->backupInfo)) != NULL)
                kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_AES_256_CBC_STR));
            else
// CSHANG Why would we not go through the config parsing system? Also, we need to set the cipher for each repo - this is the global
// repo status so need to set this at the end
                // Since we may not be going through the config parsing system, default the cipher to NONE.
                kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_NONE_STR));

            // If the backup.info file exists, get the database history information (oldest to newest) and corresponding archive
            for (unsigned int pgIdx = infoPgDataTotal(infoBackupPg(repoData->backupInfo)) - 1; (int)pgIdx >= 0; pgIdx--)
            {
                InfoPgData pgData = infoPgData(infoBackupPg(repoData->backupInfo), pgIdx);
                Variant *pgInfo = varNewKv(kvNew());

                kvPut(varKv(pgInfo), DB_KEY_ID_VAR, VARUINT(pgData.id));
                kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
                kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

                varLstAdd(dbSection, pgInfo);

                // Get the archive info for the DB from the archive.info file
                archiveDbList(stanzaData->name, &pgData, archiveSection, repoData->archiveInfo, (pgIdx == 0 ? true : false));
            }

            // Get data for all existing backups for this stanza
            backupList(backupSection, repoData->backupInfo, backupLabel);

            // Add the database history, backup and archive sections to the stanza info
            kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
            kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));
            kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));

            // If a status has not already been set, check if there's a local backup running
            static bool backupLockHeld = false;

            if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL)
            {
                // Try to acquire a lock. If not possible, assume another backup or expire is already running.
                backupLockHeld = !lockAcquire(
                    cfgOptionStr(cfgOptLockPath), stanzaData->name, cfgOptionStr(cfgOptExecId), lockTypeBackup, 0, false);

                // Immediately release the lock acquired
                lockRelease(!backupLockHeld);
            }

            // If a status has not already been set and there are no backups then set status to no backup
            if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL &&
                varLstSize(kvGetList(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR)) == 0)
            {
                stanzaStatus(INFO_STANZA_STATUS_CODE_NO_BACKUP, backupLockHeld, stanzaInfo);
            }

            // If a status has still not been set then set it to OK
            if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL)
                stanzaStatus(INFO_STANZA_STATUS_CODE_OK, backupLockHeld, stanzaInfo);

            varLstAdd(result, stanzaInfo);
        }

    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Format the text output for each database of the stanza.
***********************************************************************************************************************************/
static void
formatTextDb(const KeyValue *stanzaInfo, String *resultStr, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, stanzaInfo);
        FUNCTION_TEST_PARAM(STRING, resultStr);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(stanzaInfo != NULL);

    VariantList *dbSection = kvGetList(stanzaInfo, STANZA_KEY_DB_VAR);
    VariantList *archiveSection = kvGetList(stanzaInfo, KEY_ARCHIVE_VAR);
    VariantList *backupSection = kvGetList(stanzaInfo, STANZA_KEY_BACKUP_VAR);

    // For each database (working from oldest to newest) find the corresponding archive and backup info
    for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
    {
        KeyValue *pgInfo = varKv(varLstGet(dbSection, dbIdx));
        unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
        bool backupInDb = false;

        // If a backup label was specified then see if it exists for this database
        if (backupLabel != NULL)
        {
            for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
            {
                KeyValue *backupInfo = varKv(varLstGet(backupSection, backupIdx));
                KeyValue *backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
                unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));

                // If the backup requested is in this database then break from the loop
                if (backupDbId == dbId)
                {
                    backupInDb = true;
                    break;
                }
            }
        }

        // If backup label was requested but was not found in this database then continue to next database
        if (backupLabel != NULL && !backupInDb)
            continue;

        // List is ordered so 0 is always the current DB index
        if (dbIdx == varLstSize(dbSection) - 1)
            strCatZ(resultStr, "\n    db (current)");

        // Get the min/max archive information for the database
        String *archiveResult = strNew("");

        for (unsigned int archiveIdx = 0; archiveIdx < varLstSize(archiveSection); archiveIdx++)
        {
            KeyValue *archiveInfo = varKv(varLstGet(archiveSection, archiveIdx));
            KeyValue *archiveDbInfo = varKv(kvGet(archiveInfo, KEY_DATABASE_VAR));
            unsigned int archiveDbId = varUInt(kvGet(archiveDbInfo, DB_KEY_ID_VAR));

            if (archiveDbId == dbId)
            {
                strCatFmt(
                    archiveResult, "\n        wal archive min/max (%s): ",
                    strZ(varStr(kvGet(archiveInfo, DB_KEY_ID_VAR))));

                // Get the archive min/max if there are any archives for the database
                if (kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR) != NULL)
                {
                    strCatFmt(
                        archiveResult, "%s/%s\n", strZ(varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR))),
                        strZ(varStr(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR))));
                }
                else
                    strCatZ(archiveResult, "none present\n");
            }
        }

        // Get the information for each current backup
        String *backupResult = strNew("");

        for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
        {
            KeyValue *backupInfo = varKv(varLstGet(backupSection, backupIdx));
            KeyValue *backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
            unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));

            // If a backup label was specified but this is not it then continue
            if (backupLabel != NULL && !strEq(varStr(kvGet(backupInfo, BACKUP_KEY_LABEL_VAR)), backupLabel))
                continue;

            if (backupDbId == dbId)
            {
                strCatFmt(
                    backupResult, "\n        %s backup: %s\n", strZ(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR))),
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
                    backupResult, "            timestamp start/stop: %s / %s\n", timeBufferStart, timeBufferStop);
                strCatZ(backupResult, "            wal start/stop: ");

                KeyValue *archiveBackupInfo = varKv(kvGet(backupInfo, KEY_ARCHIVE_VAR));

                if (kvGet(archiveBackupInfo, KEY_START_VAR) != NULL &&
                    kvGet(archiveBackupInfo, KEY_STOP_VAR) != NULL)
                {
                    strCatFmt(
                        backupResult, "%s / %s\n", strZ(varStr(kvGet(archiveBackupInfo, KEY_START_VAR))),
                        strZ(varStr(kvGet(archiveBackupInfo, KEY_STOP_VAR))));
                }
                else
                    strCatZ(backupResult, "n/a\n");

                KeyValue *info = varKv(kvGet(backupInfo, BACKUP_KEY_INFO_VAR));

                strCatFmt(
                    backupResult, "            database size: %s, backup size: %s\n",
                    strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_SIZE_VAR)))),
                    strZ(strSizeFormat(varUInt64Force(kvGet(info, KEY_DELTA_VAR)))));

                KeyValue *repoInfo = varKv(kvGet(info, INFO_KEY_REPOSITORY_VAR));

                strCatFmt(
                    backupResult, "            repository size: %s, repository backup size: %s\n",
                    strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_SIZE_VAR)))),
                    strZ(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_DELTA_VAR)))));

                if (kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR) != NULL)
                {
                    StringList *referenceList = strLstNewVarLst(varVarLst(kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR)));
                    strCatFmt(backupResult, "            backup reference list: %s\n", strZ(strLstJoin(referenceList, ", ")));
                }

                if (kvGet(backupInfo, BACKUP_KEY_DATABASE_REF_VAR) != NULL)
                {
                    VariantList *dbSection = kvGetList(backupInfo, BACKUP_KEY_DATABASE_REF_VAR);
                    strCatZ(backupResult, "            database list:");

                    if (varLstSize(dbSection) == 0)
                        strCatZ(backupResult, " none\n");
                    else
                    {
                        for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
                        {
                            KeyValue *db = varKv(varLstGet(dbSection, dbIdx));
                            strCatFmt(
                                backupResult, " %s (%s)", strZ(varStr(kvGet(db, KEY_NAME_VAR))),
                                strZ(varStrForce(kvGet(db, KEY_OID_VAR))));

                            if (dbIdx != varLstSize(dbSection) - 1)
                                strCatZ(backupResult, ",");
                        }

                        strCat(backupResult, LF_STR);
                    }
                }

                if (kvGet(backupInfo, BACKUP_KEY_LINK_VAR) != NULL)
                {
                    VariantList *linkSection = kvGetList(backupInfo, BACKUP_KEY_LINK_VAR);
                    strCatZ(backupResult, "            symlinks:\n");

                    for (unsigned int linkIdx = 0; linkIdx < varLstSize(linkSection); linkIdx++)
                    {
                        KeyValue *link = varKv(varLstGet(linkSection, linkIdx));

                        strCatFmt(
                            backupResult, "                %s => %s", strZ(varStr(kvGet(link, KEY_NAME_VAR))),
                            strZ(varStr(kvGet(link, KEY_DESTINATION_VAR))));

                        if (linkIdx != varLstSize(linkSection) - 1)
                            strCat(backupResult, LF_STR);
                    }

                    strCat(backupResult, LF_STR);
                }

                if (kvGet(backupInfo, BACKUP_KEY_TABLESPACE_VAR) != NULL)
                {
                    VariantList *tablespaceSection = kvGetList(backupInfo, BACKUP_KEY_TABLESPACE_VAR);
                    strCatZ(backupResult, "            tablespaces:\n");

                    for (unsigned int tblIdx = 0; tblIdx < varLstSize(tablespaceSection); tblIdx++)
                    {
                        KeyValue *tablespace = varKv(varLstGet(tablespaceSection, tblIdx));

                        strCatFmt(
                            backupResult, "                %s (%s) => %s", strZ(varStr(kvGet(tablespace, KEY_NAME_VAR))),
                            strZ(varStrForce(kvGet(tablespace, KEY_OID_VAR))),
                            strZ(varStr(kvGet(tablespace, KEY_DESTINATION_VAR))));

                        if (tblIdx != varLstSize(tablespaceSection) - 1)
                            strCat(backupResult, LF_STR);
                    }

                    strCat(backupResult, LF_STR);
                }

                if (kvGet(backupInfo, BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR) != NULL)
                {
                    StringList *checksumPageErrorList = strLstNewVarLst(
                        varVarLst(kvGet(backupInfo, BACKUP_KEY_CHECKSUM_PAGE_ERROR_VAR)));

                    strCatFmt(
                        backupResult, "            page checksum error: %s\n",
                        strZ(strLstJoin(checksumPageErrorList, ", ")));
                }
            }
        }

        // If there is data to display, then display it.
        if (strSize(archiveResult) > 0 || strSize(backupResult) > 0)
        {
            if (dbIdx != varLstSize(dbSection) - 1)
                strCatZ(resultStr, "\n    db (prior)");

            if (strSize(archiveResult) > 0)
                strCat(resultStr, archiveResult);

            if (strSize(backupResult) > 0)
                strCat(resultStr, backupResult);
        }
    }
    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the backup and archive info files on the specified repo for the stanza
***********************************************************************************************************************************/
static void
infoUpdateStanza(const Storage *storage, InfoStanzaRepo *stanzaRepo, String *stanzaName, unsigned int repoIdx, bool stanzaExists)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storage);
        FUNCTION_TEST_PARAM_P(INFO_STANZA_REPO, stanzaRepo);
        FUNCTION_TEST_PARAM(STRING, stanzaName);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(BOOL, stanzaExists);
    FUNCTION_TEST_END();

    ASSERT(storage != NULL);
    ASSERT(stanzaRepo != NULL);
    ASSERT(stanzaName != NULL);

    InfoBackup *info = NULL;

    // If the stanza exists, attempt to get the backup.info file
    if (stanzaExists)
    {
        volatile int stanzaStatus = INFO_STANZA_STATUS_CODE_OK;

        // Catch certain errors
        TRY_BEGIN()
        {
            // Attempt to load the backup info file
            info = infoBackupLoadFile(
                storage, strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanzaName), INFO_BACKUP_FILE),
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
                storage, strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanzaName), INFO_ARCHIVE_FILE),
                stanzaRepo->repoList[repoIdx].cipher, stanzaRepo->repoList[repoIdx].cipherPass);
        }

        stanzaRepo->repoList[repoIdx].stanzaStatus = stanzaStatus;
    }
    else
        stanzaRepo->repoList[repoIdx].stanzaStatus = INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH;

    stanzaRepo->repoList[repoIdx].stanzaExists = stanzaExists; // CSHANG Don't think need
    stanzaRepo->repoList[repoIdx].backupInfo = info;

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

        // initialize the list of stanzas on all repos
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
        unsigned int repoIdx = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo);
        unsigned int repoTotal = repoIdxMax;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdx = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdx + 1;
        }

// printf("repoTotal %u, repoIdx %u, repoIdxMax %u\n", repoTotal, repoIdx, repoIdxMax);fflush(stdout); // cshang remove
        for (unsigned int idx = repoIdx; idx < repoIdxMax; idx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *storageRepo = storageRepoIdx(idx);

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
            StringList *stanzaNameList = storageListP(storageRepo, STORAGE_PATH_BACKUP_STR);
// printf("stanzaList size on disk: %u\n", strLstSize(stanzaNameList));fflush(stdout); // cshang remove


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
// printf("stanzaList To Process: %u\n", strLstSize(stanzaNameList));fflush(stdout); // cshang remove
            // Process each stanza
            for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaNameList); stanzaIdx++)
            {
                String *stanzaName = strLstGet(stanzaNameList, stanzaIdx);

                // Get the stanza if it is already in the list
                InfoStanzaRepo *stanzaRepo = lstFind(stanzaRepoList, &stanzaName);

                // If the stanza was already added to the array, then update this repo for the stanza
                if (stanzaRepo != NULL)
                {
// CSHANG check if a stanza was specified (stanza != NULL) and if so, was it found on the repo? if not, then do nothing? stanzaMissing will remain 0 = false
                    infoUpdateStanza(storageRepo, stanzaRepo, stanzaName, idx, stanzaExists);

                    // CSHANG Not needed since this will only have the one stanza // If a stanza was requested then this must be it so exit the stanza loop
                    // if (stanza != NULL)
                    //     break;
                }
                // Else the stanza has not yet been added to the list, so add it
                else
                {
                    InfoStanzaRepo stanzaRepo =
                    {
                        .name = stanzaName,
                        .repoList = memNew(repoTotal * sizeof(InfoRepoData)),
                    };

                    // Initialize all the repos
                    for (unsigned int repoListIdx = 0; repoListIdx < repoTotal; repoListIdx++)
                    {
                        stanzaRepo.repoList[repoListIdx].key = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoListIdx);
                        stanzaRepo.repoList[repoListIdx].cipher = cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoListIdx));
                        stanzaRepo.repoList[repoListIdx].cipherPass = cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx);
                    }

                    // Update the info for this repo
                    infoUpdateStanza(storageRepo, &stanzaRepo, stanzaName, idx, stanzaExists);
                    lstAdd(stanzaRepoList, &stanzaRepo);
                }
            }
        }
// CSHANG Will we need to have a new status indicator if a stanza exists on one repo and not on another? Not one that has been requested, but one that we found on repo1 but not on repo2? We must not error, because maybe they did that on purpose, but we should be consistent and indicate, maybe INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH - but we won't know untill we build the full list of stanzas on every repo.
        VariantList *infoList = varLstNew();
        String *resultStr = strNew("");
// printf("stanzaRepoList size: %u\n", lstSize(stanzaRepoList));fflush(stdout); // cshang remove
/* CSHANG Discuss wth David:

1) Should go back and fix the original info command in https://github.com/pgbackrest/pgbackrest/blob/master/src/command/info/info.c#L510 so that we add an empty archive[] when the stanza is missing. The "minimum" set of data currently has  [{"backup":[],"db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}}] but should probably incude archive[], i.e.  [{"archive":[],"backup":[],"cipher":"none","db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}}]? Else I need to exlude it when the path is not found.

*/
        // If the backup storage exists, then search for and process any stanzas
        if (lstSize(stanzaRepoList) > 0)
            infoList = stanzaInfoList(stanza, stanzaRepoList, backupLabel, repoIdx, repoIdxMax);

        // Format text output
        if (strEq(cfgOptionStr(cfgOptOutput), CFGOPTVAL_INFO_OUTPUT_TEXT_STR))
        {
            // Process any stanza directories
            if  (varLstSize(infoList) > 0)
            {
                for (unsigned int stanzaIdx = 0; stanzaIdx < varLstSize(infoList); stanzaIdx++)
                {
                    KeyValue *stanzaInfo = varKv(varLstGet(infoList, stanzaIdx));

                    // Add a carriage return between stanzas
                    if (stanzaIdx > 0)
                        strCatFmt(resultStr, "\n");

                    // Stanza name and status
                    strCatFmt(
                        resultStr, "stanza: %s\n    status: ", strZ(varStr(kvGet(stanzaInfo, KEY_NAME_VAR))));

                    // If an error has occurred, provide the information that is available and move onto next stanza
                    KeyValue *stanzaStatus = varKv(kvGet(stanzaInfo, STANZA_KEY_STATUS_VAR));
                    int statusCode = varInt(kvGet(stanzaStatus, STATUS_KEY_CODE_VAR));

                    // Get the lock info
                    KeyValue *lockKv = varKv(kvGet(stanzaStatus, STATUS_KEY_LOCK_VAR));
                    KeyValue *backupLockKv = varKv(kvGet(lockKv, STATUS_KEY_LOCK_BACKUP_VAR));

                    if (statusCode != INFO_STANZA_STATUS_CODE_OK)
                    {
                        // Change displayed status if backup lock is found
                        if (varBool(kvGet(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR)))
                        {
                            strCatFmt(
                                resultStr, "%s (%s, %s)\n", INFO_STANZA_STATUS_ERROR,
                                strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))),
                                strZ(INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP_STR));
                        }
                        else
                        {
                            strCatFmt(
                                resultStr, "%s (%s)\n", INFO_STANZA_STATUS_ERROR,
                                strZ(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))));
                        }

                        if (statusCode == INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA ||
                            statusCode == INFO_STANZA_STATUS_CODE_NO_BACKUP)
                        {
                            strCatFmt(
                                resultStr, "    cipher: %s\n", strZ(varStr(kvGet(stanzaInfo, STANZA_KEY_CIPHER_VAR))));

                            // If there is a backup.info file but no backups, then process the archive info
                            if (statusCode == INFO_STANZA_STATUS_CODE_NO_BACKUP)
                                formatTextDb(stanzaInfo, resultStr, NULL);
                        }

                        continue;
                    }
                    else
                    {
                        // Change displayed status if backup lock is found
                        if (varBool(kvGet(backupLockKv, STATUS_KEY_LOCK_BACKUP_HELD_VAR)))
                        {
                            strCatFmt(
                                resultStr, "%s (%s)\n", INFO_STANZA_STATUS_OK, strZ(INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP_STR));
                        }
                        else
                            strCatFmt(resultStr, "%s\n", INFO_STANZA_STATUS_OK);
                    }

                    // Cipher
                    strCatFmt(
                        resultStr, "    cipher: %s\n", strZ(varStr(kvGet(stanzaInfo, STANZA_KEY_CIPHER_VAR))));

                    formatTextDb(stanzaInfo, resultStr, backupLabel);
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
