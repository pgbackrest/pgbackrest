/***********************************************************************************************************************************
Restore Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/restore.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/bufferWrite.h" // !!! REMOVE WITH MANIFEST TEST CODE
#include "common/log.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
User id cache
***********************************************************************************************************************************/
static struct
{
    bool userIdSet;                                                 // Has the user id been fetched yet?
    uid_t userId;                                                   // Real user id of the calling process from getuid()
} restoreLocalData;

static uid_t
userId(void)
{
    FUNCTION_TEST_VOID();

    if (restoreLocalData.userIdSet)
        FUNCTION_TEST_RETURN(restoreLocalData.userId);

    restoreLocalData.userId = getuid();
    restoreLocalData.userIdSet = true;

    FUNCTION_TEST_RETURN(restoreLocalData.userId);
}

/***********************************************************************************************************************************
Recovery type constants
***********************************************************************************************************************************/
STRING_STATIC(RECOVERY_TYPE_PRESERVE_STR,                           "preserve");

/***********************************************************************************************************************************
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestMap(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Remap the data directory
        // -------------------------------------------------------------------------------------------------------------------------
        const String *pgPath = cfgOptionStr(cfgOptPgPath);
        const ManifestTarget *targetBase = manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR);

        if (!strEq(targetBase->path, pgPath))
        {
            LOG_INFO("remap data directory to '%s'", strPtr(pgPath));
            manifestTargetUpdate(manifest, targetBase->name, pgPath, NULL);
        }

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *tablespaceMap = varKv(cfgOption(cfgOptTablespaceMap));
        const String *tablespaceMapAllPath = cfgOptionStr(cfgOptTablespaceMapAll);

        if (tablespaceMap != NULL || tablespaceMapAllPath != NULL)
        {
            StringList *tablespaceRemapped = strLstNew();

            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                // Is this a tablespace?
                if (target->tablespaceId != 0)
                {
                    const String *tablespacePath = NULL;

                    // Check for an individual mapping for this tablespace
                    if (tablespaceMap != NULL)
                    {
                        // Attempt to get the tablespace by name
                        const String *tablespacePathByName = varStr(kvGet(tablespaceMap, VARSTR(target->tablespaceName)));

                        if (tablespacePathByName != NULL)
                            strLstAdd(tablespaceRemapped, target->tablespaceName);

                        // Attempt to get the tablespace by id
                        const String *tablespacePathById = varStr(
                            kvGet(tablespaceMap, VARSTR(varStrForce(VARUINT(target->tablespaceId)))));

                        if (tablespacePathById != NULL)
                            strLstAdd(tablespaceRemapped, varStrForce(VARUINT(target->tablespaceId)));

                        // Error when both are set but the paths are different
                        if (tablespacePathByName != NULL && tablespacePathById != NULL && !
                            strEq(tablespacePathByName, tablespacePathById))
                        {
                            THROW_FMT(
                                TablespaceMapError, "tablespace remapped by name '%s' and id %u with different paths",
                                strPtr(target->tablespaceName), target->tablespaceId);
                        }
                        // Else set the path by name
                        else if (tablespacePathByName != NULL)
                        {
                            tablespacePath = tablespacePathByName;
                        }
                        // Else set the path by id
                        else if (tablespacePathById != NULL)
                            tablespacePath = tablespacePathById;
                    }

                    // If not individual mapping check if all tablespaces are being remapped
                    if (tablespacePath == NULL && tablespaceMapAllPath != NULL)
                        tablespacePath = strNewFmt("%s/%s", strPtr(tablespaceMapAllPath), strPtr(target->tablespaceName));

                    // Remap tablespace if a mapping was found
                    if (tablespacePath != NULL)
                    {
                        LOG_INFO("map tablespace '%s' to '%s'", strPtr(target->name), strPtr(tablespacePath));

                        manifestTargetUpdate(manifest, target->name, tablespacePath, NULL);
                        manifestLinkUpdate(manifest, strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strPtr(target->name)), tablespacePath);
                    }
                }
            }

            // Error on invalid tablespaces
            if (tablespaceMap != NULL)
            {
                const VariantList *tablespaceMapList = kvKeyList(tablespaceMap);

                for (unsigned int tablespaceMapIdx = 0; tablespaceMapIdx < varLstSize(tablespaceMapList); tablespaceMapIdx++)
                {
                    const String *tablespace = varStr(varLstGet(tablespaceMapList, tablespaceMapIdx));

                    if (!strLstExists(tablespaceRemapped, tablespace))
                        THROW_FMT(TablespaceMapError, "unable to remap invalid tablespace '%s'", strPtr(tablespace));
                }
            }

            // Issue a warning message when remapping tablespaces in postgre < 9.2
            if (manifestData(manifest)->pgVersion <= PG_VERSION_92)
                LOG_WARN("update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= " PG_VERSION_92_STR);
        }

        // Remap links
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *linkMap = varKv(cfgOption(cfgOptLinkMap));
        bool linkAll = cfgOptionBool(cfgOptLinkAll);

        StringList *linkRemapped = strLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);

            // Is this a link?
            if (target->type == manifestTargetTypeLink && target->tablespaceId == 0)
            {
                const String *link = strSub(target->name, strSize(MANIFEST_TARGET_PGDATA_STR) + 1);
                const String *linkPath = linkMap == NULL ? NULL : varStr(kvGet(linkMap, VARSTR(link)));

                // Remap link if a mapping was found
                if (linkPath != NULL)
                {
                    LOG_INFO("map link '%s' to '%s'", strPtr(link), strPtr(linkPath));
                    manifestLinkUpdate(manifest, target->name, linkPath);

                    // If the link is a file separate the file name from the path to update the target
                    const String *linkFile = NULL;

                    if (target->file != NULL)
                    {
                        linkFile = strBase(linkPath);
                        linkPath = strPath(linkPath);
                    }

                    manifestTargetUpdate(manifest, target->name, linkPath, linkFile);

                    // Add to remapped list for later validation that all links were valid
                    strLstAdd(linkRemapped, link);
                }
                // If all links are not being restored then remove the target and link
                else if (!linkAll)
                {
                    if (target->file != NULL)
                        LOG_WARN("file link '%s' will be restored as a file at the same location", strPtr(link));
                    else
                    {
                        LOG_WARN(
                            "contents of directory link '%s' will be restored in a directory at the same location",
                            strPtr(link));
                    }

                    manifestLinkRemove(manifest, target->name);
                    manifestTargetRemove(manifest, target->name);
                    targetIdx--;
                }
            }
        }

        // Error on invalid links
        if (linkMap != NULL)
        {
            const VariantList *linkMapList = kvKeyList(linkMap);

            for (unsigned int linkMapIdx = 0; linkMapIdx < varLstSize(linkMapList); linkMapIdx++)
            {
                const String *link = varStr(varLstGet(linkMapList, linkMapIdx));

                if (!strLstExists(linkRemapped, link))
                    THROW_FMT(LinkMapError, "unable to remap invalid link '%s'", strPtr(link));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Clean the data directory of any paths/files/links that are not in the manifest
***********************************************************************************************************************************/
typedef struct RestoreCleanCallbackData
{
    const Manifest *manifest;                                       // Manifest to compare against
    const ManifestTarget *target;                                   // Current target being compared
    const String *targetPath;                                       // Path of target currently being compared
    const String *subPath;                                          // Subpath in target currently being compared
    bool basePath;                                                  // Is this the base path?
    bool delta;                                                     // Is this a delta restore?
    bool preserveRecoveryConf;                                      // Should the recovery.conf file be preserved?
    bool modifyAllowed;                                             // Are we allowed to modify the directory on this pass?
} RestoreCleanCallbackData;

