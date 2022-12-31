/***********************************************************************************************************************************
Storage Helper Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessStorageHelper.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/**********************************************************************************************************************************/
void
hrnStorageHelperFree(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext != NULL)
        memContextFree(storageHelper.memContext);

    storageHelper = (struct StorageHelperLocal){.memContext = NULL, .helperList = storageHelper.helperList};

    FUNCTION_TEST_RETURN_VOID();
}
