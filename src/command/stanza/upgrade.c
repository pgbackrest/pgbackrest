/***********************************************************************************************************************************
Stanza Upgrade Command
***********************************************************************************************************************************/
#include <build.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "command/check/common.h"
#include "command/control/common.h"
#include "command/stanza/common.h"
#include "command/stanza/upgrade.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStanzaUpgrade(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify that a stop was not issued before proceeding
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the version and system information - validating it if the database is online
        PgControl pgControl = pgValidate();

        // For each repository configured, upgrade the stanza
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(
                CFGCMD_STANZA_UPGRADE " for stanza '%s' on %s", strZ(cfgOptionDisplay(cfgOptStanza)),
                cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            const Storage *const storageRepoReadStanza = storageRepoIdx(repoIdx);
            const Storage *const storageRepoWriteStanza = storageRepoIdxWrite(repoIdx);
            bool infoArchiveUpgrade = false;
            bool infoBackupUpgrade = false;

            // Load the info files (errors if missing)
            InfoArchive *const infoArchive = infoArchiveLoadFile(
                storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));
            InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

            InfoBackup *const infoBackup = infoBackupLoadFile(
                storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));
            InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

            // Determine the format to write. An upgrade interrupted between the two info file saves leaves them at different
            // formats and the higher of the two is the only target that does not downgrade a file, so it is written to both even
            // when no format was requested. A format that is not already in the repository must still be requested, since the
            // option default would otherwise downgrade a repository that has already been upgraded.
            const unsigned int formatArchive = infoArchiveFormat(infoArchive);
            const unsigned int formatBackup = infoBackupFormat(infoBackup);
            unsigned int format = formatArchive > formatBackup ? formatArchive : formatBackup;

            if (cfgOptionIdxSource(cfgOptRepoFormat, repoIdx) != cfgSourceDefault)
            {
                const unsigned int formatRequest = cfgOptionIdxUInt(cfgOptRepoFormat, repoIdx);

                // Error when the format would be downgraded. Backups and archives written at a newer format would no longer be
                // gated by the info files, so an older version could read the info files and then fail on newer files.
                if (formatRequest < format)
                {
                    THROW_FMT(
                        FormatError,
                        "unable to downgrade repository format from %u to %u\n"
                        "HINT: backups and archives already written at format %u would not be readable by a version that only"
                        " supports format %u.",
                        format, formatRequest, format, formatRequest);
                }

                format = formatRequest;
            }

            // Since the file save of archive.info and backup.info are not atomic, then check and update each separately.
            // Update archive
            if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
            {
                infoArchivePgSet(infoArchive, pgControl.version, pgControl.systemId);
                infoArchiveUpgrade = true;
            }

            // Update backup
            if (pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId)
            {
                infoBackupPgSet(infoBackup, pgControl.version, pgControl.systemId, pgControl.catalogVersion);
                infoBackupUpgrade = true;
            }

            // Update the format on both info files together so they never disagree
            if (format != formatArchive || format != formatBackup)
            {
                // Report a repository that was found at two formats. It is repaired here but a prior upgrade did not finish, which
                // the user has not been told about since the run it happened on did not get far enough to report it.
                if (formatArchive != formatBackup)
                    LOG_WARN("repository format mismatch from an interrupted " CFGCMD_STANZA_UPGRADE " will be repaired");

                // Log the format the repository is migrating from, which is the lower of the two when an interrupted upgrade left
                // them at different formats. This cannot be undone and a version that does not support the new format will no
                // longer be able to read the stanza, so report it rather than migrating silently.
                LOG_INFO_FMT(
                    "upgrade repository format from %u to %u", formatArchive < formatBackup ? formatArchive : formatBackup, format);

                infoArchiveFormatSet(infoArchive, format);
                infoBackupFormatSet(infoBackup, format);

                infoArchiveUpgrade = true;
                infoBackupUpgrade = true;
            }

            // Throw an error if the info files do not match before saving (even if only one needed to be updated)
            checkStanzaInfo(repoIdx, infoArchivePg(infoArchive), infoBackupPg(infoBackup));

            // Save archive info
            if (infoArchiveUpgrade)
                infoArchiveSaveFile(infoArchive, storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));

            // Save backup info
            if (infoBackupUpgrade)
                infoBackupSaveFile(infoBackup, storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx));

            if (!(infoArchiveUpgrade || infoBackupUpgrade))
            {
                LOG_INFO_FMT(
                    "stanza '%s' on %s is already up to date", strZ(cfgOptionDisplay(cfgOptStanza)),
                    cfgOptionGroupName(cfgOptGrpRepo, repoIdx));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
