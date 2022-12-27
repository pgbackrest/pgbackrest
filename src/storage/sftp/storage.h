/***********************************************************************************************************************************
SFTP Storage
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_H
#define STORAGE_SFTP_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageSftp StorageSftp;

#ifdef HAVE_LIBSSH2
#include <libssh2.h>
#include <libssh2_sftp.h>
#endif
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_SFTP_TYPE                                           STRID5("sftp", 0x850d30)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageSftpNewParam
{
    VAR_PARAM_HEADER;
    bool write;
    mode_t modeFile;
    mode_t modePath;
    StoragePathExpressionCallback *pathExpressionFunction;
    const String *user;
    const String *keyPub;
    const String *keyPriv;
    const String *keyPassphrase;
    StringId hostkeyHash;
} StorageSftpNewParam;

#define storageSftpNewP(path, host, port, timeoutConnect, timeoutSession, ...)                                                     \
    storageSftpNew(path, host, port, timeoutConnect, timeoutSession, (StorageSftpNewParam){VAR_PARAM_INIT, __VA_ARGS__})

Storage *storageSftpNew(
    const String *path, const String *host, unsigned int port, TimeMSec timeoutConnect, TimeMSec timeoutSession,
    const StorageSftpNewParam param);

#endif
