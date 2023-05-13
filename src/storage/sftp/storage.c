/***********************************************************************************************************************************
SFTP Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/socket/client.h"
#include "common/log.h"
#include "common/user.h"
#include "common/wait.h"
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
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc != 0)
        {
            THROW_FMT(
                ServiceError, "failed to free resource sftpHandle: libssh2 errno [%d]%s", rc,
                rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                    strZ(strNewFmt(": sftp errno [%lu]", libssh2_sftp_last_error(this->sftpSession))) : "");
        }
    }

    if (this->sftpSession != NULL)
    {
        do
        {
            rc = libssh2_sftp_shutdown(this->sftpSession);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc != 0)
        {
            THROW_FMT(
                ServiceError, "failed to free resource sftpSession: libssh2 errno [%d]%s", rc,
                rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                    strZ(strNewFmt(": sftp errno [%lu]", libssh2_sftp_last_error(this->sftpSession))) : "");
        }
    }

    if (this->session != NULL)
    {
        do
        {
            rc = libssh2_session_disconnect_ex(this->session, SSH_DISCONNECT_BY_APPLICATION, "pgbackrest instance shutdown", "");
        }
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc != 0)
            THROW_FMT(ServiceError, "failed to disconnect libssh2 session: libssh2 errno [%d]", rc);

        do
        {
            rc = libssh2_session_free(this->session);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc != 0)
            THROW_FMT(ServiceError, "failed to free libssh2 session: libssh2 errno [%d]", rc);
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
        errorType, "%slibssh2 error [%d]%s%s", message != NULL ? zNewFmt("%s: ", strZ(message)) : "", ssh2Errno,
        ssh2Errno == LIBSSH2_ERROR_SFTP_PROTOCOL ? zNewFmt(": sftp error [%" PRIu64 "]", sftpErrno) : "",
        hint != NULL ? zNewFmt("\n%s", strZ(hint)) : "");

    FUNCTION_TEST_NO_RETURN();
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
    Wait *const wait = waitNew(this->timeout);

    do
    {
        rc = libssh2_sftp_stat_ex(
            this->sftpSession, strZ(file), (unsigned int)strSize(file), param.followLink ? LIBSSH2_SFTP_STAT : LIBSSH2_SFTP_LSTAT,
            &attr);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

    waitFree(wait);

    // Throw libssh2 errors other than no such file
    if (rc != 0)
    {
        if (!storageSftpLibSsh2FxNoSuchFile(this, rc))
            THROW_FMT(FileOpenError, STORAGE_ERROR_INFO, strZ(file));
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

                Wait *const wait = waitNew(this->timeout);

                do
                {
                    linkDestinationSize = libssh2_sftp_symlink_ex(
                        this->sftpSession, strZ(file), (unsigned int)strSize(file), linkDestination, PATH_MAX - 1,
                        LIBSSH2_SFTP_READLINK);
                }
                while (linkDestinationSize == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

                waitFree(wait);

                if (linkDestinationSize < 0)
                    THROW_FMT(FileReadError, "unable to get destination for link '%s'", strZ(file));

                result.linkDestination = strNewZN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
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

    StorageList *result = NULL;

    // Open the directory for read
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    Wait *const wait = waitNew(this->timeout);

    do
    {
        sftpHandle = libssh2_sftp_open_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), 0, 0, LIBSSH2_SFTP_OPENDIR);
    }
    while (sftpHandle == NULL && libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

    waitFree(wait);

    // If the directory could not be opened process errors and report missing directories
    if (sftpHandle == NULL)
    {
        const int rc = libssh2_session_last_errno(this->session);

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

                Wait *wait = waitNew(this->timeout);

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

                        // Reset the timeout so we don't timeout before reading all entries
                        waitFree(wait);
                        wait = waitNew(this->timeout);
                    }
                }
                while (len > 0 || (len == LIBSSH2_ERROR_EAGAIN && waitMore(wait)));

                waitFree(wait);
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            int rc;
            Wait *const wait = waitNew(this->timeout);

            do
            {
                rc = libssh2_sftp_closedir(sftpHandle);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

            waitFree(wait);

            if (rc != 0)
                THROW_FMT(PathCloseError, "unable to close path '%s' after listing", strZ(path));

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
    Wait *const wait = waitNew(this->timeout);

    do
    {
        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

    if (rc != 0)
    {
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
                THROW_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
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

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadSftpNew(
            this, file, ignoreMissing, this->ioSession, this->session, this->sftpSession, this->sftpHandle, this->timeout,
            param.offset, param.limit));
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
            this, file, this->ioSession, this->session, this->sftpSession, this->sftpHandle, this->timeout, param.modeFile,
            param.modePath, param.user, param.group, param.timeModified, param.createPath, param.syncFile,
            this->interface.pathSync != NULL ? param.syncPath : false, param.atomic, param.truncate));
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
    Wait *const wait = waitNew(this->timeout);

    // Attempt to create the directory
    do
    {
        rc = libssh2_sftp_mkdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), (int)mode);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

    waitFree(wait);

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            uint64_t sftpErrno = libssh2_sftp_last_error(this->sftpSession);

            // libssh2 may return LIBSSH2_FX_FAILURE if the directory already exists
            if (sftpErrno == LIBSSH2_FX_FAILURE)
            {
                // Check if the directory already exists
                LIBSSH2_SFTP_ATTRIBUTES attr;

                Wait *const wait = waitNew(this->timeout);

                do
                {
                    rc = libssh2_sftp_stat_ex(
                        this->sftpSession, strZ(path), (unsigned int)strSize(path), LIBSSH2_SFTP_STAT, &attr);
                }
                while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

                waitFree(wait);

                // If rc = 0 then already exists
                if (rc == 0 && errorOnExists)
                    THROW_FMT(PathCreateError, "unable to create path '%s': path already exists", strZ(path));
            }
            // If the parent path does not exist then create it if allowed
            else if (sftpErrno == LIBSSH2_FX_NO_SUCH_FILE && !noParentCreate)
            {
                String *const pathParent = strPath(path);

                storageInterfacePathCreateP(this, pathParent, errorOnExists, noParentCreate, mode);
                storageInterfacePathCreateP(this, path, errorOnExists, noParentCreate, mode);

                strFree(pathParent);
            }
            else if (sftpErrno != LIBSSH2_FX_FILE_ALREADY_EXISTS || errorOnExists)
                THROW_FMT(PathCreateError, "sftp error unable to create path '%s'", strZ(path));
        }
        else
            THROW_FMT(PathCreateError, "ssh2 error [%d] unable to create path '%s'", rc, strZ(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

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
            StorageList *const list = storageInterfaceListP(this, path, storageInfoLevelExists);

            if (list != NULL)
            {
                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
                    {
                        const String *const file = strNewFmt("%s/%s", strZ(path), strZ(storageLstGet(list, listIdx).name));

                        // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
                        int rc;
                        Wait *const wait = waitNew(this->timeout);

                        do
                        {
                            rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
                        }
                        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

                        waitFree(wait);

                        if (rc != 0)
                        {
                            // Attempting to unlink a directory appears to return LIBSSH2_FX_FAILURE
                            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL &&
                                libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_FAILURE)
                            {
                                storageInterfacePathRemoveP(this, file, true);
                            }
                            else
                                THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strZ(file));
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
        Wait *const wait = waitNew(this->timeout);

        do
        {
            rc = libssh2_sftp_rmdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path));
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

        waitFree(wait);

        if (rc != 0)
        {
            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
            {
                if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_NO_SUCH_FILE)
                    THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));

                // Path does not exist
                result = false;
            }
            else
            {
                // Path does not exist
                result = false;

                THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));
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
        FUNCTION_LOG_PARAM(STRING, param.hostFingerprint);
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

        if (libssh2_init(0) != 0)
            THROW_FMT(ServiceError, "unable to init libssh2");

        this->ioSession = ioClientOpen(sckClientNew(host, port, timeout, timeout));

        this->session = libssh2_session_init();
        if (this->session == NULL)
            THROW_FMT(ServiceError, "unable to init libssh2 session");

        // Returns void
        libssh2_session_set_blocking(this->session, 0);

        int handshakeStatus = 0;
        Wait *wait = waitNew(timeout);

        do
        {
            handshakeStatus = libssh2_session_handshake(this->session, ioSessionFd(this->ioSession));
        }
        while (handshakeStatus == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

        waitFree(wait);

        if (handshakeStatus != 0)
            THROW_FMT(ServiceError, "libssh2 handshake failed [%d]", handshakeStatus);

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
                THROW_FMT(
                    ServiceError, "requested ssh2 hostkey hash type (%s) not available", strZ(strIdToStr(hostKeyHashType)));
                break;
        }

        const char *binaryFingerprint = libssh2_hostkey_hash(this->session, hashType);

        if (binaryFingerprint == NULL)
            THROW_FMT(ServiceError, "libssh2 hostkey hash failed: libssh2 errno [%d]", libssh2_session_last_errno(this->session));

        // Compare fingerprint if provided
        if (param.hostFingerprint != NULL)
        {
            // 256 bytes is large enough to hold the hex representation of currently supported hash types. The hex encoded version
            // requires twice as much space (hashSize * 2) as the raw version.
            char fingerprint[256];

            encodeToStr(encodingHex, (unsigned char *)binaryFingerprint, hashSize, fingerprint);

            if (strcmp(fingerprint, strZ(param.hostFingerprint)) != 0)
            {
                THROW_FMT(
                    ServiceError, "host [%s] and configured fingerprint (repo-sftp-host-fingerprint) [%s] do not match",
                    fingerprint, strZ(param.hostFingerprint));
            }
        }

        LOG_DEBUG_FMT("attempting public key authentication");

        int rc;
        wait = waitNew(timeout);

        do
        {
            rc = libssh2_userauth_publickey_fromfile(
                this->session, strZ(user), strZNull(param.keyPub), strZ(keyPriv), strZNull(param.keyPassphrase));
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

        waitFree(wait);

        if (rc != 0)
        {
            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &ServiceError,
                STRDEF("public key authentication failed"),
                STRDEF(
                    "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
                    " --repo-sftp-public-key-file to be provided\n"
                    "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to"
                    " generate the keypair"));
        }

        wait = waitNew(timeout);

        do
        {
            this->sftpSession = libssh2_sftp_init(this->session);
        }
        while (this->sftpSession == NULL && waitMore(wait));

        waitFree(wait);

        if (this->sftpSession == NULL)
            THROW_FMT(ServiceError, "unable to init libssh2_sftp session");

        // Ensure libssh2/libssh2_sftp resources freed
        memContextCallbackSet(objMemContext(this), storageSftpLibSsh2SessionFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageNew(
            STORAGE_SFTP_TYPE, path, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, param.pathExpressionFunction,
            this, this->interface));
}

#endif // HAVE_LIBSSH2
