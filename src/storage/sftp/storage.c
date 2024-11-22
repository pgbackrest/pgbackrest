/***********************************************************************************************************************************
SFTP Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/fd.h"
#include "common/io/socket/client.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/user.h"
#include "storage/posix/storage.h"
#include "storage/sftp/read.h"
#include "storage/sftp/storage.intern.h"
#include "storage/sftp/write.h"

/***********************************************************************************************************************************
Define PATH_MAX if it is not defined
***********************************************************************************************************************************/
#ifndef PATH_MAX
#define PATH_MAX                                                (4 * 1024)
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageSftp
{
    STORAGE_COMMON_MEMBER;

    IoSession *ioSession;                                           // IoSession (socket) connection to SFTP server
    LIBSSH2_SESSION *session;                                       // LibSsh2 session
    LIBSSH2_SFTP *sftpSession;                                      // LibSsh2 session sftp session
    LIBSSH2_SFTP_HANDLE *sftpHandle;                                // LibSsh2 session sftp handle
    TimeMSec timeout;                                               // Session timeout
};

/***********************************************************************************************************************************
Return known host key type based on host key type
***********************************************************************************************************************************/
static int
storageSftpKnownHostKeyType(const int hostKeyType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, hostKeyType);
    FUNCTION_TEST_END();

    int result;

    switch (hostKeyType)
    {
        case LIBSSH2_HOSTKEY_TYPE_RSA:
            result = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
            break;

        case LIBSSH2_HOSTKEY_TYPE_DSS:
            result = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
            break;

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
            result = LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
            break;
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_384
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
            result = LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
            break;
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_521
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
            result = LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
            break;
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519:
            result = LIBSSH2_KNOWNHOST_KEY_ED25519;
            break;
#endif

        default:
            result = 0;
            break;
    }

    FUNCTION_TEST_RETURN(INT, result);
}

/***********************************************************************************************************************************
Return error message based on error code
***********************************************************************************************************************************/
static const char *
libssh2SftpErrorMsg(const uint64_t error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, error);
    FUNCTION_TEST_END();

    const char *result;

    // SFTP error status codes (returned by libssh2_sftp_last_error()
    switch (error)
    {
        case LIBSSH2_FX_EOF:
            result = "eof";
            break;

        case LIBSSH2_FX_NO_SUCH_FILE:
            result = "no such file";
            break;

        case LIBSSH2_FX_PERMISSION_DENIED:
            result = "permission denied";
            break;

        case LIBSSH2_FX_FAILURE:
            result = "failure";
            break;

        case LIBSSH2_FX_BAD_MESSAGE:
            result = "bad message";
            break;

        case LIBSSH2_FX_NO_CONNECTION:
            result = "no connection";
            break;

        case LIBSSH2_FX_CONNECTION_LOST:
            result = "connection lost";
            break;

        case LIBSSH2_FX_OP_UNSUPPORTED:
            result = "operation unsupported";
            break;

        case LIBSSH2_FX_INVALID_HANDLE:
            result = "invalid handle";
            break;

        case LIBSSH2_FX_NO_SUCH_PATH:
            result = "no such path";
            break;

        case LIBSSH2_FX_FILE_ALREADY_EXISTS:
            result = "file already exists";
            break;

        case LIBSSH2_FX_WRITE_PROTECT:
            result = "write protect";
            break;

        case LIBSSH2_FX_NO_MEDIA:
            result = "no media";
            break;

        case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
            result = "no space on filesystem";
            break;

        case LIBSSH2_FX_QUOTA_EXCEEDED:
            result = "quota exceeded";
            break;

        case LIBSSH2_FX_UNKNOWN_PRINCIPAL:
            result = "unknown principal";
            break;

        case LIBSSH2_FX_LOCK_CONFLICT:
            result = "lock conflict";
            break;

        case LIBSSH2_FX_DIR_NOT_EMPTY:
            result = "directory not empty";
            break;

        case LIBSSH2_FX_NOT_A_DIRECTORY:
            result = "not a directory";
            break;

        case LIBSSH2_FX_INVALID_FILENAME:
            result = "invalid filename";
            break;

        case LIBSSH2_FX_LINK_LOOP:
            result = "link loop";
            break;

        default:
            result = "unknown error";
            break;
    }

    FUNCTION_TEST_RETURN_CONST(STRINGZ, result);
}

/***********************************************************************************************************************************
Return a match failed message based on known host check failure type
***********************************************************************************************************************************/
static const char *
storageSftpKnownHostCheckpFailureMsg(const int rc)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, rc);
    FUNCTION_TEST_END();

    const char *result;

    switch (rc)
    {
        case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
            result = "LIBSSH2_KNOWNHOST_CHECK_FAILURE";
            break;

        case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
            result = "not found in known hosts files: LIBSSH2_KNOWNHOST_CHECK_NOTFOUND";
            break;

        case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
            result = "mismatch in known hosts files: LIBSSH2_KNOWNHOST_CHECK_MISMATCH";
            break;

        default:
            result = "unknown failure";
            break;
    }

    FUNCTION_TEST_RETURN_CONST(STRINGZ, result);
}

