/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/posix/storage.h"
#include "storage/remote/storage.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Storage path constants
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_SPOOL_ARCHIVE_STR,                            STORAGE_SPOOL_ARCHIVE);
STRING_EXTERN(STORAGE_SPOOL_ARCHIVE_IN_STR,                         STORAGE_SPOOL_ARCHIVE_IN);
STRING_EXTERN(STORAGE_SPOOL_ARCHIVE_OUT_STR,                        STORAGE_SPOOL_ARCHIVE_OUT);

STRING_EXTERN(STORAGE_REPO_ARCHIVE_STR,                             STORAGE_REPO_ARCHIVE);
STRING_EXTERN(STORAGE_REPO_BACKUP_STR,                              STORAGE_REPO_BACKUP);

STRING_EXTERN(STORAGE_PATH_ARCHIVE_STR,                             STORAGE_PATH_ARCHIVE);
STRING_EXTERN(STORAGE_PATH_BACKUP_STR,                              STORAGE_PATH_BACKUP);

/***********************************************************************************************************************************
Error message when writable storage is requested in dry-run mode
***********************************************************************************************************************************/
#define WRITABLE_WHILE_DRYRUN                                                                                                      \
    "unable to get writable storage in dry-run mode or before dry-run is initialized"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct StorageHelperLocal
{
    MemContext *memContext;                                         // Mem context for storage helper

    const StorageHelper *helperList;                                // List of helpers to create storage

    Storage *storageLocal;                                          // Local read-only storage
    Storage *storageLocalWrite;                                     // Local write storage
    Storage **storagePg;                                            // PostgreSQL read-only storage
    Storage **storagePgWrite;                                       // PostgreSQL write storage
    Storage **storageRepo;                                          // Repository read-only storage
    Storage **storageRepoWrite;                                     // Repository write storage
    Storage *storageSpool;                                          // Spool read-only storage
    Storage *storageSpoolWrite;                                     // Spool write storage

    String *stanza;                                                 // Stanza for storage
    bool stanzaInit;                                                // Has the stanza been initialized?
    bool dryRunInit;                                                // Has dryRun been initialized?  If not disallow writes.
    bool dryRun;                                                    // Disallow writes in dry-run mode.
    RegExp *walRegExp;                                              // Regular expression for identifying wal files
} storageHelper;

/***********************************************************************************************************************************
Create the storage helper memory context
***********************************************************************************************************************************/
static void
storageHelperContextInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("StorageHelper")
            {
                storageHelper.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void storageHelperInit(const StorageHelper *const helperList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, helperList);
    FUNCTION_TEST_END();

    storageHelper.helperList = helperList;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageHelperDryRunInit(bool dryRun)
{
    FUNCTION_TEST_VOID();

    storageHelper.dryRunInit = true;
    storageHelper.dryRun = dryRun;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Initialize the stanza and error if it changes
***********************************************************************************************************************************/
static void
storageHelperStanzaInit(const bool stanzaRequired)
{
    FUNCTION_TEST_VOID();

    // If the stanza is NULL and the storage has not already been initialized then initialize the stanza
    if (!storageHelper.stanzaInit)
    {
        if (stanzaRequired && cfgOptionStrNull(cfgOptStanza) == NULL)
            THROW(AssertError, "stanza cannot be NULL for this storage object");

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.stanza = strDup(cfgOptionStrNull(cfgOptStanza));
            storageHelper.stanzaInit = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
const Storage *
storageLocal(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocal == NULL)
    {
        storageHelperContextInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocal =  storagePosixNewP(FSLASH_STR);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageLocal);
}

const Storage *
storageLocalWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocalWrite == NULL)
    {
        storageHelperContextInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocalWrite = storagePosixNewP(FSLASH_STR, .write = true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageLocalWrite);
}

/***********************************************************************************************************************************
Get pg storage for the specified host id
***********************************************************************************************************************************/
static Storage *
storagePgGet(unsigned int pgIdx, bool write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgIdx);
        FUNCTION_TEST_PARAM(BOOL, write);
    FUNCTION_TEST_END();

    Storage *result = NULL;

    // Use remote storage
    if (!pgIsLocal(pgIdx))
    {
        result = storageRemoteNew(
            STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, NULL,
            protocolRemoteGet(protocolStorageTypePg, pgIdx), cfgOptionUInt(cfgOptCompressLevelNetwork));
    }
    // Use Posix storage
    else
    {
        result = storagePosixNewP(cfgOptionIdxStr(cfgOptPgPath, pgIdx), .write = write);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const Storage *
storagePgIdx(unsigned int pgIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgIdx);
    FUNCTION_TEST_END();

    if (storageHelper.storagePg == NULL || storageHelper.storagePg[pgIdx] == NULL)
    {
        storageHelperContextInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            if (storageHelper.storagePg == NULL)
                storageHelper.storagePg = memNewPtrArray(cfgOptionGroupIdxTotal(cfgOptGrpPg));

            storageHelper.storagePg[pgIdx] = storagePgGet(pgIdx, false);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storagePg[pgIdx]);
}

const Storage *
storagePg(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storagePgIdx(cfgOptionGroupIdxDefault(cfgOptGrpPg)));
}

const Storage *
storagePgIdxWrite(unsigned int pgIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgIdx);
    FUNCTION_TEST_END();

    // Writes not allowed in dry-run mode
    if (!storageHelper.dryRunInit || storageHelper.dryRun)
        THROW(AssertError, WRITABLE_WHILE_DRYRUN);

    if (storageHelper.storagePgWrite == NULL || storageHelper.storagePgWrite[pgIdx] == NULL)
    {
        storageHelperContextInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            if (storageHelper.storagePgWrite == NULL)
                storageHelper.storagePgWrite = memNewPtrArray(cfgOptionGroupIdxTotal(cfgOptGrpPg));

            storageHelper.storagePgWrite[pgIdx] = storagePgGet(pgIdx, true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storagePgWrite[pgIdx]);
}

const Storage *
storagePgWrite(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storagePgIdxWrite(cfgOptionGroupIdxDefault(cfgOptGrpPg)));
}

