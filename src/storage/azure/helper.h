/***********************************************************************************************************************************
Azure Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_HELPER_H
#define STORAGE_AZURE_STORAGE_HELPER_H

#include "storage/azure/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN Storage *storageAzureHelper(unsigned int repoIdx, bool write, StoragePathExpressionCallback pathExpressionCallback);

/***********************************************************************************************************************************
Storage helper for StorageHelper array passed to storageHelperInit()
***********************************************************************************************************************************/
#define STORAGE_AZURE_HELPER                                        {.type = STORAGE_AZURE_TYPE, .helper = storageAzureHelper}

#endif
