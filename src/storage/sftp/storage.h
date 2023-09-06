/***********************************************************************************************************************************
SFTP Storage
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_H
#define STORAGE_SFTP_STORAGE_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_SFTP_TYPE                                           STRID5("sftp", 0x850d30)

#ifdef HAVE_LIBSSH2

/***********************************************************************************************************************************
SFTP StrictHostKeyChecking
***********************************************************************************************************************************/
#define SFTP_STRICT_HOSTKEY_CHECKING_YES                            STRID5("yes", 0x4cb90)
#define SFTP_STRICT_HOSTKEY_CHECKING_NO                             STRID5("no", 0x1ee0)
#define SFTP_STRICT_HOSTKEY_CHECKING_OFF                            STRID5("off", 0x18cf0)
#define SFTP_STRICT_HOSTKEY_CHECKING_ACCEPT_NEW                     STRID5("accept-new", 0x2e576e9028c610)

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
    const String *keyPub;
    const String *keyPassphrase;
    const String *hostFingerprint;
    StringId sftpStrictHostKeyChecking;
    const VariantList *sftpKnownHosts;
} StorageSftpNewParam;

#define storageSftpNewP(path, host, port, user, timeout, keyPriv, hostKeyHashType, ...)                                            \
    storageSftpNew(path, host, port, user, timeout, keyPriv, hostKeyHashType, (StorageSftpNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN Storage *storageSftpNew(
    const String *path, const String *host, unsigned int port, const String *user, TimeMSec timeout, const String *keyPriv,
    StringId hostKeyHashType, const StorageSftpNewParam param);

#endif // HAVE_LIBSSH2

#endif