/***********************************************************************************************************************************
Create the WAL regular expression
***********************************************************************************************************************************/
static void
storageHelperRepoInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.walRegExp == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            storageHelper.walRegExp = regExpNew(STRDEF("^[0-F]{24}"));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Construct a repo path from an expression and path
***********************************************************************************************************************************/
static String *
storageRepoPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);

    String *result = NULL;

    if (strEq(expression, STORAGE_REPO_ARCHIVE_STR))
    {
        // Construct the base path
        if (storageHelper.stanza != NULL)
            result = strCatFmt(strNew(), STORAGE_PATH_ARCHIVE "/%s", strZ(storageHelper.stanza));
        else
            result = strCatZ(strNew(), STORAGE_PATH_ARCHIVE);

        // If a subpath should be appended, determine if it is WAL path, else just append the subpath
        if (path != NULL)
        {
            StringList *pathSplit = strLstNewSplitZ(path, "/");
            String *file = strLstSize(pathSplit) == 2 ? strLstGet(pathSplit, 1) : NULL;

            if (file != NULL && regExpMatch(storageHelper.walRegExp, file))
                strCatFmt(result, "/%s/%s/%s", strZ(strLstGet(pathSplit, 0)), strZ(strSubN(file, 0, 16)), strZ(file));
            else
                strCatFmt(result, "/%s", strZ(path));
        }
    }
    else if (strEq(expression, STORAGE_REPO_BACKUP_STR))
    {
        // Construct the base path
        if (storageHelper.stanza != NULL)
            result = strCatFmt(strNew(), STORAGE_PATH_BACKUP "/%s", strZ(storageHelper.stanza));
        else
            result = strCatZ(strNew(), STORAGE_PATH_BACKUP);

        // Append subpath if provided
        if (path != NULL)
            strCatFmt(result, "/%s", strZ(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strZ(expression));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get the repo storage
***********************************************************************************************************************************/
static Storage *
storageRepoGet(unsigned int repoIdx, bool write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(BOOL, write);
    FUNCTION_TEST_END();

    Storage *result = NULL;

    // Use remote storage
    if (!repoIsLocal(repoIdx))
    {
        result = storageRemoteNew(
            STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, storageRepoPathExpression,
            protocolRemoteGet(protocolStorageTypeRepo, repoIdx), cfgOptionUInt(cfgOptCompressLevelNetwork));
    }
    // Use local storage
    else
    {
        // Search for the helper
        const StringId type = cfgOptionIdxStrId(cfgOptRepoType, repoIdx);

        if (storageHelper.helperList != NULL)
        {
            for (const StorageHelper *helper = storageHelper.helperList; helper->type != 0; helper++)
            {
                if (helper->type == type)
                {
                    result = helper->helper(repoIdx, write, storageRepoPathExpression);
                    break;
                }
            }
        }

        // If no helper was found it try Posix
        if (result == NULL)
        {
            CHECK(type == STORAGE_POSIX_TYPE);

            result = storagePosixNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), .write = write, .pathExpressionFunction = storageRepoPathExpression);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const Storage *
storageRepoIdx(unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    if (storageHelper.storageRepo == NULL)
    {
        storageHelperContextInit();
        storageHelperStanzaInit(false);
        storageHelperRepoInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepo = memNewPtrArray(cfgOptionGroupIdxTotal(cfgOptGrpRepo));
        }
        MEM_CONTEXT_END();
    }

    if (storageHelper.storageRepo[repoIdx] == NULL)
    {
        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepo[repoIdx] = storageRepoGet(repoIdx, false);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageRepo[repoIdx]);
}

const Storage *
storageRepo(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storageRepoIdx(cfgOptionGroupIdxDefault(cfgOptGrpRepo)));
}

const Storage *
storageRepoIdxWrite(unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    // Writes not allowed in dry-run mode
    if (!storageHelper.dryRunInit || storageHelper.dryRun)
        THROW(AssertError, WRITABLE_WHILE_DRYRUN);

    if (storageHelper.storageRepoWrite == NULL)
    {
        storageHelperContextInit();
        storageHelperStanzaInit(false);
        storageHelperRepoInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepoWrite = memNewPtrArray(cfgOptionGroupIdxTotal(cfgOptGrpRepo));
        }
        MEM_CONTEXT_END();
    }

    if (storageHelper.storageRepoWrite[repoIdx] == NULL)
    {
        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepoWrite[repoIdx] = storageRepoGet(repoIdx, true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageRepoWrite[repoIdx]);
}

const Storage *
storageRepoWrite(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storageRepoIdxWrite(cfgOptionGroupIdxDefault(cfgOptGrpRepo)));
}