/**********************************************************************************************************************************/
static String *
storageSftpLibSsh2SessionLastError(LIBSSH2_SESSION *const libSsh2Session)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, libSsh2Session);
    FUNCTION_TEST_END();

    String *result;
    char *libSsh2ErrMsg;
    int libSsh2ErrMsgLen;

    const int rc = libssh2_session_last_error(libSsh2Session, &libSsh2ErrMsg, &libSsh2ErrMsgLen, 0);

    if (libSsh2ErrMsgLen != 0)
        result = strNewZN(libSsh2ErrMsg, (size_t)libSsh2ErrMsgLen);
    else
        result = strNewFmt("libssh2 no session error message provided [%d]", rc);

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Rewrite the user's known_hosts file with a new entry
***********************************************************************************************************************************/
static void
storageSftpUpdateKnownHostsFile(
    StorageSftp *const this, const int hostKeyType, const String *const host, const char *const hostKey, const size_t hostKeyLen)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(INT, hostKeyType);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(STRINGZ, hostKey);
        FUNCTION_LOG_PARAM(SIZE, hostKeyLen);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Init a known host collection for the user's known_hosts file
        const char *const userKnownHostsFile = strZ(strNewFmt("%s%s", strZ(userHome()), "/.ssh/known_hosts"));
        LIBSSH2_KNOWNHOSTS *const userKnownHostsList = libssh2_knownhost_init(this->session);

        LOG_WARN_FMT("host '%s' not found in known hosts files, attempting to add host to '%s'", strZ(host), userKnownHostsFile);

        if (userKnownHostsList == NULL)
        {
            // Get the libssh2 error message and emit warning
            const int rc = libssh2_session_last_errno(this->session);
            LOG_WARN_FMT(
                "libssh2_knownhost_init failed for '%s' for update: libssh2 errno [%d] %s", userKnownHostsFile, rc,
                strZ(storageSftpLibSsh2SessionLastError(this->session)));
        }
        else
        {
            // Read the user's known_hosts file entries into the collection. libssh2_knownhost_readfile() returns the number of
            // successfully loaded hosts or a negative value on error, an empty known hosts file will return 0.
            int rc;

            if ((rc = libssh2_knownhost_readfile(userKnownHostsList, userKnownHostsFile, LIBSSH2_KNOWNHOST_FILE_OPENSSH)) < 0)
            {
                // Missing known_hosts file will return LIBSSH2_ERROR_FILE. Possibly issues other than missing may return this.
                if (rc == LIBSSH2_ERROR_FILE)
                {
                    // If user's known_hosts file is non-existent, create an empty one for libssh2 to operate on
                    const Storage *const sshStorage =
                        storagePosixNewP(
                            strNewFmt("%s%s", strZ(userHome()), "/.ssh"), .modeFile = 0600, .modePath = 0700, .write = true);

                    if (!storageExistsP(sshStorage, strNewFmt("%s", "known_hosts")))
                        storagePutP(storageNewWriteP(sshStorage, strNewFmt("%s", "known_hosts")), NULL);

                    // Try to load the user's known_hosts file entries into the collection again
                    rc = libssh2_knownhost_readfile(userKnownHostsList, userKnownHostsFile, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
                }
            }

            // If the user's known_hosts file was read successfully, add the host to the collection and rewrite the file
            if (rc >= 0)
            {
                // Check for a supported known host key type
                const int knownHostKeyType = storageSftpKnownHostKeyType(hostKeyType);

                if (knownHostKeyType != 0)
                {
                    // Add host to the internal list
                    if (libssh2_knownhost_addc(
                            userKnownHostsList, strZ(host), NULL, hostKey, hostKeyLen,
                            strZ(strNewZ("Generated from " PROJECT_NAME)), strlen(strZ(strNewZ("Generated from " PROJECT_NAME))),
                            LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | knownHostKeyType, NULL) == 0)
                    {
                        // Rewrite the updated known_hosts file
                        rc = libssh2_knownhost_writefile(userKnownHostsList, userKnownHostsFile, LIBSSH2_KNOWNHOST_FILE_OPENSSH);

                        if (rc != 0)
                            LOG_WARN_FMT(PROJECT_NAME " unable to write '%s' for update", userKnownHostsFile);
                        else
                            LOG_WARN_FMT(PROJECT_NAME " added new host '%s' to '%s'", strZ(host), userKnownHostsFile);
                    }
                    else
                        LOG_WARN_FMT(PROJECT_NAME " failed to add '%s' to known_hosts internal list", strZ(host));
                }
                else
                    LOG_WARN_FMT("unsupported key type [%d], unable to update knownhosts for '%s'", hostKeyType, strZ(host));
            }
            else
            {
                // On readfile failure warn that we're unable to update the user's known_hosts file
                LOG_WARN_FMT(
                    "libssh2 unable to read '%s' for update: libssh2 errno [%d] %s\n"
                    "HINT: does '%s' exist with proper permissions?", userKnownHostsFile, rc,
                    strZ(storageSftpLibSsh2SessionLastError(this->session)), userKnownHostsFile);
            }
        }

        // Free the user's known hosts list
        if (userKnownHostsList)
            libssh2_knownhost_free(userKnownHostsList);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Free libssh2 resources
***********************************************************************************************************************************/
static void
storageSftpLibSsh2SessionFreeResource(THIS_VOID)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    int rc;

    if (this->sftpHandle != NULL)
    {
        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (storageSftpWaitFd(this, rc));

        if (rc != 0)
        {
            if (rc != LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(
                    ServiceError, "failed to close sftpHandle: libssh2 errno [%d]%s", rc,
                    rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                        strZ(strNewFmt(": sftp errno [%lu]", libssh2_sftp_last_error(this->sftpSession))) : "");
            else
                THROW_FMT(
                    ServiceError, "timeout closing sftpHandle: libssh2 errno [%d]", rc);
        }
    }

    if (this->sftpSession != NULL)
    {
        do
        {
            rc = libssh2_sftp_shutdown(this->sftpSession);
        }
        while (storageSftpWaitFd(this, rc));

        if (rc != 0)
        {
            if (rc != LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(
                    ServiceError, "failed to shutdown sftpSession: libssh2 errno [%d]%s", rc,
                    rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                        strZ(strNewFmt(": sftp errno [%lu] %s", libssh2_sftp_last_error(this->sftpSession),
                                       libssh2SftpErrorMsg(libssh2_sftp_last_error(this->sftpSession)))) : "");
            else
                THROW_FMT(
                    ServiceError, "timeout shutting down sftpSession: libssh2 errno [%d]", rc);
        }
    }

    if (this->session != NULL)
    {
        do
        {
            rc = libssh2_session_disconnect_ex(this->session, SSH_DISCONNECT_BY_APPLICATION, PROJECT_NAME " instance shutdown", "");
        }
        while (storageSftpWaitFd(this, rc));

        if (rc != 0)
        {
            if (rc != LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(ServiceError, "failed to disconnect libssh2 session: libssh2 errno [%d]", rc);
            else
                THROW_FMT(ServiceError, "timeout disconnecting libssh2 session: libssh2 errno [%d]", rc);
        }

        do
        {
            rc = libssh2_session_free(this->session);
        }
        while (storageSftpWaitFd(this, rc));

        if (rc != 0)
        {
            if (rc != LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(ServiceError, "failed to free libssh2 session: libssh2 errno [%d]", rc);
            else
                THROW_FMT(ServiceError, "timeout freeing libssh2 session: libssh2 errno [%d]", rc);
        }
    }

    libssh2_exit();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN FN_NO_RETURN void
storageSftpEvalLibSsh2Error(
    const int ssh2Errno, const uint64_t sftpErrno, const ErrorType *const errorType, const String *const message,
    const String *const hint)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, ssh2Errno);
        FUNCTION_TEST_PARAM(UINT64, sftpErrno);
        FUNCTION_TEST_PARAM(ERROR_TYPE, errorType);
        FUNCTION_TEST_PARAM(STRING, message);
        FUNCTION_TEST_PARAM(STRING, hint);
    FUNCTION_TEST_END();

    ASSERT(errorType != NULL);

    THROWP_FMT(
        errorType, "%s%s%s%s", message != NULL ? zNewFmt("%s%s", strZ(message), ssh2Errno == 0 ? "" : ": ") : "",
        ssh2Errno == 0 ? "" : zNewFmt("libssh2 error [%d]", ssh2Errno),
        ssh2Errno == LIBSSH2_ERROR_SFTP_PROTOCOL ?
            zNewFmt(": sftp error [%" PRIu64 "] %s", sftpErrno, libssh2SftpErrorMsg(sftpErrno)) : "",
        hint != NULL ? zNewFmt("\n%s", strZ(hint)) : "");

    FUNCTION_TEST_NO_RETURN();
}

/***********************************************************************************************************************************
Call in a loop whenever a libssh2 call might return LIBSSH2_ERROR_EAGAIN. We handle checking the rc from the libssh2 call here and
will immediately exit out if it isn't LIBSSH2_ERROR_EAGAIN.

Note that LIBSSH2_ERROR_EAGAIN can still be set after this call -- if that happens then there was a timeout while waiting for the fd
to be ready.
***********************************************************************************************************************************/
FN_EXTERN bool
storageSftpWaitFd(StorageSftp *const this, const int64_t rc)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_SFTP, this);
        FUNCTION_TEST_PARAM(INT64, rc);
    FUNCTION_TEST_END();

    if (rc != LIBSSH2_ERROR_EAGAIN)
        FUNCTION_TEST_RETURN(BOOL, false);

    const int direction = libssh2_session_block_directions(this->session);
    const bool waitingRead = direction & LIBSSH2_SESSION_BLOCK_INBOUND;
    const bool waitingWrite = direction & LIBSSH2_SESSION_BLOCK_OUTBOUND;

    if (!waitingRead && !waitingWrite)
        FUNCTION_TEST_RETURN(BOOL, true);

    FUNCTION_TEST_RETURN(BOOL, fdReady(ioSessionFd(this->ioSession), waitingRead, waitingWrite, this->timeout));
}

/**********************************************************************************************************************************/
static bool
storageSftpLibSsh2FxNoSuchFile(THIS_VOID, const int rc)
{
    THIS(StorageSftp);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_SFTP, this);
        FUNCTION_TEST_PARAM(INT, rc);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(
        BOOL, rc == LIBSSH2_ERROR_SFTP_PROTOCOL && libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE);
}

