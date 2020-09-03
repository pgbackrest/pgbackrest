/***********************************************************************************************************************************
Info Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "command/archive/common.h"
#include "command/info/info.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/fdWrite.h"
#include "common/lock.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "config/config.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/common.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_STATIC(CFGOPTVAL_INFO_OUTPUT_TEXT_STR,                       "text");

// Naming convention: <sectionname>_KEY_<keyname>_VAR. If the key exists in multiple sections, then <sectionname>_ is omitted.
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MIN_VAR,                          "min");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MAX_VAR,                          "max");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MISSING_WAL_LIST_VAR,             "missing-list");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MISSING_WAL_NB_VAR,               "missing-nb");
VARIANT_STRDEF_STATIC(BACKREST_KEY_FORMAT_VAR,                      "format");
VARIANT_STRDEF_STATIC(BACKREST_KEY_VERSION_VAR,                     "version");
VARIANT_STRDEF_STATIC(BACKUP_KEY_BACKREST_VAR,                      "backrest");
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
#define INFO_STANZA_STATUS_CODE_MISSING_WAL_SEG                     4
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_WAL_SEG_STR,       "missing wal segment(s)");

STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_LOCK_BACKUP_STR,           "backup/expire running");

/***********************************************************************************************************************************
Set error status code and message for the stanza to the code and message passed.
***********************************************************************************************************************************/
static void
stanzaStatus(const int code, const String *message, bool backupLockHeld, Variant *stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(STRING, message);
        FUNCTION_TEST_PARAM(BOOL, backupLockHeld);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
    FUNCTION_TEST_END();

    ASSERT(code >= 0 && code <= 4);
    ASSERT(message != NULL);
    ASSERT(stanzaInfo != NULL);

    KeyValue *statusKv = kvPutKv(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR);

    kvAdd(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));
    kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(message));

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
        }

        varLstAdd(backupSection, backupInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Generate the wal archives chain in the given interval and parse .history file if needed.
***********************************************************************************************************************************/
static void
generateNeededArchivesList(
    StringList *neededArchivesList, const unsigned int pgVersion, const String *stanza, const String *archiveId,
    const String *archiveStart, const String *archiveStop)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, neededArchivesList);
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(STRING, archiveStart);
        FUNCTION_TEST_PARAM(STRING, archiveStop);
    FUNCTION_TEST_END();

    ASSERT(neededArchivesList != NULL);
    ASSERT(archiveStart != NULL);
    ASSERT(archiveStop != NULL);

    // Until we find a way to get the WAL segment size from e.g. the archive.info, let's use the PG default size
    unsigned int PG_WAL_SEGMENT_SIZE_DEFAULT = 16 * 1024 * 1024;

    // Get .history file if timelines don't match
    uint32_t startTimeline = (uint32_t)strtol(strZ(strSubN(archiveStart, 0, 8)), NULL, 16);
    uint32_t stopTimeline = (uint32_t)strtol(strZ(strSubN(archiveStop, 0, 8)), NULL, 16);

    // Store history content: WAL segments and target timeline
    StringList *timelineSwitchWal = strLstNew();
    KeyValue *targetTimelines = kvNew();

    if(stopTimeline > startTimeline)
    {
        String *historyFile = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s/%08X.history", strZ(stanza), strZ(archiveId), stopTimeline);
        IoRead *source = storageReadIo(storageNewReadP(storageRepo(), historyFile));

        CipherType repoCipherType = cipherType(cfgOptionStr(cfgOptRepoCipherType));
        if (repoCipherType != cipherTypeNone)
        {
            // Get cipher pass from archive.info file
            InfoArchive *info = infoArchiveLoadFile(
                storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), INFO_ARCHIVE_FILE),
                repoCipherType, cfgOptionStr(cfgOptRepoCipherPass));

            cipherBlockFilterGroupAdd(ioReadFilterGroup(source), repoCipherType, cipherModeDecrypt, infoArchiveCipherPass(info));
        }

        Buffer *buffer = bufNew(ioBufferSize());
        if(ioReadOpen(source))
        {
            ioRead(source, buffer);
            ioReadClose(source);
        }

        // Extract history file content line by line
        const StringList *historyFileContentList = strLstNewSplit(strNewBuf(buffer), LF_STR);
        String *previousBranchWal = NULL;

        for (unsigned int contentIdx = 0; contentIdx < strLstSize(historyFileContentList); contentIdx++)
        {
            if (pgVersion < PG_VERSION_93)
            {
                // History files format changed after PG 9.3
                const String *historyLine = strLstGet(historyFileContentList, contentIdx);
                RegExp *historyExp = regExpNew(STRDEF("^[ ]*[0-9A-F]{1,8}[\t][0-9A-F]{24}[\t].*$"));

                if (regExpMatch(historyExp, historyLine))
                {
                    // Parse the line to extract timeline and WAL segment
                    StringList *tabSplitList = strLstNewSplitZ(strTrim(regExpMatchStr(historyExp)), "\t");
                    uint32_t currentTimeline = (uint32_t)strtol(strZ(strLstGet(tabSplitList,0)), NULL, 16);
                    String *branchWal = strLstGet(tabSplitList,1);

                    // Store the WAL segment where timeline switch occurred and the target timeline
                    strLstAddIfMissing(timelineSwitchWal, branchWal);
                    kvPut(targetTimelines, VARSTR(branchWal), VARUINT(currentTimeline));

                }
            }
            else
            {
                const String *historyLine = strLstGet(historyFileContentList, contentIdx);
                RegExp *historyExp = regExpNew(STRDEF("^[ ]*[0-9A-F]{1,8}[\t][0-9A-F]{1,8}/[0-9A-F]{1,8}[\t].*$"));

                if (regExpMatch(historyExp, historyLine))
                {
                    // Parse the line to extract timeline, WAL and segment numbers
                    StringList *tabSplitList = strLstNewSplitZ(strTrim(regExpMatchStr(historyExp)), "\t");
                    StringList *slashSplitList = strLstNewSplit(strLstGet(tabSplitList,1), FSLASH_STR);
                    uint32_t currentTimeline = (uint32_t)strtol(strZ(strLstGet(tabSplitList,0)), NULL, 16);

                    String *branchWal = strNewFmt("%08X%08X%08X", currentTimeline,
                        (uint32_t)strtol(strZ(strLstGet(slashSplitList,0)), NULL, 16),
                        (uint32_t)strtol(strZ(strLstGet(slashSplitList,1)), NULL, 16)>>24);

                    // Store the WAL segment where timeline switch occurred
                    strLstAddIfMissing(timelineSwitchWal, branchWal);

                    // Now that we know what was the target timeline, store it for the previous WAL segment
                    if (previousBranchWal != NULL)
                        kvPut(targetTimelines, VARSTR(previousBranchWal), VARUINT(currentTimeline));

                    previousBranchWal = branchWal;
                }
            }
        }

        // What if there should be a timeline switch but we don't find any target in the history file?
        // Throw an error to avoid infinite loop later
        if (strLstSize(timelineSwitchWal) == 0)
            THROW(AssertError, "no target WAL have been found in the history file(s)");
    }

    // Generate the archives list we need for the stopTimeline
    String *nextWalSegment = strDup(archiveStart);
    strLstAddIfMissing(neededArchivesList, archiveStart);
    strLstAddIfMissing(neededArchivesList, archiveStop);

    while(!strEq(nextWalSegment, archiveStop))
    {
        nextWalSegment = walSegmentNext(nextWalSegment, PG_WAL_SEGMENT_SIZE_DEFAULT, pgVersion);

        if(strLstExists(timelineSwitchWal, nextWalSegment))
        {
            // Timeline switch found at nextWalSegment. Target timeline has to be defined.
            // If we couldn't define it earlier from the history file, let's assume the target is max wal archive timeline.
            uint32_t nextTimeline = stopTimeline;

            if(kvKeyExists(targetTimelines, VARSTR(nextWalSegment)))
                nextTimeline = varUInt(kvGet(targetTimelines, VARSTR(nextWalSegment)));

            nextWalSegment = strNewFmt("%08X%s", nextTimeline, strZ(strSub(nextWalSegment, 8)));
        }

        strLstAddIfMissing(neededArchivesList, nextWalSegment);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Check if all the archives needed to perform a restore on the current timeline are in the repo.
***********************************************************************************************************************************/
static void
walArchivesGapDetection(
    const String *stanza, const VariantList *dbSection, VariantList *archiveSection, const VariantList *backupSection,
    StringList *missingArchiveList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(VARIANT_LIST, dbSection);
        FUNCTION_TEST_PARAM(VARIANT_LIST, archiveSection);
        FUNCTION_TEST_PARAM(VARIANT_LIST, backupSection);
        FUNCTION_TEST_PARAM(STRING_LIST, missingArchiveList);
    FUNCTION_TEST_END();

    ASSERT(stanza != NULL);
    ASSERT(dbSection != NULL);
    ASSERT(archiveSection != NULL);
    ASSERT(backupSection != NULL);

    for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
    {
        KeyValue *pgInfo = varKv(varLstGet(dbSection, dbIdx));
        unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));
        unsigned int pgVersion = pgVersionFromStr(varStr(kvGet(pgInfo, DB_KEY_VERSION_VAR)));

        String *archiveStart = NULL;
        String *archiveStop = NULL;
        String *archiveId = NULL;
        unsigned int archiveSectionIdx = 0;
        String *firstStartWal = NULL;
        StringList *backupStartWalsList = strLstNew();
        StringList *neededArchivesList = strLstNew();

        // Get the min/max archive information for the database
        for (unsigned int archiveIdx = 0; archiveIdx < varLstSize(archiveSection); archiveIdx++)
        {
            KeyValue *archiveInfo = varKv(varLstGet(archiveSection, archiveIdx));
            KeyValue *archiveDbInfo = varKv(kvGet(archiveInfo, KEY_DATABASE_VAR));
            unsigned int archiveDbId = varUInt(kvGet(archiveDbInfo, DB_KEY_ID_VAR));

            if (archiveDbId == dbId && kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR) != NULL)
            {
                archiveStart = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR));
                archiveStop = varStrForce(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR));
                archiveId = varStrForce(kvGet(archiveInfo, DB_KEY_ID_VAR));
                archiveSectionIdx = archiveIdx;
            }
        }

        // Don't run gapDetection if there's no archive found in the repo for that specific db-id
        if (archiveStart != NULL)
        {
            // Get the start/stop archive information for each backup
            for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
            {
                KeyValue *backupInfo = varKv(varLstGet(backupSection, backupIdx));
                KeyValue *backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
                unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));

                if (backupDbId == dbId)
                {
                    KeyValue *archiveBackupInfo = varKv(kvGet(backupInfo, KEY_ARCHIVE_VAR));

                    if (kvGet(archiveBackupInfo, KEY_START_VAR) != NULL &&
                        kvGet(archiveBackupInfo, KEY_STOP_VAR) != NULL)
                    {
                        // Generate the list of required WAL archives for each backup
                        generateNeededArchivesList(
                            neededArchivesList, pgVersion, stanza, archiveId,
                            varStr(kvGet(archiveBackupInfo, KEY_START_VAR)),
                            varStr(kvGet(archiveBackupInfo, KEY_STOP_VAR)));

                        // Store the first backup start WAL location
                        if (firstStartWal == NULL)
                            firstStartWal = varStrForce(kvGet(archiveBackupInfo, KEY_START_VAR));

                        if(strEq(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR)), BACKUP_TYPE_FULL_STR))
                        {
                            strLstAdd(backupStartWalsList, varStr(kvGet(archiveBackupInfo, KEY_START_VAR)));
                        }
                        else if(strEq(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR)), BACKUP_TYPE_DIFF_STR) && (
                                strEq(cfgOptionStr(cfgOptRepoRetentionArchiveType), BACKUP_TYPE_DIFF_STR) ||
                                strEq(cfgOptionStr(cfgOptRepoRetentionArchiveType), BACKUP_TYPE_INCR_STR)))
                        {
                            strLstAdd(backupStartWalsList, varStr(kvGet(archiveBackupInfo, KEY_START_VAR)));
                        }
                        else if(strEq(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR)), BACKUP_TYPE_INCR_STR) &&
                                strEq(cfgOptionStr(cfgOptRepoRetentionArchiveType), BACKUP_TYPE_INCR_STR))
                        {
                            strLstAdd(backupStartWalsList, varStr(kvGet(archiveBackupInfo, KEY_START_VAR)));
                        }
                    }
                }
            }

            // Check that the oldest archive in the repo is not older than the first backup start WAL location.
            // It could indicate an expire problem.
            if (strCmp(archiveStart,firstStartWal) < 0)
                LOG_WARN_FMT(
                    "min wal archive ('%s') found in the repo is older than the first backup start WAL location ('%s').\n"
                    "HINT: this might indicate a configuration error in the retention policy.",
                    strZ(archiveStart), strZ(firstStartWal));

            // If repo-retention-archive is set, exclude aggressively expired archives.
            // Works the same way with repo-retention-full-type = count or time.
            if (cfgOptionTest(cfgOptRepoRetentionArchive) && strLstSize(backupStartWalsList) >= cfgOptionUInt(cfgOptRepoRetentionArchive))
            {
                archiveStart = strLstGet(backupStartWalsList,
                    strLstSize(backupStartWalsList) - cfgOptionUInt(cfgOptRepoRetentionArchive));
            }

            // Generate the complete list of required WAL archives
            generateNeededArchivesList(neededArchivesList, pgVersion, stanza, archiveId, archiveStart, archiveStop);
            strLstSort(neededArchivesList, sortOrderAsc);

            // Get the list of all WAL archives in WAL directories in the archive repo
            StringList *repoArchivesList = strLstNew();
            String *archivePath = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanza), strZ(archiveId));

            StringList *walDir = strLstSort(
                storageListP(storageRepo(), archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);

            for (unsigned int walDirIdx = 0; walDirIdx < strLstSize(walDir); walDirIdx++)
            {
                StringList *list = storageListP(
                    storageRepo(), strNewFmt("%s/%s", strZ(archivePath), strZ(strLstGet(walDir, walDirIdx))),
                    .expression = WAL_SEGMENT_FILE_REGEXP_STR);

                for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(list); archiveIdx++)
                    strLstAdd(repoArchivesList, strSubN(strLstGet(list, archiveIdx), 0, 24));
            }
            strLstSort(repoArchivesList, sortOrderAsc);

            // Find missing archives
            for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(neededArchivesList); archiveIdx++)
            {
                // Look for each needed WAL archive if it exists in the repo
                if(!strLstExists(repoArchivesList, strLstGet(neededArchivesList, archiveIdx)))
                    strLstAddIfMissing(missingArchiveList, strLstGet(neededArchivesList, archiveIdx));
            }

            // Add missing archives information to archive section
            Variant *archiveInfo = varLstGet(archiveSection, archiveSectionIdx);
            kvPut(varKv(archiveInfo), ARCHIVE_KEY_MISSING_WAL_LIST_VAR, VARSTR(strLstJoin(missingArchiveList, ",")));
            kvPut(varKv(archiveInfo), ARCHIVE_KEY_MISSING_WAL_NB_VAR, VARUINT(strLstSize(missingArchiveList)));
            varLstRemoveIdx(archiveSection, archiveSectionIdx);
            varLstAdd(archiveSection, archiveInfo);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the stanza data for each stanza found in the repo.
