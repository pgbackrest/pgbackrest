/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/control/common.h"
#include "command/stanza/delete.h"
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Helper functions to assist with testing
***********************************************************************************************************************************/
static void
manifestDelete(const Storage *storageRepoWriteStanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storageRepoWriteStanza);
    FUNCTION_TEST_END();

    ASSERT(storageRepoWriteStanza != NULL);

    // Get the list of backup directories from newest to oldest since don't want to invalidate a backup before
    // invalidating any backups that depend on it.
    StringList *backupList = strLstSort(
        storageListP(
            storageRepo(), STORAGE_REPO_BACKUP_STR,
            .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
        sortOrderDesc);

    // Delete all manifest files
    for (unsigned int idx = 0; idx < strLstSize(backupList); idx++)
    {
        storageRemoveP(
            storageRepoWriteStanza, strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(strLstGet(backupList, idx))));
        storageRemoveP(
            storageRepoWriteStanza,
            strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(strLstGet(backupList, idx))));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
stanzaDelete(const Storage *storageRepoWriteStanza, const StringList *archiveList, const StringList *backupList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storageRepoWriteStanza);
        FUNCTION_TEST_PARAM(STRING_LIST, archiveList);
        FUNCTION_TEST_PARAM(STRING_LIST, backupList);
    FUNCTION_TEST_END();

    ASSERT(storageRepoWriteStanza != NULL);

    bool result = false;

    // For most drivers, NULL indicates the directory does not exist at all. For those that do not support paths (e.g. S3) an
    // empty StringList will be returned; in such a case, the directory will attempt to be deleted (this is OK).
    if (archiveList != NULL || backupList != NULL)
    {
        bool archiveNotEmpty = (archiveList != NULL && !strLstEmpty(archiveList)) ? true : false;
        bool backupNotEmpty = (backupList != NULL && !strLstEmpty(backupList)) ? true : false;

        // If something exists in either directory, then remove
        if (archiveNotEmpty || backupNotEmpty)
        {
            // If the stop file does not exist, then error. This check is required even when --force is issued.
            if (!storageExistsP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))))
            {
                THROW_FMT(
                    FileMissingError, "stop file does not exist for stanza '%s'\n"
                    "HINT: has the pgbackrest stop command been run on this server for this stanza?",
                    strZ(cfgOptionStr(cfgOptStanza)));
            }

            // If a force has not been issued and Postgres is running, then error
            if (!cfgOptionBool(cfgOptForce) && storageExistsP(storagePg(), STRDEF(PG_FILE_POSTMASTERPID)))
            {
                THROW_FMT(
                    PgRunningError, PG_FILE_POSTMASTERPID " exists - looks like " PG_NAME " is running. "
                    "To delete stanza '%s' on repo%u, shut down " PG_NAME " for stanza '%s' and try again, or use --force.",
                    strZ(cfgOptionStr(cfgOptStanza)),
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, cfgOptionGroupIdxDefault(cfgOptGrpRepo)),
                    strZ(cfgOptionStr(cfgOptStanza)));
            }

            // Delete the archive info files
            if (archiveNotEmpty)
            {
                storageRemoveP(storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR);
                storageRemoveP(storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_COPY_STR);
            }

            // Delete the backup info files
            if (backupNotEmpty)
            {
                storageRemoveP(storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR);
                storageRemoveP(storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_COPY_STR);
            }

            // Remove manifest files
            manifestDelete(storageRepoWriteStanza);
        }

        // Recursively remove the entire stanza repo if exists. S3 will attempt to remove even if not.
        if (archiveList != NULL)
            storagePathRemoveP(storageRepoWriteStanza, STORAGE_REPO_ARCHIVE_STR, .recurse = true);

        if (backupList != NULL)
            storagePathRemoveP(storageRepoWriteStanza, STORAGE_REPO_BACKUP_STR, .recurse = true);

        // Remove the stop file - this will not error if the stop file does not exist. If the stanza directories existed but nothing
        // was in them, then no pgbackrest commands can be in progress without the info files so a stop is technically not necessary
        storageRemoveP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza)));

        result = true;
    }
    else
        result = true;

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
cmdStanzaDelete(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *storageRepoReadStanza = storageRepo();

        stanzaDelete(
            storageRepoWrite(), storageListP(storageRepoReadStanza, STORAGE_REPO_ARCHIVE_STR, .nullOnMissing = true),
            storageListP(storageRepoReadStanza, STORAGE_REPO_BACKUP_STR, .nullOnMissing = true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
