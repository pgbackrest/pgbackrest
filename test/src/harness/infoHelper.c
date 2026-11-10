/***********************************************************************************************************************************
Info Helper Harness
***********************************************************************************************************************************/
#include <build.h>

#include "common/io/bufferWrite.h"

#include "harness/debug.h"
#include "harness/infoHelper.h"
#include "harness/test.intern.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    bool statementClearDisabled;                                    // Is clearing cached info at each test statement disabled?
    unsigned int resultBeginTotal;                                  // Result tests begun when cached info was last cleared
} hrnInfoHelperLocal;

/**********************************************************************************************************************************/
void
hrnInfoHelperStatementClearSet(const bool enabled)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BOOL, enabled);
    FUNCTION_HARNESS_END();

    hrnInfoHelperLocal.statementClearDisabled = !enabled;

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Clear cached info when a test statement has begun since it was last cleared. The info objects are not freed since a test may still
reference them.
***********************************************************************************************************************************/
static void
hrnInfoHelperStatementClear(void)
{
    FUNCTION_HARNESS_VOID();

    if (!hrnInfoHelperLocal.statementClearDisabled && hrnInfoHelperLocal.resultBeginTotal != hrnTestResultBeginTotal())
    {
        if (infoHelperLocal.memContext != NULL && infoHelperLocal.cfgInitTotal == cfgInitTotal())
            memset(infoHelperLocal.repoList, 0, sizeof(InfoHelperRepo) * cfgOptionGroupIdxTotal(cfgOptGrpRepo));

        hrnInfoHelperLocal.resultBeginTotal = hrnTestResultBeginTotal();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Error when cached info does not match the info file
***********************************************************************************************************************************/
static void
hrnInfoHelperCheck(const char *const type, const unsigned int repoIdx, const Buffer *const cached, const Buffer *const file)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, type);
        FUNCTION_HARNESS_PARAM(UINT, repoIdx);
        FUNCTION_HARNESS_PARAM(BUFFER, cached);
        FUNCTION_HARNESS_PARAM(BUFFER, file);
    FUNCTION_HARNESS_END();

    if (!bufEq(cached, file))
    {
        THROW_FMT(
            AssertError,
            "cached %s info for %s does not match the %s.info file\n"
            "HINT: was the file changed outside the info helper or the cached info changed and not saved?\n"
            "HINT: load the configuration again to load the file again.",
            type, cfgOptionGroupName(cfgOptGrpRepo, repoIdx), type);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
infoArchiveRepoIdx() shim
***********************************************************************************************************************************/
InfoArchive *
infoArchiveRepoIdx(const unsigned int repoIdx)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, repoIdx);
    FUNCTION_HARNESS_END();

    hrnInfoHelperStatementClear();

    const InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    // Check cached info against the file
    if (repo->infoArchive != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            Buffer *const cached = bufNew(0);
            Buffer *const file = bufNew(0);

            infoArchiveSave(repo->infoArchive, ioBufferWriteNew(cached));
            infoArchiveSave(
                infoArchiveLoadFile(storageRepoIdx(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx)),
                ioBufferWriteNew(file));

            hrnInfoHelperCheck("archive", repoIdx, cached, file);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_HARNESS_RETURN(INFO_ARCHIVE, infoArchiveRepoIdx_SHIMMED(repoIdx));
}

/***********************************************************************************************************************************
infoBackupRepoIdx() shim
***********************************************************************************************************************************/
InfoBackup *
infoBackupRepoIdx(const unsigned int repoIdx, const InfoBackupRepoIdxParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, repoIdx);
        FUNCTION_HARNESS_PARAM(BOOL, param.reconstruct);
    FUNCTION_HARNESS_END();

    hrnInfoHelperStatementClear();

    const InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    // Check cached info against the file
    if (repo->infoBackup != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            Buffer *const cached = bufNew(0);
            Buffer *const file = bufNew(0);

            infoBackupSave(repo->infoBackup, ioBufferWriteNew(cached));
            infoBackupSave(
                infoBackupLoadFile(storageRepoIdx(repoIdx), INFO_BACKUP_PATH_FILE_STR, cfgCipherSpecMainIdx(repoIdx)),
                ioBufferWriteNew(file));

            hrnInfoHelperCheck("backup", repoIdx, cached, file);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_HARNESS_RETURN(INFO_BACKUP, infoBackupRepoIdx_SHIMMED(repoIdx, param));
}
