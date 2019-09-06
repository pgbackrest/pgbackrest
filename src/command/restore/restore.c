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
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestRemap(Manifest *manifest)
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
                        LOG_INFO("remap tablespace '%s' to '%s'", strPtr(target->name), strPtr(tablespacePath));

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

        if (linkMap != NULL || linkAll)
        {
            StringList *linkRemapped = strLstNew();

            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                // Is this a link?
                if (target->type == manifestTargetTypeLink && target->tablespaceId == 0)
                {
                    const String *linkPath = linkMap == NULL ? NULL : varStr(kvGet(linkMap, VARSTR(target->name)));

                    // Remap link if a mapping was found
                    if (linkPath != NULL)
                    {
                        LOG_INFO("remap link '%s' to '%s'", strPtr(target->name), strPtr(linkPath));
                        manifestLinkUpdate(manifest, target->name, linkPath);

                        // If the link is a file separate the file name from the path to update the target
                        const String *linkFile = NULL;

                        if (target->file != NULL)
                        {
                            linkFile = strBase(linkPath);
                            linkPath = strPath(linkPath);
                        }

                        manifestTargetUpdate(manifest, target->name, linkPath, linkFile);
                    }
                    // If all links are not being restored then remove the target and link
                    else if (!linkAll)
                    {
                        if (target->file != NULL)
                            LOG_WARN("file link '%s' will be restored as a file at the same location", strPtr(target->name));
                        else
                        {
                            LOG_WARN(
                                "contents of directory link '%s' will be restored in a directory at the same location",
                                strPtr(target->name));
                        }

                        // $oManifest->remove(MANIFEST_SECTION_BACKUP_TARGET, $strTarget);
                        // $oManifest->remove(MANIFEST_SECTION_TARGET_LINK, $strTarget);
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
                        THROW_FMT(TablespaceMapError, "unable to remap invalid link '%s'", strPtr(link));
                }
            }
        }
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

        // Remap manifest
        restoreManifestRemap(manifest);

        // Save manifest before any modifications are made to PGDATA
        manifestSave(manifest, storageWriteIo(storageNewWriteNP(storagePgWrite(), MANIFEST_FILE_STR)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
