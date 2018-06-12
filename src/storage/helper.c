/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Mem context for storage helper
***********************************************************************************************************************************/
static MemContext *memContextStorageHelper = NULL;

/***********************************************************************************************************************************
Cache for local storage
***********************************************************************************************************************************/
static Storage *storageLocalData = NULL;
static Storage *storageLocalWriteData = NULL;

/***********************************************************************************************************************************
Cache for spool storage
***********************************************************************************************************************************/
static Storage *storageSpoolData = NULL;
static const String *storageSpoolStanza = NULL;

/***********************************************************************************************************************************
Create the storage helper memory context
***********************************************************************************************************************************/
static void
storageHelperInit()
{
    FUNCTION_TEST_VOID();

    if (memContextStorageHelper == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            memContextStorageHelper = memContextNew("storageHelper");
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Get a local storage object
***********************************************************************************************************************************/
const Storage *
storageLocal()
{
    FUNCTION_TEST_VOID();

    if (storageLocalData == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(memContextStorageHelper)
        {
            storageLocalData = storageNewNP(strNew("/"));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageLocalData);
}

/***********************************************************************************************************************************
Get a writable local storage object

This should be used very sparingly.  If writes are not needed then always use storageLocal() or a specific storage object instead.
***********************************************************************************************************************************/
const Storage *
storageLocalWrite()
{
    FUNCTION_TEST_VOID();

    if (storageLocalWriteData == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(memContextStorageHelper)
        {
            storageLocalWriteData = storageNewP(strNew("/"), .write = true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageLocalWriteData);
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
String *
storageSpoolPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);

        FUNCTION_TEST_ASSERT(expression != NULL);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (strcmp(strPtr(expression), STORAGE_SPOOL_ARCHIVE_IN) == 0)
    {
        if (path == NULL)
            result = strNewFmt("archive/%s/in", strPtr(storageSpoolStanza));
        else
            result = strNewFmt("archive/%s/in/%s", strPtr(storageSpoolStanza), strPtr(path));
    }
    else if (strcmp(strPtr(expression), STORAGE_SPOOL_ARCHIVE_OUT) == 0)
    {
        if (path == NULL)
            result = strNewFmt("archive/%s/out", strPtr(storageSpoolStanza));
        else
            result = strNewFmt("archive/%s/out/%s", strPtr(storageSpoolStanza), strPtr(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpool()
{
    FUNCTION_TEST_VOID();

    if (storageSpoolData == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(memContextStorageHelper)
        {
            storageSpoolStanza = strDup(cfgOptionStr(cfgOptStanza));
            storageSpoolData = storageNewP(
                cfgOptionStr(cfgOptSpoolPath), .bufferSize = (size_t)cfgOptionInt(cfgOptBufferSize),
                .pathExpressionFunction = storageSpoolPathExpression, .write = true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STORAGE, storageSpoolData);
}
