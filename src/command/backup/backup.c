/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <string.h>
// #include <sys/stat.h>
#include <time.h>
// #include <unistd.h>
//
#include "command/control/common.h"
#include "command/backup/backup.h"
// #include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
// #include "common/regExp.h"
// #include "common/user.h"
#include "config/config.h"
// #include "config/exec.h"
#include "info/infoBackup.h"
// #include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
// #include "protocol/parallel.h"
#include "storage/helper.h"
// #include "storage/write.intern.h"
// #include "version.h"

/***********************************************************************************************************************************
Make a backup
***********************************************************************************************************************************/
void
cmdBackup(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Get the start timestamp which will later be written into the manifest to track total backup time
    time_t timestampStart = time(NULL);

    // Verify the repo is local
    repoIsLocalVerify();

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load backup.info
        InfoBackup *infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));

        // Get the primary pg storage !!! HACKED TO ONE FOR NOW UNTIL WE BRING IN THE DB LOGIC
        const Storage *storagePgPrimary = storagePgId(1);

        // Get control information from the primary and validate it against backup info
        PgControl pgControl = pgControlFromFile(storagePgPrimary);

        if (pgControl.version != infoPg.version || pgControl.systemId != infoPg.systemId)
        {
            THROW_FMT(
                BackupMismatchError,
                PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
                strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(infoPg.version)),
                infoPg.systemId);
        }

        // Parameters that must be passed to Perl during migration
        StringList *paramList = strLstNew();
        strLstAdd(paramList, strNewFmt("%" PRIu64, (uint64_t)timestampStart));
        strLstAdd(paramList, strNewFmt("%u", infoPg.id));
        strLstAdd(paramList, pgVersionToStr(infoPg.version));
        strLstAdd(paramList, strNewFmt("%" PRIu64, infoPg.systemId));
        strLstAdd(paramList, strNewFmt("%u", pgControlVersion(infoPg.version)));
        strLstAdd(paramList, strNewFmt("%u", pgCatalogVersion(infoPg.version)));
        cfgCommandParamSet(paramList);

        // Shutdown protocol so Perl can take locks
        protocolFree();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
