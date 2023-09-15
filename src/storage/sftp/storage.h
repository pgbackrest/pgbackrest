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
#define SFTP_STRICT_HOSTKEY_CHECKING_ACCEPT_NEW                     STRID5("accept-new", 0x2e576e9028c610)
#define SFTP_STRICT_HOSTKEY_CHECKING_FINGERPRINT                    STRID5("fingerprint", 0x51c9942453b9260)
#define SFTP_STRICT_HOSTKEY_CHECKING_NONE                           STRID5("none", 0x2b9ee0)
#define SFTP_STRICT_HOSTKEY_CHECKING_STRICT                         STRID5("strict", 0x2834ca930)

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
    StringId hostKeyCheckType;
    const String *hostFingerprint;
    const StringList *knownHosts;
} StorageSftpNewParam;

#define storageSftpNewP(path, host, port, user, timeout, keyPriv, hostKeyHashType, ...)                                            \
    storageSftpNew(path, host, port, user, timeout, keyPriv, hostKeyHashType, (StorageSftpNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN Storage *storageSftpNew(
    const String *path, const String *host, unsigned int port, const String *user, TimeMSec timeout, const String *keyPriv,
    StringId hostKeyHashType, const StorageSftpNewParam param);

#endif // HAVE_LIBSSH2

#endif
