/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/stanza/common.h"

// CSHANG We really need string constant for archive info , backup info, the repo, etc to be in a common file (maybe storage)? Like STRDEF(STORAGE_REPO_ARCHIVE) should maybe be a STRING_EXTERN / STRING_DECLARE. Any reason NOT to put them in storage.h? I don't think we can put something like FILE_ARCHIVE_INFO_STR in infoArchive.h as an external string because it must be in the infoArchive.c and that file would never use it.
/***********************************************************************************************************************************

***********************************************************************************************************************************/
void infoFilePgValidate(const InfoPgData *archiveInfo, const InfoPgData *backupInfo);
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
    FUNCTION_LOG_END();

    ASSERT(archiveInfo != NULL);
    ASSERT(backupInfo != NULL);

    // MEM_CONTEXT_TEMP_BEGIN()
    // {
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
    // }
    // MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
