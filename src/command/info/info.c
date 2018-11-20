/***********************************************************************************************************************************
Help Command
***********************************************************************************************************************************/
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>  // CSHANG only for debugging

#include "command/archive/common.h"
#include "common/debug.h"
#include "common/io/handle.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoManifest.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Define the console width - use a fixed with of 80 since this should be safe on virtually all consoles
***********************************************************************************************************************************/
#define CONSOLE_WIDTH                                               80  // CSHANG Only for TEXT format width - this out put should be restricted - whether do it through an assertion or put in a unit test.

// Naming convention: <sectionname>_KEY_<keyname>_STR. If the key exists in multiple sections, then <sectionname>_ is omitted.
STRING_STATIC(ARCHIVE_KEY_MIN_STR,                                  "min")
STRING_STATIC(ARCHIVE_KEY_MAX_STR,                                  "max")
STRING_STATIC(BACKREST_KEY_FORMAT_STR,                              "format")
STRING_STATIC(BACKREST_KEY_VERSION_STR,                             "version")
STRING_STATIC(BACKUP_KEY_BACKREST_STR,                              "backrest")
STRING_STATIC(BACKUP_KEY_INFO_STR,                                  "info")
STRING_STATIC(BACKUP_KEY_LABEL_STR,                                 "label")
STRING_STATIC(BACKUP_KEY_PRIOR_STR,                                 "prior")
STRING_STATIC(BACKUP_KEY_REFERENCE_STR,                             "reference")
STRING_STATIC(BACKUP_KEY_TIMESTAMP_STR,                             "timestamp")
STRING_STATIC(BACKUP_KEY_TYPE_STR,                                  "type")
STRING_STATIC(DB_KEY_ID_STR,                                        "id")
STRING_STATIC(DB_KEY_SYSTEM_ID_STR,                                 "system-id")
STRING_STATIC(DB_KEY_VERSION_STR,                                   "version")
STRING_STATIC(INFO_KEY_REPOSITORY_STR,                              "repository")
STRING_STATIC(KEY_ARCHIVE_STR,                                      "archive")
STRING_STATIC(KEY_DATABASE_STR,                                     "database")
STRING_STATIC(KEY_DELTA_STR,                                        "delta")
STRING_STATIC(KEY_SIZE_STR,                                         "size")
STRING_STATIC(KEY_START_STR,                                        "start")
STRING_STATIC(KEY_STOP_STR,                                         "stop")
STRING_STATIC(STANZA_KEY_BACKUP_STR,                                "backup")
STRING_STATIC(STANZA_KEY_CIPHER_STR,                                "cipher")
STRING_STATIC(STANZA_KEY_NAME_STR,                                  "name")
STRING_STATIC(STANZA_KEY_STATUS_STR,                                "status")
STRING_STATIC(STANZA_KEY_DB_STR,                                    "db")
STRING_STATIC(STATUS_KEY_CODE_STR,                                  "code")
STRING_STATIC(STATUS_KEY_MESSAGE_STR,                               "message")

#define INFO_STANZA_STATUS_CODE_OK                                  0
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_OK_STR,                    "ok")
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH                 1
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR,   "missing stanza path")
#define INFO_STANZA_STATUS_CODE_NO_BACKUP                           2
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR,             "no valid backups")
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA                 3
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR,   "missing stanza data")

