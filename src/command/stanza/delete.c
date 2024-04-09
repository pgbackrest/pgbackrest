/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/common.h"
#include "command/control/common.h"
#include "command/stanza/delete.h"
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

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStanzaDelete(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const archiveList = storageListP(storageRepo(), STORAGE_REPO_ARCHIVE_STR, .nullOnMissing = true);
        const StringList *const backupList = storageListP(storageRepo(), STORAGE_REPO_BACKUP_STR, .nullOnMissing = true);

        // For most drivers, NULL indicates the directory does not exist at all. For those that do not support paths (e.g. S3) an
        // empty StringList will be returned; in such a case, the directory will attempt to be deleted (this is OK).
        if (archiveList != NULL || backupList != NULL)
        {
            const bool archiveNotEmpty = (archiveList != NULL && !strLstEmpty(archiveList)) ? true : false;
            const bool backupNotEmpty = (backupList != NULL && !strLstEmpty(backupList)) ? true : false;

            // If something exists in either directory, then remove
            if (archiveNotEmpty || backupNotEmpty)
            {
                // If the stop file does not exist, then error. This check is required even when --force is issued.
                if (!storageExistsP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))))
                {
                    THROW_FMT(
                        FileMissingError, "stop file does not exist for stanza '%s'\n"
                        "HINT: has the pgbackrest stop command been run on this server for this stanza?",
                        strZ(cfgOptionDisplay(cfgOptStanza)));
                }

                // If a force has not been issued and Postgres is running, then error
                if (!cfgOptionBool(cfgOptForce) && storageExistsP(storagePg(), STRDEF(PG_FILE_POSTMTRPID)))
                {
                    THROW_FMT(
                        PgRunningError, PG_FILE_POSTMTRPID " exists - looks like " PG_NAME " is running. "
                        "To delete stanza '%s' on %s, shut down " PG_NAME " for stanza '%s' and try again, or use --force.",
                        strZ(cfgOptionDisplay(cfgOptStanza)),
                        cfgOptionGroupName(cfgOptGrpRepo, cfgOptionGroupIdxDefault(cfgOptGrpRepo)),
                        strZ(cfgOptionDisplay(cfgOptStanza)));
                }

                // Delete the archive info files
                if (archiveNotEmpty)
                {
                    storageRemoveP(storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_STR);
                    storageRemoveP(storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_COPY_STR);
                }

                // Delete the backup info files
                if (backupNotEmpty)
                {
                    storageRemoveP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR);
                    storageRemoveP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_COPY_STR);
                }
            }

            // Recursively remove the entire stanza repo if exists. S3 will attempt to remove even if not.
            if (archiveList != NULL)
                storagePathRemoveP(storageRepoWrite(), STORAGE_REPO_ARCHIVE_STR, .recurse = true);

            if (backupList != NULL)
                storagePathRemoveP(storageRepoWrite(), STORAGE_REPO_BACKUP_STR, .recurse = true);

            // Remove the stop file - this will not error if the stop file does not exist. If the stanza directories existed but
            // nothing was in them, then no pgbackrest commands can be in progress without the info files so a stop is technically
            // not necessary
            storageRemoveP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
