/***********************************************************************************************************************************
Posix Direct I/O Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_DIO_STORAGE_HELPER_H
#define STORAGE_POSIX_DIO_STORAGE_HELPER_H

#include "storage/posixDio/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN Storage *storagePosixDioHelper(unsigned int repoIdx, bool write, StoragePathExpressionCallback pathExpressionCallback);

/***********************************************************************************************************************************
Storage helper for StorageHelper array passed to storageHelperInit()
***********************************************************************************************************************************/
#define STORAGE_POSIX_DIO_HELPER                                   {.type = STORAGE_POSIX_DIO_TYPE, .helper = storagePosixDioHelper}

#endif