// static void
// buildKv(Variant *kv, const String *section, const String *key, const Variant *value)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(VARIANT, kv);
//         FUNCTION_TEST_PARAM(STRING, section);
//         FUNCTION_TEST_PARAM(STRING, key);
//         FUNCTION_TEST_PARAM(VARIANT, value);
//         // Variant *sectionKey = varNewStr(section);
//         // KeyValue *sectionKv = varKv(kvGet(this->backupCurrent, sectionKey));
//         //
//         // if (sectionKv == NULL)
//         //     sectionKv = kvPutKv(this->backupCurrent, sectionKey);
//         //
//         // kvAdd(sectionKv, varNewStr(key), value);
static void
stanzaStatus(const int code, const String *message, Variant *stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(STRING, message);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);

        FUNCTION_TEST_ASSERT(code >= 0 && code <= 3);
        FUNCTION_TEST_ASSERT(message != NULL);
        FUNCTION_TEST_ASSERT(stanzaInfo != NULL);
    FUNCTION_TEST_END();

    Variant *stanzaStatus = varNewStr(STANZA_KEY_STATUS_STR);
    KeyValue *statusKv = kvPutKv(varKv(stanzaInfo), stanzaStatus);

    kvAdd(statusKv, varNewStr(STATUS_KEY_CODE_STR), varNewInt(code));
    kvAdd(statusKv, varNewStr(STATUS_KEY_MESSAGE_STR), varNewStr(message));

    FUNCTION_TEST_RESULT_VOID();
}

static VariantList *
archiveDbList(const String *stanza, const InfoPgData *pgData, VariantList *archiveSection, bool currentDb)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(INFO_PG_DATAP, pgData);
        FUNCTION_TEST_PARAM(VARIANT, archiveSection);
        FUNCTION_TEST_PARAM(BOOL, currentDb);

        FUNCTION_TEST_ASSERT(stanza != NULL);
        FUNCTION_TEST_ASSERT(pgData != NULL);
        FUNCTION_TEST_ASSERT(archiveSection != NULL);
    FUNCTION_TEST_END();

    InfoArchive *info = infoArchiveNew(
        storageRepo(), strNewFmt("%s/%s/%s", STORAGE_PATH_ARCHIVE, strPtr(stanza), INFO_ARCHIVE_FILE), false);

    // With multiple DB versions, the backup.info history-id may not be the same as archive.info history-id, so the
    // archive path must be built by retrieving the archive id given the db version and system id of the backup.info file.
    // If there is no match, an error will be thrown.
    const String *archiveId = infoArchiveIdMatch(info, pgData->version, pgData->systemId);

    String *archivePath = strNewFmt("%s/%s/%s", STORAGE_PATH_ARCHIVE, strPtr(stanza), strPtr(archiveId));
    String *archiveStart = NULL;
    String *archiveStop = NULL;
    Variant *archiveInfo = varNewKv();
printf("PATH: %s\n", strPtr(archivePath)); fflush(stdout);
    // Get a list of WAL directories in the archive repo from oldest to newest, if any exist
    StringList *walDir = storageListP(storageRepo(), archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR);