/**********************************************************************************************************************************/
static StorageInfo
storageSftpInfo(THIS_VOID, const String *const file, const StorageInfoLevel level, const StorageInterfaceInfoParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {.level = level};

    // Stat the file to check if it exists
    LIBSSH2_SFTP_ATTRIBUTES attr;
    int rc;

    do
    {
        rc = libssh2_sftp_stat_ex(
            this->sftpSession, strZ(file), (unsigned int)strSize(file), param.followLink ? LIBSSH2_SFTP_STAT : LIBSSH2_SFTP_LSTAT,
            &attr);
    }
    while (storageSftpWaitFd(this, rc));

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileOpenError, "timeout opening '%s'", strZ(file));

        // Throw libssh2 on errors other than no such file
        if (!storageSftpLibSsh2FxNoSuchFile(this, rc))
        {
            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &FileOpenError, strNewFmt(STORAGE_ERROR_INFO, strZ(file)), NULL);
        }
    }
    // Else the file exists
    else
    {
        result.exists = true;

        // Add type info (no need set file type since it is the default)
        if (result.level >= storageInfoLevelType && !LIBSSH2_SFTP_S_ISREG(attr.permissions))
        {
            if (LIBSSH2_SFTP_S_ISDIR(attr.permissions))
                result.type = storageTypePath;
            else if (LIBSSH2_SFTP_S_ISLNK(attr.permissions))
                result.type = storageTypeLink;
            else
                result.type = storageTypeSpecial;
        }

        // Add basic level info
        if (result.level >= storageInfoLevelBasic)
        {
            if ((attr.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
                result.timeModified = (time_t)attr.mtime;

            if (result.type == storageTypeFile)
                if ((attr.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
                    result.size = (uint64_t)attr.filesize;
        }

        // Add detail level info
        if (result.level >= storageInfoLevelDetail)
        {
            if ((attr.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
            {
                result.groupId = (unsigned int)attr.gid;
                result.userId = (unsigned int)attr.uid;
            }

            if ((attr.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
                result.mode = attr.permissions & (LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO);

            if (result.type == storageTypeLink)
            {
                char linkDestination[PATH_MAX] = {0};
                ssize_t linkDestinationSize = 0;

                do
                {
                    linkDestinationSize = libssh2_sftp_symlink_ex(
                        this->sftpSession, strZ(file), (unsigned int)strSize(file), linkDestination, PATH_MAX - 1,
                        LIBSSH2_SFTP_READLINK);
                }
                while (storageSftpWaitFd(this, linkDestinationSize));

                if (linkDestinationSize == LIBSSH2_ERROR_EAGAIN)
                    THROW_FMT(FileReadError, "timeout getting destination for link '%s'", strZ(file));

                if (linkDestinationSize < 0)
                {
                    storageSftpEvalLibSsh2Error(
                        (int)linkDestinationSize, libssh2_sftp_last_error(this->sftpSession), &FileReadError,
                        strNewFmt("unable to get destination for link '%s'", strZ(file)), NULL);
                }

                result.linkDestination = strNewZN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Build known hosts file list. If knownHosts is empty build the default file list, otherwise build the list provided. knownHosts
requires full path and/or leading tilde path entries.
***********************************************************************************************************************************/
static StringList *
storageSftpKnownHostsFilesList(const StringList *const knownHosts)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_LIST, knownHosts);
    FUNCTION_LOG_END();

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strLstEmpty(knownHosts))
        {
            // Create default file list
            strLstAddFmt(result, "%s%s", strZ(userHome()), "/.ssh/known_hosts");
            strLstAddFmt(result, "%s%s", strZ(userHome()), "/.ssh/known_hosts2");
            strLstAddZ(result, "/etc/ssh/ssh_known_hosts");
            strLstAddZ(result, "/etc/ssh/ssh_known_hosts2");
        }
        else
        {
            // Process the known host list entries and add them to the result list
            for (unsigned int listIdx = 0; listIdx < strLstSize(knownHosts); listIdx++)
            {
                // Get the trimmed file path and add it to the result list
                const String *const filePath = strTrim(strLstGet(knownHosts, listIdx));

                if (strBeginsWithZ(filePath, "~/"))
                {
                    // Replace leading tilde with space, trim space, prepend user home path and add to the result list
                    strLstAddFmt(
                        result, "%s%s", strZ(userHome()), strZ(strTrim(strSub(filePath, (size_t)strChr(filePath, '~') + 1))));
                }
                else
                    strLstAdd(result, filePath);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
static String *
storageSftpExpandTildePath(const String *const tildePath)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, tildePath);
    FUNCTION_TEST_END();

    String *const result = strNew();

    // Append to user home directory path substring after the tilde
    MEM_CONTEXT_TEMP_BEGIN()
    {
        strCatFmt(result, "%s%s", strZ(userHome()), strZ(strSub(tildePath, (size_t)strChr(tildePath, '~') + 1)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
// Helper function to get info for a file if it exists. This logic can't live directly in storageSftpList() because there is a race
// condition where a file might exist while listing the directory but it is gone before stat() can be called. In order to get
// complete test coverage this function must be split out.
static void
storageSftpListEntry(
    StorageSftp *const this, StorageList *const list, const String *const path, const char *const name,
    const StorageInfoLevel level)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_SFTP, this);
        FUNCTION_TEST_PARAM(STORAGE_LIST, list);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRINGZ, name);
        FUNCTION_TEST_PARAM(ENUM, level);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(this != NULL);
    ASSERT(list != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);

    StorageInfo info = storageInterfaceInfoP(this, strNewFmt("%s/%s", strZ(path), name), level);

    if (info.exists)
    {
        info.name = STR(name);
        storageLstAdd(list, &info);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static StorageList *
storageSftpList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(param.targetTime == 0);

    StorageList *result = NULL;

    // Open the directory for read
    LIBSSH2_SFTP_HANDLE *sftpHandle;

    do
    {
        sftpHandle = libssh2_sftp_open_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), 0, 0, LIBSSH2_SFTP_OPENDIR);
    }
    while (sftpHandle == NULL && storageSftpWaitFd(this, libssh2_session_last_errno(this->session)));

    // If the directory could not be opened process errors and report missing directories
    if (sftpHandle == NULL)
    {
        const int rc = libssh2_session_last_errno(this->session);

        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileReadError, "timeout opening directory '%s'", strZ(path));

        // If sftpHandle == NULL is due to LIBSSH2_FX_NO_SUCH_FILE, do not throw error here, return NULL result
        if (!storageSftpLibSsh2FxNoSuchFile(this, rc))
        {
            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &PathOpenError, strNewFmt(STORAGE_ERROR_LIST_INFO, strZ(path)),
                NULL);
        }
    }
    else
    {
        // Directory was found
        result = storageLstNew(level);

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                LIBSSH2_SFTP_ATTRIBUTES attr;
                char filename[PATH_MAX] = {0};
                int len;

                // Read the directory entries
                do
                {
                    len = libssh2_sftp_readdir_ex(sftpHandle, filename, PATH_MAX - 1, NULL, 0, &attr);

                    if (len > 0)
                    {
                        filename[len] = '\0';

                        // Always skip . and ..
                        if (!strEqZ(DOT_STR, filename) && !strEqZ(DOTDOT_STR, filename))
                        {
                            if (level == storageInfoLevelExists)
                            {
                                const StorageInfo storageInfo =
                                {
                                    .name = STR(filename),
                                    .level = storageInfoLevelExists,
                                    .exists = true,
                                };

                                storageLstAdd(result, &storageInfo);
                            }
                            else
                                storageSftpListEntry(this, result, path, filename, level);
                        }

                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                }
                while (len > 0 || storageSftpWaitFd(this, len));
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            int rc;

            do
            {
                rc = libssh2_sftp_closedir(sftpHandle);
            }
            while (storageSftpWaitFd(this, rc));

            if (rc != 0)
            {
                if (rc != LIBSSH2_ERROR_EAGAIN)
                {
                    storageSftpEvalLibSsh2Error(
                        rc, libssh2_sftp_last_error(this->sftpSession), &PathCloseError,
                        strNewFmt("unable to close path '%s' after listing", strZ(path)), NULL);
                }
                else
                    THROW_FMT(PathCloseError, "timeout closing path '%s' after listing", strZ(path));
            }

            sftpHandle = NULL;
        }
        TRY_END();
    }

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
}

/**********************************************************************************************************************************/
static void
storageSftpRemove(THIS_VOID, const String *const file, const StorageInterfaceRemoveParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to unlink the file
    int rc;

    do
    {
        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
    }
    while (storageSftpWaitFd(this, rc));

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileRemoveError, "timeout removing '%s'", strZ(file));

        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            if (param.errorOnMissing || !storageSftpLibSsh2FxNoSuchFile(this, rc))
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
                    strNewFmt("unable to remove '%s'", strZ(file)), NULL);
            }
        }
        else
        {
            if (param.errorOnMissing)
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
                    strNewFmt("unable to remove '%s'", strZ(file)), NULL);
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageRead *
storageSftpNewRead(THIS_VOID, const String *const file, const bool ignoreMissing, const StorageInterfaceNewReadParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(!param.version);
    ASSERT(param.versionId == NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadSftpNew(
            this, file, ignoreMissing, this->session, this->sftpSession, this->sftpHandle, param.offset, param.limit));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageSftpNewWrite(THIS_VOID, const String *const file, const StorageInterfaceNewWriteParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.group);
        FUNCTION_LOG_PARAM(TIME, param.timeModified);
        FUNCTION_LOG_PARAM(BOOL, param.createPath);
        FUNCTION_LOG_PARAM(BOOL, param.syncFile);
        FUNCTION_LOG_PARAM(BOOL, param.syncPath);
        FUNCTION_LOG_PARAM(BOOL, param.atomic);
        FUNCTION_LOG_PARAM(BOOL, param.truncate);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(param.createPath);
    ASSERT(param.truncate);
    ASSERT(param.user == NULL);
    ASSERT(param.group == NULL);
    ASSERT(param.timeModified == 0);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteSftpNew(
            this, file, this->session, this->sftpSession, this->sftpHandle, param.modeFile, param.modePath, param.user, param.group,
            param.timeModified, param.createPath, param.syncFile, this->interface.pathSync != NULL ? param.syncPath : false,
            param.atomic, param.truncate));
}

/**********************************************************************************************************************************/
static void
storageSftpPathCreate(
    THIS_VOID, const String *const path, const bool errorOnExists, const bool noParentCreate, const mode_t mode,
    const StorageInterfacePathCreateParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    int rc;

    // Attempt to create the directory
    do
    {
        rc = libssh2_sftp_mkdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), (int)mode);
    }
    while (storageSftpWaitFd(this, rc));

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(PathCreateError, "timeout creating path '%s'", strZ(path));

        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            const uint64_t sftpErrno = libssh2_sftp_last_error(this->sftpSession);

            // libssh2 may return LIBSSH2_FX_FAILURE if the directory already exists
            if (sftpErrno == LIBSSH2_FX_FAILURE || sftpErrno == LIBSSH2_FX_FILE_ALREADY_EXISTS)
            {
                // Check if the directory already exists
                LIBSSH2_SFTP_ATTRIBUTES attr;

                do
                {
                    rc = libssh2_sftp_stat_ex(
                        this->sftpSession, strZ(path), (unsigned int)strSize(path), LIBSSH2_SFTP_STAT, &attr);
                }
                while (storageSftpWaitFd(this, rc));

                if (rc == LIBSSH2_ERROR_EAGAIN)
                    THROW_FMT(PathCreateError, "timeout stat'ing path '%s'", strZ(path));

                // If rc = 0 then already exists
                if (rc == 0 && errorOnExists)
                {
                    storageSftpEvalLibSsh2Error(
                        rc, libssh2_sftp_last_error(this->sftpSession), &PathCreateError,
                        strNewFmt("sftp error unable to create path '%s': path already exists", strZ(path)), NULL);
                }
            }
            // If the parent path does not exist then create it if allowed
            else if (sftpErrno == LIBSSH2_FX_NO_SUCH_FILE && !noParentCreate)
            {
                String *const pathParent = strPath(path);

                storageInterfacePathCreateP(this, pathParent, errorOnExists, noParentCreate, mode);
                storageInterfacePathCreateP(this, path, errorOnExists, noParentCreate, mode);

                strFree(pathParent);
            }
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, sftpErrno, &PathCreateError, strNewFmt("sftp error unable to create path '%s'", strZ(path)), NULL);
            }
        }
        else
        {
            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &PathCreateError,
                strNewFmt("ssh2 error [%d] unable to create path '%s'", rc, strZ(path)), NULL);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static bool
