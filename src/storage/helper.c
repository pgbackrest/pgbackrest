/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.h"
#include "storage/driver/posix/storage.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context for storage helper

    Storage *storageLocal;                                          // Local read-only storage
    Storage *storageLocalWrite;                                     // Local write storage
    Storage *storageRepo;                                           // Repository read-only storage
    Storage *storageSpool;                                          // Spool read-only storage
    Storage *storageSpoolWrite;                                     // Spool write storage

    String *stanza;                                                 // Stanza for storage
    RegExp *walRegExp;                                              // Regular expression for identifying wal files
} storageHelper;

/***********************************************************************************************************************************
Create the storage helper memory context
***********************************************************************************************************************************/
static void
storageHelperInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            storageHelper.memContext = memContextNew("storageHelper");
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Initialize the stanza and error if it changes
***********************************************************************************************************************************/
static void
storageHelperStanzaInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.stanza == NULL)
    {
        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.stanza = strDup(cfgOptionStr(cfgOptStanza));
        }
        MEM_CONTEXT_END();
    }
    else if (!strEq(storageHelper.stanza, cfgOptionStr(cfgOptStanza)))
    {
        THROW_FMT(
            AssertError, "stanza has changed from '%s' to '%s'", strPtr(storageHelper.stanza), strPtr(cfgOptionStr(cfgOptStanza)));
    }

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Get a local storage object
***********************************************************************************************************************************/
const Storage *
storageLocal(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocal == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocal = storageDriverPosixInterface(
                storageDriverPosixNew(
                    strNew("/"), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageHelper.storageLocal);
}

/***********************************************************************************************************************************
Get a writable local storage object

This should be used very sparingly.  If writes are not needed then always use storageLocal() or a specific storage object instead.
***********************************************************************************************************************************/
const Storage *
storageLocalWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocalWrite == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocalWrite = storageDriverPosixInterface(
                storageDriverPosixNew(
                    strNew("/"), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageHelper.storageLocalWrite);
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
static String *
storageRepoPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);

        FUNCTION_TEST_ASSERT(expression != NULL);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (strEqZ(expression, STORAGE_REPO_ARCHIVE))
    {
        result = strNewFmt("archive/%s", strPtr(storageHelper.stanza));

        if (path != NULL)
        {
            StringList *pathSplit = strLstNewSplitZ(path, "/");
            String *file = strLstSize(pathSplit) == 2 ? strLstGet(pathSplit, 1) : NULL;

            if (file != NULL && regExpMatch(storageHelper.walRegExp, file))
                strCatFmt(result, "/%s/%s/%s", strPtr(strLstGet(pathSplit, 0)), strPtr(strSubN(file, 0, 16)), strPtr(file));
            else
                strCatFmt(result, "/%s", strPtr(path));
        }
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Get the repo storage
***********************************************************************************************************************************/
static Storage *
storageRepoGet(const String *type, bool write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(BOOL, write);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(write == false);                       // ??? Need to create cifs driver before storage can be writable
    FUNCTION_TEST_END();

    Storage *result = NULL;

    // For now treat posix and cifs drivers as if they are the same.  This won't be true once the repository storage becomes
    // writable but for now it's OK.  The assertion above should pop if we try to create writable repo storage.
    if (strEqZ(type, STORAGE_TYPE_POSIX) || strEqZ(type, STORAGE_TYPE_CIFS))
    {
        result = storageDriverPosixInterface(
            storageDriverPosixNew(
                cfgOptionStr(cfgOptRepoPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write,
                storageRepoPathExpression));
    }
    else
        THROW_FMT(AssertError, "invalid storage type '%s'", strPtr(type));

    FUNCTION_TEST_RESULT(STORAGE, result);
}

/***********************************************************************************************************************************
Get a read-only repository storage object
***********************************************************************************************************************************/
const Storage *
storageRepo(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageRepo == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.walRegExp = regExpNew(strNew("^[0-F]{24}"));
            storageHelper.storageRepo = storageRepoGet(cfgOptionStr(cfgOptRepoType), false);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageHelper.storageRepo);
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
static String *
storageSpoolPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);

        FUNCTION_TEST_ASSERT(expression != NULL);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_IN))
    {
        if (path == NULL)
            result = strNewFmt("archive/%s/in", strPtr(storageHelper.stanza));
        else
            result = strNewFmt("archive/%s/in/%s", strPtr(storageHelper.stanza), strPtr(path));
    }
    else if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_OUT))
    {
        if (path == NULL)
            result = strNewFmt("archive/%s/out", strPtr(storageHelper.stanza));
        else
            result = strNewFmt("archive/%s/out/%s", strPtr(storageHelper.stanza), strPtr(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Get a read-only spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpool(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageSpool == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpool = storageDriverPosixInterface(
                storageDriverPosixNew(
                    cfgOptionStr(cfgOptSpoolPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false,
                    storageSpoolPathExpression));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageHelper.storageSpool);
}

/***********************************************************************************************************************************
Get a writable spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpoolWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageSpoolWrite == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpoolWrite = storageDriverPosixInterface(
                storageDriverPosixNew(
                    cfgOptionStr(cfgOptSpoolPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true,
                    storageSpoolPathExpression));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageHelper.storageSpoolWrite);
}