printf("PATH: %s, waldir %u\n", strPtr(archivePath), (walDir != NULL ? strLstSize(walDir) : 0)); fflush(stdout);
    if (walDir != NULL)
    {
        unsigned int sizeWalDir = strLstSize(walDir);

        if (sizeWalDir > 1)
            walDir = strLstSort(walDir, sortOrderAsc);

        for (unsigned int idx = 0; idx < sizeWalDir; idx++)
        {
            // Get a list of all WAL
            StringList *list = storageListP(
                storageRepo(), strNewFmt("%s/%s/%s", STORAGE_PATH_ARCHIVE, strPtr(archivePath), strPtr(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the oldest one as the archive start
            if (list != NULL && strLstSize(list) > 0)
            {
                // Sort the list from oldest to newest to get the oldest starting WAL archived for this DB
                list = strLstSort(list, sortOrderAsc);
                archiveStart = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }

        // Iterate through the directory list in the reverse so processing newest first. Cast comparison to an int for readability.
        for (unsigned int idx = sizeWalDir - 1; (int)idx > 0; idx--)
        {
            // Get a list of all WAL
            StringList *list = storageListP(
                storageRepo(), strNewFmt("%s/%s/%s", STORAGE_PATH_ARCHIVE, strPtr(archivePath), strPtr(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the newest one as the archive stop
            if (list != NULL && strLstSize(list) > 0)
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
        KeyValue *databaseInfo = kvPutKv(varKv(archiveInfo), varNewStr(KEY_DATABASE_STR));
        kvAdd(databaseInfo, varNewStr(DB_KEY_ID_STR), varNewUInt64(pgData->id));

        kvPut(varKv(archiveInfo), varNewStr(DB_KEY_ID_STR), varNewStr(archiveId));
        kvPut(varKv(archiveInfo), varNewStr(ARCHIVE_KEY_MIN_STR),
            (archiveStart != NULL ? varNewStr(archiveStart) : (Variant *)NULL));
        kvPut(varKv(archiveInfo), varNewStr(ARCHIVE_KEY_MAX_STR),
            (archiveStop != NULL ? varNewStr(archiveStop) : (Variant *)NULL));

        varLstAdd(archiveSection, archiveInfo);
    }

    FUNCTION_TEST_RESULT(VARIANT_LIST, archiveSection);
}

static VariantList *
backupList(const String *stanza, Variant *stanzaInfo, VariantList *backupSection, InfoBackup *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
        FUNCTION_TEST_PARAM(VARIANT, backupSection);
        FUNCTION_TEST_PARAM(INFO_BACKUP, info);

        FUNCTION_TEST_ASSERT(stanza != NULL);
        FUNCTION_TEST_ASSERT(stanzaInfo != NULL);
        FUNCTION_TEST_ASSERT(backupSection != NULL);
        FUNCTION_TEST_ASSERT(info != NULL);
    FUNCTION_TEST_END();

    // For each current backup, get the label and corresponding data
    StringList *backupKey = infoBackupCurrentKeyGet(info);

    if (backupKey != NULL)
    {
        // Build the backup section
        for (unsigned int keyIdx = 0; keyIdx < strLstSize(backupKey); keyIdx++)
        {
            String *backupLabel = strLstGet(backupKey, keyIdx);
            Variant *backupInfo = varNewKv();

            // main keys
            kvPut(varKv(backupInfo), varNewStr(BACKUP_KEY_LABEL_STR), varNewStr(backupLabel));
            kvPut(varKv(backupInfo), varNewStr(BACKUP_KEY_TYPE_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_TYPE_STR));
            kvPut(varKv(backupInfo), varNewStr(BACKUP_KEY_PRIOR_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_PRIOR_STR));
            kvPut(varKv(backupInfo), varNewStr(BACKUP_KEY_REFERENCE_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_BACKUP_KEY_BACKUP_REFERENCE_STR));

            // archive section
            KeyValue *archiveInfo = kvPutKv(varKv(backupInfo), varNewStr(KEY_ARCHIVE_STR));
            kvAdd(archiveInfo, varNewStr(KEY_START_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR));
            kvAdd(archiveInfo, varNewStr(KEY_STOP_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR));

            // backrest section
            KeyValue *backrestInfo = kvPutKv(varKv(backupInfo), varNewStr(BACKUP_KEY_BACKREST_STR));
            kvAdd(backrestInfo, varNewStr(BACKREST_KEY_FORMAT_STR),
                infoBackupCurrentGet(info, backupLabel, INI_KEY_FORMAT_STR));
            kvAdd(backrestInfo, varNewStr(BACKREST_KEY_VERSION_STR),
                infoBackupCurrentGet(info, backupLabel, INI_KEY_VERSION_STR));

            // database section
            KeyValue *dbInfo = kvPutKv(varKv(backupInfo), varNewStr(KEY_DATABASE_STR));
            kvAdd(dbInfo, varNewStr(DB_KEY_ID_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_KEY_DB_ID_STR));

            // info section
            KeyValue *infoInfo = kvPutKv(varKv(backupInfo), varNewStr(BACKUP_KEY_INFO_STR));
            kvAdd(infoInfo, varNewStr(KEY_SIZE_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR));
            kvAdd(infoInfo, varNewStr(KEY_DELTA_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR));

            // info:repository section
            KeyValue *repoInfo = kvPutKv(infoInfo, varNewStr(INFO_KEY_REPOSITORY_STR));
            kvAdd(repoInfo, varNewStr(KEY_SIZE_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR));
            kvAdd(repoInfo, varNewStr(KEY_DELTA_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR));

            // timestamp section
            KeyValue *timeInfo = kvPutKv(varKv(backupInfo), varNewStr(BACKUP_KEY_TIMESTAMP_STR));
            kvAdd(timeInfo, varNewStr(KEY_START_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR));
            kvAdd(timeInfo, varNewStr(KEY_STOP_STR),
                infoBackupCurrentGet(info, backupLabel, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR));

            varLstAdd(backupSection, backupInfo);
        }
    }

    FUNCTION_TEST_RESULT(VARIANT_LIST, backupSection);
}

static VariantList *
stanzaInfoList(const String *stanza, StringList *stanzaList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(STRING_LIST, stanzaList);

        FUNCTION_TEST_ASSERT(stanzaList != NULL);
    FUNCTION_TEST_END();

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
        Variant *stanzaInfo = varNewKv();
        VariantList *dbSection = varLstNew();
        VariantList *backupSection = varLstNew();
        VariantList *archiveSection = varLstNew();
        InfoBackup *info = NULL;

        // Catch certain errors
        TRY_BEGIN()
        {
            // Attempt to load the backup info file
            info = infoBackupNew(
                storageRepo(), strNewFmt("%s/%s/%s", STORAGE_PATH_BACKUP, strPtr(stanzaListName), INFO_BACKUP_FILE), false);
        }
        CATCH(FileMissingError)
        {
            // If there is no backup.info then set the status to indicate missing
            stanzaStatus(
                INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR, stanzaInfo);
        }
        CATCH(CryptoError)
        {
            THROW_FMT(
                CryptoError,
                "%s\n"
                "HINT: use option --stanza if encryption settings are different for the stanza than the global settings",
                errorMessage());
        }
        TRY_END();

        // Set the stanza name and cipher
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_NAME_STR), varNewStr(stanzaListName));
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_CIPHER_STR), varNewStr(cfgOptionStr(cfgOptRepoCipherType)));

        // If the backup.info file exists, get the database history information (newest to oldest) and corresponding archive
        if (info != NULL)
        {
            for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoBackupPg(info)); pgIdx++)
            {
                InfoPgData pgData = infoPgData(infoBackupPg(info), pgIdx);
                Variant *pgInfo = varNewKv();
                kvPut(varKv(pgInfo), varNewStr(DB_KEY_ID_STR), varNewUInt64(pgData.id));
                kvPut(varKv(pgInfo), varNewStr(DB_KEY_SYSTEM_ID_STR), varNewUInt64(pgData.systemId));
                kvPut(varKv(pgInfo), varNewStr(DB_KEY_VERSION_STR), varNewStr(pgVersionToStr(pgData.version)));

                varLstAdd(dbSection, pgInfo);
// CSHANG Probably just VOID the return for archiveDbList and backupList
                // Get the archive info for the DB
                archiveSection = archiveDbList(stanzaListName, &pgData, archiveSection, (pgIdx == 0 ? true : false));
            }
            // Get data for all existing backups for this stanza
            backupSection = backupList(stanzaListName, stanzaInfo, backupSection, info);
        }

        // Add the database history, backup and archive sections to the stanza info
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_DB_STR), varNewVarLst(dbSection));
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_BACKUP_STR), varNewVarLst(backupSection));
        kvPut(varKv(stanzaInfo), varNewStr(KEY_ARCHIVE_STR), varNewVarLst(archiveSection));

        // If a status has not already been set and there are no backups then set status to no backup
        if (kvGet(varKv(stanzaInfo), varNewStr(STANZA_KEY_STATUS_STR)) == NULL &&
            varLstSize(kvGetList(varKv(stanzaInfo), varNewStr(STANZA_KEY_BACKUP_STR))) == 0)
        {
            stanzaStatus(INFO_STANZA_STATUS_CODE_NO_BACKUP, INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR, stanzaInfo);
        }

        // If a status has still not been set then set it to OK
        if (kvGet(varKv(stanzaInfo), varNewStr(STANZA_KEY_STATUS_STR)) == NULL)
            stanzaStatus(INFO_STANZA_STATUS_CODE_OK, INFO_STANZA_STATUS_MESSAGE_OK_STR, stanzaInfo);

        varLstAdd(result, stanzaInfo);
    }

    // If looking for a specific stanza and it was not found, set minimum info and the status
    if (stanza != NULL && !stanzaFound)
    {
        Variant *stanzaInfo = varNewKv();
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_NAME_STR), varNewStr(stanza));

        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_DB_STR), varNewVarLst(varLstNew()));
        kvPut(varKv(stanzaInfo), varNewStr(STANZA_KEY_BACKUP_STR), varNewVarLst(varLstNew()));

        stanzaStatus(INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR, stanzaInfo);
        varLstAdd(result, stanzaInfo);
    }

    FUNCTION_TEST_RESULT(VARIANT_LIST, result);
}
/* A.CSHANG Why does helpRender need a memeory context but helpRenderText and helpRenderValue do not? Aren't all the variables created
in the temp mem context of cmdHelp?  Since all these are static, I don't understand why a memory context is needed anywhere except
the cmdXXX function...   render is the MAIN function so best practive to have mem context.
*/

