/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#include "string.h"

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
    if (memContextStorageHelper == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            memContextStorageHelper = memContextNew("storageHelper");
        }
        MEM_CONTEXT_END();
    }
}

/***********************************************************************************************************************************
Get a local storage object
***********************************************************************************************************************************/
const Storage *
storageLocal()
{
    storageHelperInit();

    if (storageLocalData == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextStorageHelper)
        {
            storageLocalData = storageNew(strNew("/"), STORAGE_PATH_MODE_DEFAULT, 65536, NULL);
        }
        MEM_CONTEXT_END();
    }

    return storageLocalData;
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
String *
storageSpoolPathExpression(const String *expression, const String *path)
{
    String *result = NULL;

    if (strcmp(strPtr(expression), STORAGE_SPOOL_ARCHIVE_OUT) == 0)
    {
        if (path == NULL)
            result = strNewFmt("archive/%s/out", strPtr(storageSpoolStanza));
        else
            result = strNewFmt("archive/%s/out/%s", strPtr(storageSpoolStanza), strPtr(path));
    }
    else
        THROW(AssertError, "invalid expression '%s'", strPtr(expression));

    return result;
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpool()
{
    if (storageSpoolData == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextStorageHelper)
        {
            storageSpoolStanza = strDup(cfgOptionStr(cfgOptStanza));
            storageSpoolData = storageNew(
                cfgOptionStr(cfgOptSpoolPath), STORAGE_PATH_MODE_DEFAULT, cfgOptionInt(cfgOptBufferSize),
                (StoragePathExpressionCallback)storageSpoolPathExpression);
        }
        MEM_CONTEXT_END();
    }

    return storageSpoolData;
}
