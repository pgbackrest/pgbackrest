/***********************************************************************************************************************************
Info Helper
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/helper.intern.h"
#include "info/infoArchive.intern.h"
#include "info/infoBackup.intern.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
typedef struct InfoHelperRepo
{
    InfoArchive *infoArchive;                                       // Archive info
    InfoBackup *infoBackup;                                         // Backup info
} InfoHelperRepo;

static struct InfoHelperLocal
{
    MemContext *memContext;                                         // Mem context for info helper
    unsigned int cfgInitTotal;                                      // Configuration init total when the cache was created
    InfoHelperRepo *repoList;                                       // Info by repo index
} infoHelperLocal;

/***********************************************************************************************************************************
Cached info for a repo. The cache is created again when the configuration has been loaded since the cache was created.
***********************************************************************************************************************************/
static InfoHelperRepo *
infoHelperRepo(const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    ASSERT(repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo));

    if (infoHelperLocal.memContext == NULL || infoHelperLocal.cfgInitTotal != cfgInitTotal())
    {
        const unsigned int repoTotal = cfgOptionGroupIdxTotal(cfgOptGrpRepo);

        if (infoHelperLocal.memContext != NULL)
            memContextFree(infoHelperLocal.memContext);

        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(InfoHelper, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
            {
                infoHelperLocal.memContext = MEM_CONTEXT_NEW();
                infoHelperLocal.repoList = memNew(sizeof(InfoHelperRepo) * repoTotal);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();

        memset(infoHelperLocal.repoList, 0, sizeof(InfoHelperRepo) * repoTotal);
        infoHelperLocal.cfgInitTotal = cfgInitTotal();
    }

    FUNCTION_TEST_RETURN_TYPE_P(InfoHelperRepo, &infoHelperLocal.repoList[repoIdx]);
}

/***********************************************************************************************************************************
Main cipher spec for a repo. The repo storage is fetched before the cipher spec is created. A remote repo sets the cipher options
when they are not configured locally and the cipher spec does not change once it is created. The order in which arguments are
evaluated is unspecified so fetching the storage in another argument of the same call is not enough.
***********************************************************************************************************************************/
static const CipherSpec *
infoHelperCipherSpec(const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    storageRepoIdx(repoIdx);

    FUNCTION_TEST_RETURN_CONST(CIPHER_SPEC, cfgCipherSpecMainIdx(repoIdx));
}

/***********************************************************************************************************************************
Archive info for a repo, loaded when it is not cached
***********************************************************************************************************************************/
static InfoArchive *
infoHelperArchive(const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    if (repo->infoArchive == NULL)
    {
        MEM_CONTEXT_BEGIN(infoHelperLocal.memContext)
        {
            repo->infoArchive = infoArchiveLoadFile(
                storageRepoIdx(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, infoHelperCipherSpec(repoIdx));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(INFO_ARCHIVE, repo->infoArchive);
}

/***********************************************************************************************************************************
Backup info for a repo, loaded when it is not cached
***********************************************************************************************************************************/
static InfoBackup *
infoHelperBackup(const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    if (repo->infoBackup == NULL)
    {
        InfoBackup *infoBackup;

        MEM_CONTEXT_BEGIN(infoHelperLocal.memContext)
        {
            infoBackup = infoBackupLoadFile(storageRepoIdx(repoIdx), INFO_BACKUP_PATH_FILE_STR, infoHelperCipherSpec(repoIdx));
        }
        MEM_CONTEXT_END();

        // Set the repository format before the backup info is cached. An error leaves the backup info uncached.
        cfgRepoFormatSet(repoIdx, infoBackupFormat(infoBackup));
        repo->infoBackup = infoBackup;
    }

    FUNCTION_TEST_RETURN(INFO_BACKUP, repo->infoBackup);
}

/**********************************************************************************************************************************/
FN_EXTERN InfoArchive *
infoArchiveRepoIdx(const unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, infoHelperArchive(repoIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoArchiveRepoIdxSet(const unsigned int repoIdx, InfoArchive *const infoArchive)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, infoArchive);
    FUNCTION_LOG_END();

    ASSERT(infoArchive != NULL);

    InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    CHECK(AssertError, repo->infoArchive == NULL, "archive info is already cached");

    repo->infoArchive = infoArchiveMove(infoArchive, infoHelperLocal.memContext);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoArchiveRepoIdxSave(const unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    infoArchiveSaveFile(
        infoHelperArchive(repoIdx), storageRepoIdxWrite(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, infoHelperCipherSpec(repoIdx));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN InfoBackup *
infoBackupRepoIdx(const unsigned int repoIdx, const InfoBackupRepoIdxParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, param.reconstruct);
    FUNCTION_LOG_END();

    InfoBackup *const result = infoHelperBackup(repoIdx);

    if (param.reconstruct)
        infoBackupReconstruct(result, storageRepoIdx(repoIdx));

    FUNCTION_LOG_RETURN(INFO_BACKUP, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoBackupRepoIdxSet(const unsigned int repoIdx, InfoBackup *const infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    InfoHelperRepo *const repo = infoHelperRepo(repoIdx);

    CHECK(AssertError, repo->infoBackup == NULL, "backup info is already cached");

    cfgRepoFormatSet(repoIdx, infoBackupFormat(infoBackup));
    repo->infoBackup = infoBackupMove(infoBackup, infoHelperLocal.memContext);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoBackupRepoIdxSave(const unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    infoBackupSaveFile(
        infoHelperBackup(repoIdx), storageRepoIdxWrite(repoIdx), INFO_BACKUP_PATH_FILE_STR, infoHelperCipherSpec(repoIdx));

    FUNCTION_LOG_RETURN_VOID();
}