static void
restoreCleanInfoListCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    RestoreCleanCallbackData *cleanData = (RestoreCleanCallbackData *)data;

    // Skip . path
    if (info->type == storageTypePath && strEq(info->name, DOT_STR))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Don't include the manifest or recovery.conf (when preserved) in the comparison or empty directory check
    if (cleanData->basePath && info->type == storageTypeFile &&
        (strEq(info->name, MANIFEST_FILE_STR) || (cleanData->preserveRecoveryConf && strEq(info->name, PG_FILE_RECOVERYCONF_STR))))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // If this is not a delta then error because the directory is expected to be empty
    if (!cleanData->delta)
    {
        THROW_FMT(
            PathNotEmptyError,
            "unable to restore to path '%s' because it contains files\n"
            "HINT: try using --delta if this is what you intended.",
            strPtr(cleanData->targetPath));
    }

    // If we are not allowed to modify any data in this pass then stop processing
    if (!cleanData->modifyAllowed)
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreClean(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Allocate data for each target
        RestoreCleanCallbackData *cleanDataList = memNew(sizeof(RestoreCleanCallbackData) * manifestTargetTotal(manifest));

        // Check that each target directory exists
        const String *basePath = manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path;

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *cleanData = &cleanDataList[targetIdx];

            cleanData->target = manifestTarget(manifest, targetIdx);
            cleanData->targetPath = strBeginsWith(cleanData->target->path, FSLASH_STR) ?
                cleanData->target->path : strNewFmt("%s/%s", strPtr(basePath), strPtr(cleanData->target->path));
            cleanData->basePath = targetIdx == 0;
            cleanData->delta = cfgOptionBool(cfgOptDelta);
            cleanData->preserveRecoveryConf = strEq(cfgOptionStr(cfgOptType), RECOVERY_TYPE_PRESERVE_STR);

            // Check that the path exists
            StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .ignoreMissing = true);

            if (!info.exists)
                THROW_FMT(PathMissingError, "unable to restore to missing path '%s'", strPtr(cleanData->targetPath));

            // Make sure our uid will be able to write to this directory
            if (userId() != 0 && userId() != info.userId)
                THROW_FMT(PathOpenError, "unable to restore to path '%s' not owned by current user", strPtr(cleanData->targetPath));

            if ((info.mode & 0700) != 0700)
                THROW_FMT(PathOpenError, "unable to restore to path '%s' without rwx permissions", strPtr(cleanData->targetPath));
        }

        (void)restoreCleanInfoListCallback;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Restore a backup
