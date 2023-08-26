/***********************************************************************************************************************************
SFTP Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_HELPER_H
#define STORAGE_SFTP_STORAGE_HELPER_H

#ifdef HAVE_LIBSSH2

#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN Storage *storageSftpHelper(unsigned int repoIdx, bool write, StoragePathExpressionCallback pathExpressionCallback);

/***********************************************************************************************************************************
Storage helper for StorageHelper array passed to storageHelperInit()
***********************************************************************************************************************************/
#define STORAGE_SFTP_HELPER                                         {.type = STORAGE_SFTP_TYPE, .helper = storageSftpHelper}

#endif // HAVE_LIBSSH2

#endif
