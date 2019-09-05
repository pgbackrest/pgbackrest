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
#include "info/infoManifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestRemap(InfoManifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    // Reassign the base path if specified
    const String *pgPath = cfgOptionStr(cfgOptPgPath);
    const InfoManifestTarget *targetBase = infoManifestTargetFind(manifest, INFO_MANIFEST_TARGET_PGDATA_STR);

    if (!strEq(targetBase->path, pgPath))
    {
        LOG_INFO("remap data directory to '%s'", strPtr(pgPath));
        infoManifestTargetUpdate(manifest, targetBase->name, pgPath);
    }

    // Remap tablespaces
    KeyValue tablespaceMap = cfgOption(cfgOptTablespaceMap);

    if (cfgOptionTest(cfgOptTablespaceMap) || cfgOptionTest(cfgOptTablespaceMapAll))
    {

        const String tablespaceMapAll = cfgOptionStr(cfgOptTablespaceMapAll);

        for (unsigned int targetIdx = 0; targetIdx < infoManifestTargetTotal(manifest); targetIdx++)
        {
            InfoManifestTarget *target = infoManifestTarget(manifest, targetIdx);

            // Is this a tablespace?
            if (target->tablespaceId != 0)
            {
                // Check for an individual mapping for this tablespace
                String *tablespacePath = VARSTR(kvGet(tablespaceMap, VARSTR(target->name)));

                if (tablespacePath == NULL &&

                // Get tablespace target
                const InfoManifestTarget *target = infoManifestTargetFindDefault(
                    manifest, varStr(varLstGet(tablespaceList, tablespaceIdx)), NULL);

                if (target == NULL || target->tablespaceId == 0)
                    THROW_FMT(ErrorTablespaceMap, "unable to remap invalid tablespace '%s'", strPtr(target->name));

                // Error if this tablespace has already been remapped
                if (strListExists(tablespaceRemapped))
                    THROW_FMT(ErrorTablespaceMap, "tablespace '%s' has already been remapped", strPtr(target->name));

                strLstAdd(tablespaceRemapped, target->name);

                // Remap tablespace
                infoManifestTargetUpdate(manifest, target.name, kvGet(tablespaceMap, VARSTR(tablespace));
                infoManifestLinkUpdate(manifest, strNewFmt(INFO_MANIFEST_TARGET_PGDATA "/%s", strPtr(target->name), );
                const String *tablespace = varStr(varLstGet(tablespaceList, tablespaceIdx));
                const String *tablespacePath = kvGet(tablespaceMap, VARSTR(tablespace));
            }
        }
        // my $oTablespaceRemapRequest = cfgOption(CFGOPT_TABLESPACE_MAP);
        //
        // for my $strKey (sort(keys(%{$oTablespaceRemapRequest})))
        // {
        //     my $bFound = false;
        //
        //     for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
        //     {
        //         if ($oManifest->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_ID, $strKey) ||
        //             $oManifest->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_NAME, $strKey))
        //         {
        //             if (defined(${$oTablespaceRemap}{$strTarget}))
        //             {
        //                 confess &log(ERROR, "tablespace ${strKey} has already been remapped to ${$oTablespaceRemap}{$strTarget}",
        //                              ERROR_TABLESPACE_MAP);
        //             }
        //
        //             ${$oTablespaceRemap}{$strTarget} = ${$oTablespaceRemapRequest}{$strKey};
        //             $bFound = true;
        //         }
        //     }
        //
        //     # Error if the tablespace was not found to be remapped
        //     if (!$bFound)
        //     {
        //         confess &log(ERROR, "cannot remap invalid tablespace ${strKey} to ${$oTablespaceRemapRequest}{$strKey}",
        //                      ERROR_TABLESPACE_MAP);
        //     }
        // }
    }

    // Remap all tablespaces except those already remapped individually
    //     if (cfgOptionTest(cfgOptTablespaceMapAll))
    // {
    // }

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
            !storageExistsNP(storagePg(), STRDEF(PG_FILE_PGVERSION)) && !storageExistsNP(storagePg(), STRDEF(INFO_MANIFEST_FILE)))
        {
            LOG_WARN(
                "--delta or --force specified but unable to find '" PG_FILE_PGVERSION "' or '" INFO_MANIFEST_FILE "' in '%s' to"
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
        InfoManifest *manifest = infoManifestLoadFile(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE, strPtr(backupSet)),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));

        // !!! THIS IS TEMPORARY TO DOUBLE-CHECK THE C MANIFEST CODE.  LOAD THE ORIGINAL MANIFEST AND COMPARE IT TO WHAT WE WOULD
        // SAVE TO DISK IF WE SAVED NOW.  THE LATER SAVE MAY HAVE MADE MODIFICATIONS BASED ON USER INPUT SO WE CAN'T USE THAT.
        // -------------------------------------------------------------------------------------------------------------------------
        if (cipherType(cfgOptionStr(cfgOptRepoCipherType)) == cipherTypeNone)                       // {uncovered_branch - !!! TEST}
        {
            Buffer *manifestTestPerlBuffer = storageGetNP(
                storageNewReadNP(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE, strPtr(backupSet))));

            Buffer *manifestTestCBuffer = bufNew(0);
            infoManifestSave(manifest, ioBufferWriteNew(manifestTestCBuffer));

            if (!bufEq(manifestTestPerlBuffer, manifestTestCBuffer))                                // {uncovered_branch - !!! TEST}
            {
                // Dump manifests to disk so we can check them with diff
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(INFO_MANIFEST_FILE ".expected")), manifestTestPerlBuffer);
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(INFO_MANIFEST_FILE ".actual")), manifestTestCBuffer);

                THROW_FMT(                                                                          // {uncovered - !!! TEST}
                    AssertError, "C and Perl manifests are not equal, files saved to '%s'",
                    strPtr(storagePathNP(storagePgWrite(), NULL)));
            }
        }

        // Sanity check to ensure the manifest has not been moved to a new directory
        const InfoManifestData *manifestData = infoManifestData(manifest);

        if (!strEq(manifestData->backupLabel, backupSet))
        {
            THROW_FMT(
                FormatError,
                "requested backup '%s' and manifest label '%s' do not match\n"
                "HINT: this indicates some sort of corruption (at the very least paths have been renamed).",
                strPtr(backupSet), strPtr(manifestData->backupLabel));
        }

        // Log the backup set to restore
        LOG_INFO("restore backup set %s", strPtr(backupSet));

        // Remap manifest
        restoreManifestRemap(manifest);

        // Save manifest before any modifications are made to PGDATA
        infoManifestSave(manifest, storageWriteIo(storageNewWriteNP(storagePgWrite(), INFO_MANIFEST_FILE_STR)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
