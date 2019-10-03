/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <string.h>
// #include <sys/stat.h>
// #include <time.h>
// #include <unistd.h>
//
// #include "command/restore/protocol.h"
#include "command/backup/backup.h"
// #include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
// #include "common/regExp.h"
// #include "common/user.h"
// #include "config/config.h"
// #include "config/exec.h"
// #include "info/infoBackup.h"
// #include "info/manifest.h"
// #include "postgres/interface.h"
// #include "postgres/version.h"
#include "protocol/helper.h"
// #include "protocol/parallel.h"
// #include "storage/helper.h"
// #include "storage/write.intern.h"
// #include "version.h"

/***********************************************************************************************************************************
Make a backup
***********************************************************************************************************************************/
void
cmdBackup(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify the repo is local
    repoIsLocalVerify();

    MEM_CONTEXT_TEMP_BEGIN()
    {
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