storageSftpPathRemove(THIS_VOID, const String *const path, const bool recurse, const StorageInterfacePathRemoveParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            const StorageList *const list = storageInterfaceListP(this, path, storageInfoLevelExists);

            if (list != NULL)
            {
                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
                    {
                        const String *const file = strNewFmt("%s/%s", strZ(path), strZ(storageLstGet(list, listIdx).name));

                        // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
                        int rc;

                        do
                        {
                            rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
                        }
                        while (storageSftpWaitFd(this, rc));

                        if (rc != 0)
                        {
                            if (rc == LIBSSH2_ERROR_EAGAIN)
                                THROW_FMT(PathRemoveError, "timeout removing file '%s'", strZ(file));

                            // Attempting to unlink a directory appears to return LIBSSH2_FX_FAILURE or LIBSSH2_FX_PERMISSION_DENIED
                            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
                            {
                                const uint64_t sftpErrno = libssh2_sftp_last_error(this->sftpSession);

                                if (sftpErrno == LIBSSH2_FX_FAILURE || sftpErrno == LIBSSH2_FX_PERMISSION_DENIED)
                                    storageInterfacePathRemoveP(this, file, true);
                                else
                                {
                                    THROW_FMT(
                                        PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE " libssh sftp [%" PRIu64 "] %s", strZ(file),
                                        sftpErrno, libssh2SftpErrorMsg(sftpErrno));
                                }
                            }
                            else
                                THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE " libssh ssh [%d]", strZ(file), rc);
                        }

                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                }
                MEM_CONTEXT_TEMP_END();
            }
        }

        // Delete the path
        int rc;

        do
        {
            rc = libssh2_sftp_rmdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path));
        }
        while (storageSftpWaitFd(this, rc));

        if (rc != 0)
        {
            if (rc == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(PathRemoveError, "timeout removing path '%s'", strZ(path));

            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
            {
                const uint64_t sftpErrno = libssh2_sftp_last_error(this->sftpSession);

                if (sftpErrno != LIBSSH2_FX_NO_SUCH_FILE)
                {
                    THROW_FMT(
                        PathRemoveError, STORAGE_ERROR_PATH_REMOVE " sftp error [%" PRIu64 "] %s", strZ(path), sftpErrno,
                        libssh2SftpErrorMsg(sftpErrno));
                }

                // Path does not exist
                result = false;
            }
            else
            {
                // Path does not exist
                result = false;

                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &PathRemoveError,
                    strNewFmt(STORAGE_ERROR_PATH_REMOVE, strZ(path)), NULL);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceSftp =
{
    .feature = 1 << storageFeaturePath | 1 << storageFeatureInfoDetail,

    .info = storageSftpInfo,
    .list = storageSftpList,
    .newRead = storageSftpNewRead,
    .newWrite = storageSftpNewWrite,
    .pathCreate = storageSftpPathCreate,
    .pathRemove = storageSftpPathRemove,
    .remove = storageSftpRemove,
};

FN_EXTERN Storage *
storageSftpNew(
    const String *const path, const String *const host, const unsigned int port, const String *const user,
    const TimeMSec timeout, const String *const keyPriv, const StringId hostKeyHashType, const StorageSftpNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(STRING, keyPriv);
        FUNCTION_LOG_PARAM(STRING_ID, hostKeyHashType);
        FUNCTION_LOG_PARAM(STRING, param.keyPub);
        FUNCTION_TEST_PARAM(STRING, param.keyPassphrase);
        FUNCTION_LOG_PARAM(STRING_ID, param.hostKeyCheckType);
        FUNCTION_LOG_PARAM(STRING, param.hostFingerprint);
        FUNCTION_LOG_PARAM(STRING_LIST, param.knownHosts);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(BOOL, param.write);
        FUNCTION_LOG_PARAM(FUNCTIONP, param.pathExpressionFunction);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(host != NULL);
    ASSERT(port != 0);
    ASSERT(user != NULL);
    ASSERT(keyPriv != NULL);
    ASSERT(hostKeyHashType != 0);
    // Initialize user module
    userInit();

    // Create the object
    OBJ_NEW_BEGIN(StorageSftp, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageSftp)
        {
            .interface = storageInterfaceSftp,
            .timeout = timeout,
        };

        // Init SFTP session
        if (libssh2_init(0) != 0)
            THROW_FMT(ServiceError, "unable to init libssh2");

        this->ioSession = ioClientOpen(sckClientNew(host, port, timeout, timeout));
        this->session = libssh2_session_init();

        if (this->session == NULL)
            THROW_FMT(ServiceError, "unable to init libssh2 session");

        // Set session to non-blocking
        libssh2_session_set_blocking(this->session, 0);

        // Perform handshake
        int rc;

        do
        {
            rc = libssh2_session_handshake(this->session, ioSessionFd(this->ioSession));
        }
        while (storageSftpWaitFd(this, rc));

        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(ServiceError, "timeout during libssh2 handshake [%d]", rc);

        if (rc != 0)
        {
            THROW_FMT(
                ServiceError, "libssh2 handshake failed [%d]: %s", rc, strZ(storageSftpLibSsh2SessionLastError(this->session)));
        }

        int hashType = LIBSSH2_HOSTKEY_HASH_SHA1;
        size_t hashSize = 0;

        // Verify that the fingerprint[N] buffer declared below is large enough when adding a new hashType
        switch (hostKeyHashType)
        {
            case hashTypeMd5:
                hashType = LIBSSH2_HOSTKEY_HASH_MD5;
                hashSize = HASH_TYPE_M5_SIZE;
                break;

            case hashTypeSha1:
                hashType = LIBSSH2_HOSTKEY_HASH_SHA1;
                hashSize = HASH_TYPE_SHA1_SIZE;
                break;

#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
            case hashTypeSha256:
                hashType = LIBSSH2_HOSTKEY_HASH_SHA256;
                hashSize = HASH_TYPE_SHA256_SIZE;
                break;
#endif // LIBSSH2_HOSTKEY_HASH_SHA256

            default:
                THROW_FMT(ServiceError, "requested ssh2 hostkey hash type (%s) not available", strZ(strIdToStr(hostKeyHashType)));
                break;
        }

        // Compare fingerprint if provided else check known hosts files for a match
        if (param.hostKeyCheckType == SFTP_STRICT_HOSTKEY_CHECKING_FINGERPRINT)
        {
            const char *const binaryFingerprint = libssh2_hostkey_hash(this->session, hashType);

            if (binaryFingerprint == NULL)
            {
                THROW_FMT(
                    ServiceError, "libssh2 hostkey hash failed: libssh2 errno [%d]", libssh2_session_last_errno(this->session));
            }

            // 256 bytes is large enough to hold the hex representation of currently supported hash types. The hex encoded version
            // requires twice as much space (hashSize * 2) as the raw version.
            char fingerprint[256];

            encodeToStr(encodingHex, (const unsigned char *)binaryFingerprint, hashSize, fingerprint);

            if (strcmp(fingerprint, strZ(param.hostFingerprint)) != 0)
            {
                THROW_FMT(
                    ServiceError, "host [%s] and configured fingerprint (repo-sftp-host-fingerprint) [%s] do not match",
                    fingerprint, strZ(param.hostFingerprint));
            }
        }
        else if (param.hostKeyCheckType != SFTP_STRICT_HOSTKEY_CHECKING_NONE)
        {
            // Init the known host collection
            LIBSSH2_KNOWNHOSTS *const knownHostsList = libssh2_knownhost_init(this->session);

            if (knownHostsList == NULL)
            {
                const int rc = libssh2_session_last_errno(this->session);

                THROW_FMT(
                    ServiceError,
                    "failure during libssh2_knownhost_init: libssh2 errno [%d] %s", rc,
                    strZ(storageSftpLibSsh2SessionLastError(this->session)));
            }

            // Get the list of known host files to search
            const StringList *const knownHostsPathList = storageSftpKnownHostsFilesList(param.knownHosts);

            // Loop through the list of known host files
            for (unsigned int listIdx = 0; listIdx < strLstSize(knownHostsPathList); listIdx++)
            {
                const char *const currentKnownHostFile = strZNull(strLstGet(knownHostsPathList, listIdx));

                // Read the known hosts file entries into the collection, log message for readfile status.
                // libssh2_knownhost_readfile() returns the number of successfully loaded hosts or a negative value on error, an
                // empty known hosts file will return 0.
                if ((rc = libssh2_knownhost_readfile(knownHostsList, currentKnownHostFile, LIBSSH2_KNOWNHOST_FILE_OPENSSH)) <= 0)
                {
                    if (rc == 0)
                        LOG_DETAIL_FMT("libssh2 '%s' file is empty", currentKnownHostFile);
                    else
                    {
                        LOG_DETAIL_FMT(
                            "libssh2 read '%s' failed: libssh2 errno [%d] %s", currentKnownHostFile, rc,
                            strZ(storageSftpLibSsh2SessionLastError(this->session)));
                    }
                }
                else
                    LOG_DETAIL_FMT("libssh2 read '%s' succeeded", currentKnownHostFile);
            }

            // Get the remote host key
            size_t hostKeyLen;
            int hostKeyType;
            const char *const hostKey = libssh2_session_hostkey(this->session, &hostKeyLen, &hostKeyType);

            // Check for a match in known hosts files else throw an error if no host key was retrieved
            if (hostKey != NULL)
            {
                rc = libssh2_knownhost_checkp(
                    knownHostsList, strZ(host), (int)port, hostKey, hostKeyLen,
                    LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, NULL);

                // Handle check success/failure
                if (rc == LIBSSH2_KNOWNHOST_CHECK_MATCH)
                    LOG_DETAIL_FMT("known hosts match found for '%s'", strZ(host));
                else
                {
                    // Handle failure to match in a similar manner as ssh_config StrictHostKeyChecking. If this flag is set to
                    // "strict", never automatically add host keys to the ~/.ssh/known_hosts file, and refuse to connect to hosts
                    // whose host key has changed. This option forces the user to manually add all new hosts. If this flag is set to
                    // "accept-new" then automatically add new host keys to the user known hosts files, but do not permit
                    // connections to hosts with changed host keys.
                    switch (param.hostKeyCheckType)
                    {
                        case SFTP_STRICT_HOSTKEY_CHECKING_STRICT:
                        {
                            // Throw an error when set to strict and we have any result other than match
                            libssh2_knownhost_free(knownHostsList);

                            THROW_FMT(
                                ServiceError, "known hosts failure: '%s' %s [%d]: check type [%s]", strZ(host),
                                storageSftpKnownHostCheckpFailureMsg(rc), rc, strZ(strIdToStr(param.hostKeyCheckType)));

                            break;
                        }

                        default:
                        {
                            ASSERT(param.hostKeyCheckType == SFTP_STRICT_HOSTKEY_CHECKING_ACCEPT_NEW);

                            // Throw an error when set to accept-new and match fails or mismatches else add the new host key to the
                            // user's known_hosts file
                            if (rc == LIBSSH2_KNOWNHOST_CHECK_MISMATCH || rc == LIBSSH2_KNOWNHOST_CHECK_FAILURE)
                            {
                                // Free the known hosts list
                                libssh2_knownhost_free(knownHostsList);

                                THROW_FMT(
                                    ServiceError, "known hosts failure: '%s': %s [%d]: check type [%s]", strZ(host),
                                    storageSftpKnownHostCheckpFailureMsg(rc), rc,
                                    strZ(strIdToStr(param.hostKeyCheckType)));
                            }
                            else
                                storageSftpUpdateKnownHostsFile(this, hostKeyType, host, hostKey, hostKeyLen);

                            break;
                        }
                    }
                }
            }
            else
            {
                THROW_FMT(
                    ServiceError,
                    "libssh2_session_hostkey failed to get hostkey: libssh2 error [%d]", libssh2_session_last_errno(this->session));
            }

            // Free the known hosts list
            libssh2_knownhost_free(knownHostsList);
        }

        // Perform public key authorization, expand leading tilde key file paths if needed
        String *const privKeyPath = regExpMatchOne(STRDEF("^ *~"), keyPriv) ? storageSftpExpandTildePath(keyPriv) : strDup(keyPriv);
        String *const pubKeyPath =
            param.keyPub != NULL && regExpMatchOne(STRDEF("^ *~"), param.keyPub) ?
                storageSftpExpandTildePath(param.keyPub) : strDup(param.keyPub);

        do
        {
            rc = libssh2_userauth_publickey_fromfile(
                this->session, strZ(user), strZNull(pubKeyPath), strZ(privKeyPath), strZNull(param.keyPassphrase));
        }
        while (storageSftpWaitFd(this, rc));

        strFree(privKeyPath);
        strFree(pubKeyPath);

        if (rc != 0)
        {
            if (rc == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(ServiceError, "timeout during public key authentication");

            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &ServiceError,
                STRDEF("public key authentication failed"),
                STRDEF(
                    "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
                    " --repo-sftp-public-key-file to be provided\n"
                    "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to"
                    " generate the keypair\n"
                    "HINT: check authorization log on the SFTP server"));
        }

        // Init the sftp session
        do
        {
            this->sftpSession = libssh2_sftp_init(this->session);
        }
        while (this->sftpSession == NULL && storageSftpWaitFd(this, libssh2_session_last_errno(this->session)));

        if (this->sftpSession == NULL)
        {
            if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(ServiceError, "timeout during init of libssh2_sftp session");
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &ServiceError,
                    strNewFmt("unable to init libssh2_sftp session"), NULL);
            }
        }

        // Ensure libssh2/libssh2_sftp resources freed
        memContextCallbackSet(objMemContext(this), storageSftpLibSsh2SessionFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageNew(
            STORAGE_SFTP_TYPE, path, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, 0, param.pathExpressionFunction,
            this, this->interface));
}

#endif // HAVE_LIBSSH2