***********************************************************************************************************************************/
void
cmdRestore(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // PostgreSQL must be local
        if (!pgIsLocal(1))
            THROW(HostInvalidError, CFGCMD_RESTORE " command must be run on the " PG_NAME " host");

        // The PGDATA directory must exist
        // ??? We should also do this for the rest of the paths that backrest will not create (but later after manifest load)
        if (!storagePathExistsNP(storagePg(), NULL))
            THROW_FMT(PathMissingError, "$PGDATA directory '%s' does not exist", strPtr(cfgOptionStr(cfgOptPgPath)));

        // PostgreSQL must not be running
        if (storageExistsNP(storagePg(), STRDEF(PG_FILE_POSTMASTERPID)))
        {
            THROW_FMT(
                PostmasterRunningError,
                "unable to restore while PostgreSQL is running\n"
                    "HINT: presence of '" PG_FILE_POSTMASTERPID "' in '%s' indicates PostgreSQL is running.\n"
                    "HINT: remove '" PG_FILE_POSTMASTERPID "' only if PostgreSQL is not running.",
                strPtr(cfgOptionStr(cfgOptPgPath)));
        }

        // If the restore will be destructive attempt to verify that PGDATA looks like a valid PostgreSQL directory
        if ((cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce)) &&
            !storageExistsNP(storagePg(), STRDEF(PG_FILE_PGVERSION)) && !storageExistsNP(storagePg(), STRDEF(MANIFEST_FILE)))
        {
            LOG_WARN(
                "--delta or --force specified but unable to find '" PG_FILE_PGVERSION "' or '" MANIFEST_FILE "' in '%s' to"
                    " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                    " exist in the destination directories the restore will be aborted.",
               strPtr(cfgOptionStr(cfgOptPgPath)));

            cfgOptionSet(cfgOptDelta, cfgSourceDefault, VARBOOL(false));
            cfgOptionSet(cfgOptForce, cfgSourceDefault, VARBOOL(false));
        }

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Load backup.info
        InfoBackup *infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        // If backup set to restore is default (i.e. latest) then get the actual set
        const String *backupSet = NULL;

        if (cfgOptionSource(cfgOptSet) == cfgSourceDefault)
        {
            if (infoBackupDataTotal(infoBackup) == 0)
                THROW(BackupSetInvalidError, "no backup sets to restore");

            backupSet = strDup(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel);
        }
        // Otherwise check to make sure the specified backup set is valid
        else
        {
            backupSet = strDup(cfgOptionStr(cfgOptSet));

            bool found = false;

            for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
            {
                if (strEq(infoBackupData(infoBackup, backupIdx).backupLabel, backupSet))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                THROW_FMT(BackupSetInvalidError, "backup set %s is not valid", strPtr(backupSet));
        }

        // Load manifest
        Manifest *manifest = manifestLoadFile(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" MANIFEST_FILE, strPtr(backupSet)),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));

        // !!! THIS IS TEMPORARY TO DOUBLE-CHECK THE C MANIFEST CODE.  LOAD THE ORIGINAL MANIFEST AND COMPARE IT TO WHAT WE WOULD
        // SAVE TO DISK IF WE SAVED NOW.  THE LATER SAVE MAY HAVE MADE MODIFICATIONS BASED ON USER INPUT SO WE CAN'T USE THAT.
        // -------------------------------------------------------------------------------------------------------------------------
        if (cipherType(cfgOptionStr(cfgOptRepoCipherType)) == cipherTypeNone)                       // {uncovered_branch - !!! TEST}
        {
            Buffer *manifestTestPerlBuffer = storageGetNP(
                storageNewReadNP(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" MANIFEST_FILE, strPtr(backupSet))));

            Buffer *manifestTestCBuffer = bufNew(0);
            manifestSave(manifest, ioBufferWriteNew(manifestTestCBuffer));

            if (!bufEq(manifestTestPerlBuffer, manifestTestCBuffer))                                // {uncovered_branch - !!! TEST}
            {
                // Dump manifests to disk so we can check them with diff
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(MANIFEST_FILE ".expected")), manifestTestPerlBuffer);
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(MANIFEST_FILE ".actual")), manifestTestCBuffer);

                THROW_FMT(                                                                          // {uncovered - !!! TEST}
                    AssertError, "C and Perl manifests are not equal, files saved to '%s'",
                    strPtr(storagePathNP(storagePgWrite(), NULL)));
            }
        }

        // Sanity check to ensure the manifest has not been moved to a new directory
        const ManifestData *data = manifestData(manifest);

        if (!strEq(data->backupLabel, backupSet))
        {
            THROW_FMT(
                FormatError,
                "requested backup '%s' and manifest label '%s' do not match\n"
                "HINT: this indicates some sort of corruption (at the very least paths have been renamed).",
                strPtr(backupSet), strPtr(data->backupLabel));
        }

        // Log the backup set to restore
        LOG_INFO("restore backup set %s", strPtr(backupSet));

        // Map manifest
        restoreManifestMap(manifest);

        // Clean the data directory
        restoreClean(manifest);

        // Save manifest before any modifications are made to PGDATA
        manifestSave(manifest, storageWriteIo(storageNewWriteNP(storagePgWrite(), MANIFEST_FILE_STR)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