/***********************************************************************************************************************************
Spool storage path expression
***********************************************************************************************************************************/
static String *
storageSpoolPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);
    ASSERT(storageHelper.stanza != NULL);

    String *result = NULL;

    if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE))
    {
        if (path == NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s", strZ(storageHelper.stanza));
        else
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strZ(storageHelper.stanza), strZ(path));
    }
    else if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_IN))
    {
        if (path == NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/in", strZ(storageHelper.stanza));
        else
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/in/%s", strZ(storageHelper.stanza), strZ(path));
    }
    else if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_OUT))
    {
        if (path == NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/out", strZ(storageHelper.stanza));
        else
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/out/%s", strZ(storageHelper.stanza), strZ(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strZ(expression));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const Storage *
storageSpool(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageSpool == NULL)
    {
        storageHelperContextInit();
        storageHelperStanzaInit(true);

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpool = storagePosixNewP(
                cfgOptionStr(cfgOptSpoolPath), .pathExpressionFunction = storageSpoolPathExpression);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageSpool);
}

const Storage *
storageSpoolWrite(void)
{
    FUNCTION_TEST_VOID();

    // Writes not allowed in dry-run mode
    if (!storageHelper.dryRunInit || storageHelper.dryRun)
        THROW(AssertError, WRITABLE_WHILE_DRYRUN);

    if (storageHelper.storageSpoolWrite == NULL)
    {
        storageHelperContextInit();
        storageHelperStanzaInit(true);

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpoolWrite = storagePosixNewP(
                cfgOptionStr(cfgOptSpoolPath), .write = true, .pathExpressionFunction = storageSpoolPathExpression);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageSpoolWrite);
}

/**********************************************************************************************************************************/
void
storageHelperFree(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext != NULL)
        memContextFree(storageHelper.memContext);

    storageHelper = (struct StorageHelperLocal){.memContext = NULL, .helperList = storageHelper.helperList};

    FUNCTION_TEST_RETURN_VOID();
}