static String *
infoRender(void)
{
    FUNCTION_DEBUG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get stanza if specified
        const String *stanza = cfgOptionTest(cfgOptStanza) ? cfgOptionStr(cfgOptStanza) : NULL;

        // Get a list of stanzas in the backup directory
        StringList *stanzaList = storageListNP(storageRepo(), strNew(STORAGE_PATH_BACKUP));

        VariantList *infoList = varLstNew();
        String *resultStr = strNew("");

        // If there are any stanzas to process then process them
        if (stanzaList != NULL)
            infoList = stanzaInfoList(stanza, stanzaList);

        // CSHANG Dave says to CREATE #DEFINES for the options, but where? Shouldn't this be in the config system?
        // Dave - for now just create #defines in the info.h which also needs the cmdInfo to be defined in there
        if (strEqZ(cfgOptionStr(cfgOptOutput), "text"))
        {
            // CSHANG call a formatText function and then if it returns NULL the following would be returned?
            resultStr = strNewFmt("No stanzas exist in %s\n", strPtr(storagePathNP(storageRepo(), NULL)));
        }
        else if (strEqZ(cfgOptionStr(cfgOptOutput), "json"))
        {
            resultStr = varToJson(varNewVarLst(infoList), 4);
        }

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(resultStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}

// /***********************************************************************************************************************************
// Helper function for helpRender() to make output look good on a console
// ***********************************************************************************************************************************/
// static String *
// helpRenderText(const String *text, size_t indent, bool indentFirst, size_t length)
// {
//     FUNCTION_DEBUG_BEGIN(logLevelTrace);
//         FUNCTION_DEBUG_PARAM(STRING, text);
//         FUNCTION_DEBUG_PARAM(SIZE, indent);
//         FUNCTION_DEBUG_PARAM(BOOL, indentFirst);
//         FUNCTION_DEBUG_PARAM(SIZE, length);
//
//         FUNCTION_DEBUG_ASSERT(text != NULL);
//         FUNCTION_DEBUG_ASSERT(length > 0);
//     FUNCTION_DEBUG_END();
//
//     String *result = strNew("");
//
//     // Split the text into paragraphs
//     StringList *lineList = strLstNewSplitZ(text, "\n");
//
//     // Iterate through each paragraph and split the lines according to the line length
//     for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
//     {
//         // Add LF if there is already content
//         if (strSize(result) != 0)
//             strCat(result, "\n");
//
//         // Split the paragraph into lines that don't exceed the line length
//         StringList *partList = strLstNewSplitSizeZ(strLstGet(lineList, lineIdx), " ", length - indent);
//
//         for (unsigned int partIdx = 0; partIdx < strLstSize(partList); partIdx++)
//         {
//             // Indent when required
//             if (partIdx != 0 || indentFirst)
//             {
//                 if (partIdx != 0)
//                     strCat(result, "\n");
//
//                 if (strSize(strLstGet(partList, partIdx)))
//                     strCatFmt(result, "%*s", (int)indent, "");
//             }
//
//             // Add the line
//             strCat(result, strPtr(strLstGet(partList, partIdx)));
//         }
//     }
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }
//
// /***********************************************************************************************************************************
// Helper function for helpRender() to output values as strings
// ***********************************************************************************************************************************/
// static String *
// helpRenderValue(const Variant *value)
// {
//     FUNCTION_DEBUG_BEGIN(logLevelTrace);
//         FUNCTION_DEBUG_PARAM(VARIANT, value);
//     FUNCTION_DEBUG_END();
//
//     String *result = NULL;
//
//     if (value != NULL)
//     {
//         if (varType(value) == varTypeBool)
//         {
//             if (varBool(value))
//                 result = strNew("y");
//             else
//                 result = strNew("n");
//         }
//         else if (varType(value) == varTypeKeyValue)
//         {
//             result = strNew("");
//
//             const KeyValue *optionKv = varKv(value);
//             const VariantList *keyList = kvKeyList(optionKv);
//
//             for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
//             {
//                 if (keyIdx != 0)
//                     strCat(result, ", ");
//
//                 strCatFmt(
//                     result, "%s=%s", strPtr(varStr(varLstGet(keyList, keyIdx))),
//                     strPtr(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx)))));
//             }
//         }
//         else if (varType(value) == varTypeVariantList)
//         {
//             result = strNew("");
//
//             const VariantList *list = varVarLst(value);
//
//             for (unsigned int listIdx = 0; listIdx < varLstSize(list); listIdx++)
//             {
//                 if (listIdx != 0)
//                     strCat(result, ", ");
//
//                 strCatFmt(result, "%s", strPtr(varStr(varLstGet(list, listIdx))));
//             }
//         }
//         else
//             result = varStrForce(value);
//     }
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }
//
// /***********************************************************************************************************************************
// Render help to a string
// ***********************************************************************************************************************************/
// static String *
// helpRender(void)
// {
//     FUNCTION_DEBUG_VOID(logLevelDebug);
//
//     String *result = strNew(PGBACKREST_NAME " " PGBACKREST_VERSION);
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         // Message for more help when it is available
//         String *more = NULL;
//
//         // Display general help
//         if (cfgCommand() == cfgCmdHelp || cfgCommand() == cfgCmdNone)
//         {
//             strCat(
//                 result,
//                 " - General help\n"
//                 "\n"
//                 "Usage:\n"
//                 "    " PGBACKREST_BIN " [options] [command]\n"
//                 "\n"
//                 "Commands:\n");
//
//             // Find size of longest command name
//             size_t commandSizeMax = 0;
//
//             for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
//             {
//                 if (commandId == cfgCmdNone)
//                     continue;
//
//                 if (strlen(cfgCommandName(commandId)) > commandSizeMax)
//                     commandSizeMax = strlen(cfgCommandName(commandId));
//             }
//
//             // Output help for each command
//             for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
//             {
//                 if (commandId == cfgCmdNone)
//                     continue;
//
//                 const char *helpSummary = cfgDefCommandHelpSummary(cfgCommandDefIdFromId(commandId));
//
//                 if (helpSummary != NULL)
//                 {
//                     strCatFmt(
//                         result, "    %s%*s%s\n", cfgCommandName(commandId),
//                         (int)(commandSizeMax - strlen(cfgCommandName(commandId)) + 2), "",
//                         strPtr(helpRenderText(strNew(helpSummary), commandSizeMax + 6, false, CONSOLE_WIDTH)));
//                 }
//             }
//
//             // Construct message for more help
//             more = strNew("[command]");
//         }
//         else
//         {
//             ConfigCommand commandId = cfgCommand();
//             ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(commandId);
//             const char *commandName = cfgCommandName(commandId);
//
//             // Output command part of title
//             strCatFmt(result, " - '%s' command", commandName);
//
//             // If no additional params then this is command help
//             if (strLstSize(cfgCommandParam()) == 0)
//             {
//                 // Output command summary and description
//                 strCatFmt(
//                     result,
//                     " help\n"
//                     "\n"
//                     "%s\n"
//                     "\n"
//                     "%s\n",
//                     strPtr(helpRenderText(strNew(cfgDefCommandHelpSummary(commandDefId)), 0, true, CONSOLE_WIDTH)),
//                     strPtr(helpRenderText(strNew(cfgDefCommandHelpDescription(commandDefId)), 0, true, CONSOLE_WIDTH)));
//
//                 // Construct key/value of sections and options
//                 KeyValue *optionKv = kvNew();
//                 size_t optionSizeMax = 0;
//
//                 for (unsigned int optionDefId = 0; optionDefId < cfgDefOptionTotal(); optionDefId++)
//                 {
//                     if (cfgDefOptionValid(commandDefId, optionDefId) && !cfgDefOptionInternal(commandDefId, optionDefId))
//                     {
//                         String *section = NULL;
//
//                         if (cfgDefOptionHelpSection(optionDefId) != NULL)
//                             section = strNew(cfgDefOptionHelpSection(optionDefId));
//
//                         if (section == NULL ||
//                             (!strEqZ(section, "general") && !strEqZ(section, "log") && !strEqZ(section, "repository") &&
//                              !strEqZ(section, "stanza")))
//                         {
//                             section = strNew("command");
//                         }
//
//                         kvAdd(optionKv, varNewStr(section), varNewInt((int)optionDefId));
//
//                         if (strlen(cfgDefOptionName(optionDefId)) > optionSizeMax)
//                             optionSizeMax = strlen(cfgDefOptionName(optionDefId));
//                     }
//                 }
//
//                 // Output sections
//                 StringList *sectionList = strLstSort(strLstNewVarLst(kvKeyList(optionKv)), sortOrderAsc);
//
//                 for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
//                 {
//                     const String *section = strLstGet(sectionList, sectionIdx);
//
//                     strCatFmt(result, "\n%s Options:\n\n", strPtr(strFirstUpper(strDup(section))));
//
//                     // Output options
//                     VariantList *optionList = kvGetList(optionKv, varNewStr(section));
//
//                     for (unsigned int optionIdx = 0; optionIdx < varLstSize(optionList); optionIdx++)
//                     {
//                         ConfigDefineOption optionDefId = varInt(varLstGet(optionList, optionIdx));
//                         ConfigOption optionId = cfgOptionIdFromDefId(optionDefId, 0);
//
//                         // Get option summary
//                         String *summary = strFirstLower(strNewN(
//                             cfgDefOptionHelpSummary(commandDefId, optionDefId),
//                             strlen(cfgDefOptionHelpSummary(commandDefId, optionDefId)) - 1));
//
//                         // Ouput current and default values if they exist
//                         String *defaultValue = helpRenderValue(cfgOptionDefault(optionId));
//                         String *value = NULL;
//
//                         if (cfgOptionSource(optionId) != cfgSourceDefault)
//                             value = helpRenderValue(cfgOption(optionId));
//
//                         if (value != NULL || defaultValue != NULL)
//                         {
//                             strCat(summary, " [");
//
//                             if (value != NULL)
//                                 strCatFmt(summary, "current=%s", strPtr(value));
//
//                             if (defaultValue != NULL)
//                             {
//                                 if (value != NULL)
//                                     strCat(summary, ", ");
//
//                                 strCatFmt(summary, "default=%s", strPtr(defaultValue));
//                             }
//
//                             strCat(summary, "]");
//                         }
//
//                         // Output option help
//                         strCatFmt(
//                             result, "  --%s%*s%s\n",
//                             cfgDefOptionName(optionDefId), (int)(optionSizeMax - strlen(cfgDefOptionName(optionDefId)) + 2), "",
//                             strPtr(helpRenderText(summary, optionSizeMax + 6, false, CONSOLE_WIDTH)));
//                     }
//                 }
//
//                 // Construct message for more help if there are options
//                 if (optionSizeMax > 0)
//                     more = strNewFmt("%s [option]", commandName);
//             }
//             // Else option help for the specified command
//             else
//             {
//                 // Make sure only one option was specified
//                 if (strLstSize(cfgCommandParam()) > 1)
//                     THROW(ParamInvalidError, "only one option allowed for option help");
//
//                 // Ensure the option is valid
//                 const char *optionName = strPtr(strLstGet(cfgCommandParam(), 0));
//                 ConfigOption optionId = cfgOptionId(optionName);
//
//                 if (cfgOptionId(optionName) == -1)
//                 {
//                     if (cfgDefOptionId(optionName) != -1)
//                         optionId = cfgOptionIdFromDefId(cfgDefOptionId(optionName), 0);
//                     else
//                         THROW_FMT(OptionInvalidError, "option '%s' is not valid for command '%s'", optionName, commandName);
//                 }
//
//                 // Output option summary and description
//                 ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
//
//                 strCatFmt(
//                     result,
//                     " - '%s' option help\n"
//                     "\n"
//                     "%s\n"
//                     "\n"
//                     "%s\n",
//                     optionName,
//                     strPtr(helpRenderText(strNew(cfgDefOptionHelpSummary(commandDefId, optionDefId)), 0, true, CONSOLE_WIDTH)),
//                     strPtr(helpRenderText(strNew(cfgDefOptionHelpDescription(commandDefId, optionDefId)), 0, true, CONSOLE_WIDTH)));
//
//                 // Ouput current and default values if they exist
//                 String *defaultValue = helpRenderValue(cfgOptionDefault(optionId));
//                 String *value = NULL;
//
//                 if (cfgOptionSource(optionId) != cfgSourceDefault)
//                     value = helpRenderValue(cfgOption(optionId));
//
//                 if (value != NULL || defaultValue != NULL)
//                 {
//                     strCat(result, "\n");
//
//                     if (value != NULL)
//                         strCatFmt(result, "current: %s\n", strPtr(value));
//
//                     if (defaultValue != NULL)
//                         strCatFmt(result, "default: %s\n", strPtr(defaultValue));
//                 }
//
//                 // Output alternate names (call them deprecated so the user will know not to use them)
//                 if (cfgDefOptionHelpNameAlt(optionDefId))
//                 {
//                     strCat(result, "\ndeprecated name");
//
//                     if (cfgDefOptionHelpNameAltValueTotal(optionDefId) > 1) // {uncovered - no option has more than one alt name}
//                         strCat(result, "s");                                // {+uncovered}
//
//                     strCat(result, ": ");
//
//                     for (unsigned int nameAltIdx = 0; nameAltIdx < cfgDefOptionHelpNameAltValueTotal(optionDefId); nameAltIdx++)
//                     {
//                         if (nameAltIdx != 0)                                // {uncovered - no option has more than one alt name}
//                             strCat(result, ", ");                           // {+uncovered}
//
//                         strCat(result, cfgDefOptionHelpNameAltValue(optionDefId, nameAltIdx));
//                     }
//
//                     strCat(result, "\n");
//                 }
//             }
//         }
//
//         // If there is more help available output a message to let the user know
//         if (more != NULL)
//             strCatFmt(result, "\nUse '" PGBACKREST_BIN " help %s' for more information.\n", strPtr(more));
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }

/***********************************************************************************************************************************
Render info and output to stdout
***********************************************************************************************************************************/
void
cmdInfo(void)
{
    FUNCTION_DEBUG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioHandleWriteOneStr(STDOUT_FILENO, infoRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}
