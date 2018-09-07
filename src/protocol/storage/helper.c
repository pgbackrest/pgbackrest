/***********************************************************************************************************************************
Protocol Storage Helper
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.h"
#include "protocol/storage/helper.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
struct
{
    MemContext *memContext;                                         // Mem context for protocol storage
    Storage *storageRepo;                                           // Repository read-only storage
    String *stanza;                                                 // Stanza for storage
    RegExp *walRegExp;                                              // Regular expression for identifying wal files
} protocolStorageHelper;

/***********************************************************************************************************************************
Create the storage helper memory context
***********************************************************************************************************************************/
static void
protocolStorageHelperInit(void)
{
    FUNCTION_TEST_VOID();

    if (protocolStorageHelper.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            protocolStorageHelper.memContext = memContextNew("protocolStorageHelper");
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT_VOID();
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
        result = strNewFmt("archive/%s", strPtr(protocolStorageHelper.stanza));

        if (path != NULL)
        {
            StringList *pathSplit = strLstNewSplitZ(path, "/");
            String *file = strLstSize(pathSplit) == 2 ? strLstGet(pathSplit, 1) : NULL;

            if (file != NULL && regExpMatch(protocolStorageHelper.walRegExp, file))
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
Get a read-only repository storage object
***********************************************************************************************************************************/
const Storage *
storageRepo(void)
{
    FUNCTION_TEST_VOID();

    if (protocolStorageHelper.storageRepo == NULL)
    {
        protocolStorageHelperInit();

        MEM_CONTEXT_BEGIN(protocolStorageHelper.memContext)
        {
            protocolStorageHelper.stanza = strDup(cfgOptionStr(cfgOptStanza));
            protocolStorageHelper.walRegExp = regExpNew(strNew("^[0-F]{24}"));
            protocolStorageHelper.storageRepo = storageNewP(
                cfgOptionStr(cfgOptRepoPath), .pathExpressionFunction = storageRepoPathExpression);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, protocolStorageHelper.storageRepo);
}