***********************************************************************************************************************************/
static VariantList *
stanzaInfoList(const String *stanza, StringList *stanzaList, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(STRING_LIST, stanzaList);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(stanzaList != NULL);

    VariantList *result = varLstNew();
    bool stanzaFound = false;

    // Sort the list
    stanzaList = strLstSort(stanzaList, sortOrderAsc);

    for (unsigned int idx = 0; idx < strLstSize(stanzaList); idx++)
    {
        String *stanzaListName = strLstGet(stanzaList, idx);

        // If a specific stanza has been requested and this is not it, then continue to the next in the list else indicate found
        if (stanza != NULL)
        {
            if (!strEq(stanza, stanzaListName))
                continue;
            else
                stanzaFound = true;
        }

        // Create the stanzaInfo and section variables
        Variant *stanzaInfo = varNewKv(kvNew());
        VariantList *dbSection = varLstNew();
        VariantList *backupSection = varLstNew();
        VariantList *archiveSection = varLstNew();
        InfoBackup *info = NULL;

        // Catch certain errors
        TRY_BEGIN()
        {
            // Attempt to load the backup info file
            info = infoBackupLoadFile(
                storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanzaListName), INFO_BACKUP_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStrNull(cfgOptRepoCipherPass));
        }
        CATCH(FileMissingError)
        {
            // If there is no backup.info then set the status to indicate missing
            stanzaStatus(
                INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR, false, stanzaInfo);
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

        // Set the stanza name and cipher. Since we may not be going through the config parsing system, default the cipher to NONE.
        kvPut(varKv(stanzaInfo), KEY_NAME_VAR, VARSTR(stanzaListName));
        kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_NONE_STR));

        // If the backup.info file exists, get the database history information (newest to oldest) and corresponding archive
        if (info != NULL)
        {
            // Determine if encryption is enabled by checking for a cipher passphrase.  This is not ideal since it does not tell us
            // what type of encryption is in use, but to figure that out we need a way to query the (possibly) remote repo to find
            // out.  No such mechanism exists so this will have to do for now.  Probably the easiest thing to do is store the
            // cipher type in the info file.
            if (infoPgCipherPass(infoBackupPg(info)) != NULL)
                kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_AES_256_CBC_STR));

            for (unsigned int pgIdx = infoPgDataTotal(infoBackupPg(info)) - 1; (int)pgIdx >= 0; pgIdx--)
            {
                InfoPgData pgData = infoPgData(infoBackupPg(info), pgIdx);
                Variant *pgInfo = varNewKv(kvNew());

                kvPut(varKv(pgInfo), DB_KEY_ID_VAR, VARUINT(pgData.id));
                kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
                kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

                varLstAdd(dbSection, pgInfo);

                // Get the archive info for the DB from the archive.info file
                InfoArchive *info = infoArchiveLoadFile(
                    storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(stanzaListName), INFO_ARCHIVE_FILE),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStrNull(cfgOptRepoCipherPass));
                archiveDbList(stanzaListName, &pgData, archiveSection, info, (pgIdx == 0 ? true : false));
            }

            // Get data for all existing backups for this stanza
            backupList(backupSection, info, backupLabel);
        }

        // WAL archives gap detection
        StringList *missingArchiveList = strLstNew();
        if (cfgOptionValid(cfgOptArchiveGapDetection) && cfgOptionBool(cfgOptArchiveGapDetection))
            walArchivesGapDetection(stanzaListName, dbSection, archiveSection, backupSection, missingArchiveList);

        // Add the database history, backup and archive sections to the stanza info
        kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));
        kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));
        // If a status has not already been set, check if there's a local backup running
        static bool backupLockHeld = false;

        if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL)
        {
            // Try to acquire a lock. If not possible, assume another backup or expire is already running.
            backupLockHeld = !lockAcquire(cfgOptionStr(cfgOptLockPath), stanzaListName, lockTypeBackup, 0, false);

            // Immediately release the lock acquired
            lockRelease(!backupLockHeld);
        }

        // Set status in case of missing archives
        if (strLstSize(missingArchiveList) > 0)
        {
            stanzaStatus(
                INFO_STANZA_STATUS_CODE_MISSING_WAL_SEG, INFO_STANZA_STATUS_MESSAGE_MISSING_WAL_SEG_STR, backupLockHeld,
                stanzaInfo);
        }

        // If a status has not already been set and there are no backups then set status to no backup
        if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL &&
            varLstSize(kvGetList(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR)) == 0)
        {
            stanzaStatus(INFO_STANZA_STATUS_CODE_NO_BACKUP, INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR, backupLockHeld, stanzaInfo);
        }

        // If a status has still not been set then set it to OK
        if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL)
            stanzaStatus(INFO_STANZA_STATUS_CODE_OK, INFO_STANZA_STATUS_MESSAGE_OK_STR, backupLockHeld, stanzaInfo);

        varLstAdd(result, stanzaInfo);
    }

    // If looking for a specific stanza and it was not found, set minimum info and the status
    if (stanza != NULL && !stanzaFound)
    {
        Variant *stanzaInfo = varNewKv(kvNew());

        kvPut(varKv(stanzaInfo), KEY_NAME_VAR, VARSTR(stanza));

        kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(varLstNew()));
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(varLstNew()));

        stanzaStatus(
            INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR, false, stanzaInfo);
        varLstAdd(result, stanzaInfo);
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

                // Show missing archives if any
                if (kvKeyExists(archiveInfo, ARCHIVE_KEY_MISSING_WAL_NB_VAR) &&
                    varUInt(kvGet(archiveInfo, ARCHIVE_KEY_MISSING_WAL_NB_VAR)) > 0)
                {
                    strCatFmt(
                        archiveResult, "        missing (%u): %s\n",
                        varUInt(kvGet(archiveInfo, ARCHIVE_KEY_MISSING_WAL_NB_VAR)),
                        strZ(varStr(kvGet(archiveInfo, ARCHIVE_KEY_MISSING_WAL_LIST_VAR))));
                }
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

        // Get the backup label if specified
        const String *backupLabel = cfgOptionStrNull(cfgOptSet);

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // If a backup set was specified, see if the manifest exists
        if (backupLabel != NULL)
        {
            if (!strEq(cfgOptionStr(cfgOptOutput), CFGOPTVAL_INFO_OUTPUT_TEXT_STR))
                THROW(ConfigError, "option 'set' is currently only valid for text output");

            if (!storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel))))
            {
                THROW_FMT(
                    FileMissingError, "manifest does not exist for backup '%s'\n"
                    "HINT: is the backup listed when running the info command with --stanza option only?", strZ(backupLabel));
            }
        }

        // Get a list of stanzas in the backup directory.
        StringList *stanzaList = storageListP(storageRepo(), STORAGE_PATH_BACKUP_STR);
        VariantList *infoList = varLstNew();
        String *resultStr = strNew("");

        // If the backup storage exists, then search for and process any stanzas
        if (strLstSize(stanzaList) > 0)
            infoList = stanzaInfoList(stanza, stanzaList, backupLabel);

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

                        if (statusCode != INFO_STANZA_STATUS_CODE_MISSING_WAL_SEG)
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
