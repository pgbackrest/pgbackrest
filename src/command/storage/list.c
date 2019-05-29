/***********************************************************************************************************************************
Storage List Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/memContext.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/string.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Render storage list as a string
***********************************************************************************************************************************/
static String *
storageListRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Get the path if passed
    const String *path = NULL;

    if (strLstSize(cfgCommandParam()) == 1)
        path = strLstGet(cfgCommandParam(), 0);

    // Output the list
    String *result = strNew("");
    StringList *list = storageListP(storageRepo(), path, .nullOnMissing = true, .expression = cfgOptionStr(cfgOptFilter));

    if (list != NULL)
    {
        strLstSort(list, strEqZ(cfgOptionStr(cfgOptSort), "asc") ? sortOrderAsc : sortOrderDesc);

        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
        {
            strCat(result, strPtr(strLstGet(list, listIdx)));
            strCat(result, "\n");
        }
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Render storage list and output to stdout
***********************************************************************************************************************************/
void
cmdStorageList(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioHandleWriteOneStr(STDOUT_FILENO, storageListRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
