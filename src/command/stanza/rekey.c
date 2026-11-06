/***********************************************************************************************************************************
Stanza Rekey Command
***********************************************************************************************************************************/
#include <build.h>

#include <time.h>

#include "command/control/common.h"
#include "command/stanza/common.h"
#include "command/stanza/rekey.h"
#include "common/debug.h"
#include "common/format/format.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStanzaRekey(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify that a stop was not issued before proceeding
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int repoIdxMin = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1;
        unsigned int repoTotal = 0;

        // If the repo was specified then rekey only that repo
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxMin = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxMin;
        }

        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            const Storage *const storageRepoReadStanza = storageRepoIdx(repoIdx);
            const CipherType cipherType = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx);

            // Skip a repo that is not encrypted
            if (cipherType == cipherTypeNone)
                continue;

            repoTotal++;

            const bool rekey = cfgOptionIdxTest(cfgOptRepoCipherPassOld, repoIdx);

            LOG_INFO_FMT(
                CFGCMD_STANZA_REKEY " for stanza '%s' on %s", strZ(cfgOptionDisplay(cfgOptStanza)),
                cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            // Error when the new passphrase is the old passphrase
            if (rekey && strEq(cfgOptionIdxStr(cfgOptRepoCipherPass, repoIdx), cfgOptionIdxStr(cfgOptRepoCipherPassOld, repoIdx)))
            {
                THROW_FMT(
                    OptionInvalidValueError, "option '%s' must not be the same as option '%s'",
                    cfgOptionIdxName(cfgOptRepoCipherPass, repoIdx), cfgOptionIdxName(cfgOptRepoCipherPassOld, repoIdx));
            }

            // Load the info files (errors if missing)
            const CipherSpec *const cipherSpecMain = cfgCipherSpecMainIdx(repoIdx);

            InfoArchive *const infoArchive = infoArchiveLoadFile(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR, cipherSpecMain);
            InfoBackup *const infoBackup = infoBackupLoadFile(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR, cipherSpecMain);

            // Format of the repository
            const unsigned int format = infoArchiveFormat(infoArchive);

            // Warn when there is nothing to do. Format < 6 has a single archive key that cannot be rotated.
            if (!rekey && format < REPOSITORY_FORMAT_6)
            {
                LOG_WARN("archive key rotation requires repository format " STRINGIFY(REPOSITORY_FORMAT_6) " or higher");
                continue;
            }

            // Check the new passphrase
            if (rekey)
                stanzaCipherPassCheck(repoIdx, format, "rekey a stanza");

            // Rotate the archive key
            if (format >= REPOSITORY_FORMAT_6)
            {
                infoArchiveCipherRotate(infoArchive, cipherSpecGen(cipherType, format), time(NULL));

                LOG_INFO_FMT("rotate archive key to id %s", strZ(cipherSpecMapIdCurrent(infoArchiveCipherSpecMap(infoArchive))));
            }

            // Save the info files with the passphrase
            infoArchiveCipherPriorClear(infoArchive);
            infoArchiveSaveFile(infoArchive, storageRepoIdxWrite(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, cipherSpecMain);

            if (rekey)
            {
                infoBackupCipherPriorClear(infoBackup);
                infoBackupSaveFile(infoBackup, storageRepoIdxWrite(repoIdx), INFO_BACKUP_PATH_FILE_STR, cipherSpecMain);

                LOG_INFO("rekey info files with the new passphrase");
            }
        }

        // Error when no repo is encrypted
        if (repoTotal == 0)
            THROW(RepoInvalidError, CFGCMD_STANZA_REKEY " requires an encrypted repository");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
