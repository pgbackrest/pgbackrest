/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/stanza/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************

***********************************************************************************************************************************/
void infoValidate(const InfoPgData *archiveInfo, const InfoPgData *backupInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, archiveInfo);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, backupInfo);
    FUNCTION_LOG_END();

    ASSERT(archiveInfo != NULL);
    ASSERT(backupInfo != NULL);

    // Error if there is a mismatch between the archive and backup info files
    if (archiveInfo->id != backupInfo->id || archiveInfo->systemId != backupInfo->systemId ||
        archiveInfo->version != backupInfo->version)
    {
        THROW_FMT(
            FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = %u, version = %s, system-id = %" PRIu64 "\n"
            "backup : id = %u, version = %s, system-id = %" PRIu64 "\n"
            "HINT: this may be a symptom of repository corruption!",
            archiveInfo->id, strPtr(pgVersionToStr(archiveInfo->version)), archiveInfo->systemId, backupInfo->id,
            strPtr(pgVersionToStr(backupInfo->version)), backupInfo->systemId);
    }

    FUNCTION_LOG_RETURN_VOID();
}
