/***********************************************************************************************************************************
SFTP Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/io/socket/client.h"
#include "common/log.h"
#include "common/user.h"
#include "common/wait.h"
#include "storage/sftp/read.h"
#include "storage/sftp/storage.h"
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

    int libssh2_initStatus;
    int handshakeStatus;
    IoSession *ioSession;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    TimeMSec timeoutConnect;
    TimeMSec timeoutSession;
    Wait *wait;
};

/**********************************************************************************************************************************/
static bool
storageSftpLibSsh2FxNoSuchFile(THIS_VOID, const int rc)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(INT, rc);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
            result = true;

    FUNCTION_LOG_RETURN(BOOL, result);
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

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {.level = level};

    // Stat the file to check if it exists
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    if (param.followLink)
    {
        do
        {
            rc = libssh2_sftp_stat_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file), LIBSSH2_SFTP_STAT, &attrs);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
    }
    else
    {
        do
        {
            rc = libssh2_sftp_stat_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file), LIBSSH2_SFTP_LSTAT, &attrs);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
    }

    if (rc)
    {
        // Mimics posix driver - throw on libssh2 errors other than no such file
        if (!storageSftpLibSsh2FxNoSuchFile(this, rc))
            THROW_FMT(FileOpenError, STORAGE_ERROR_INFO, strZ(file));
    }
    // On success the file exists
    else
    {
        result.exists = true;

        // Add type info (no need set file type since it is the default)
        if (result.level >= storageInfoLevelType && !LIBSSH2_SFTP_S_ISREG(attrs.permissions))
        {
            if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions))
                result.type = storageTypePath;
            else if (LIBSSH2_SFTP_S_ISLNK(attrs.permissions))
                result.type = storageTypeLink;
            else
                result.type = storageTypeSpecial;
        }

        // Add basic level info
        if (result.level >= storageInfoLevelBasic)
        {
            if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
                result.timeModified = (time_t)attrs.mtime;

            if (result.type == storageTypeFile)
                if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
                    result.size = (uint64_t)attrs.filesize;
        }

        // Add detail level info
        if (result.level >= storageInfoLevelDetail)
        {
            if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
            {
                result.groupId = (unsigned int)attrs.gid;
                result.group = groupNameFromId(result.groupId);
                result.userId = (unsigned int)attrs.uid;
                result.user = userNameFromId(result.userId);
            }

            if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
                result.mode = attrs.permissions & (LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO);

            if (result.type == storageTypeLink)
            {
                char linkDestination[PATH_MAX] = {};
                ssize_t linkDestinationSize = 0;

                this->wait = waitNew(this->timeoutConnect);

                do
                {
                    linkDestinationSize = libssh2_sftp_symlink_ex(
                        this->sftpSession, strZ(file), (unsigned int)strSize(file), linkDestination, PATH_MAX - 1,
                        LIBSSH2_SFTP_READLINK);
                }
                while (linkDestinationSize == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

                if (linkDestinationSize < 0)
                    THROW_FMT(FileReadError, "unable to get destination for link '%s'", strZ(file));

                result.linkDestination = strNewZN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
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

    int rc = 0;

    if (this->sftpHandle != NULL)
    {
        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc)
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

        if (rc)
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

        if (rc)
            THROW_FMT(ServiceError, "failed to disconnect libssh2 session: libssh2 errno [%d]", rc);

        do
        {
            rc = libssh2_session_free(this->session);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN);

        if (rc)
            THROW_FMT(ServiceError, "failed to free libssh2 session: libssh2 errno [%d]", rc);
    }

    libssh2_exit();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
storageSftpLinkCreate(
    THIS_VOID, const String *const target, const String *const linkPath, const StorageInterfaceLinkCreateParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, target);
        FUNCTION_LOG_PARAM(STRING, linkPath);
        FUNCTION_LOG_PARAM(ENUM, param.linkType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(target != NULL);
    ASSERT(linkPath != NULL);

    // Create symlink
    if (param.linkType == storageLinkSym)
    {
        ssize_t rc = 0;
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_symlink_ex(
                this->sftpSession, strZ(target), (unsigned int)strSize(target), (char *)strZ(linkPath),
                (unsigned int)strSize(linkPath), LIBSSH2_SFTP_SYMLINK);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        if (rc != 0)
            THROW_FMT(FileOpenError, "unable to create symlink '%s' to '%s'", strZ(linkPath), strZ(target));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageSftpEvalLibSsh2Error(
    const int ssh2Errno, const uint64_t sftpErrno, const ErrorType *const errorType, const String *const msg,
    const String *const hint)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, ssh2Errno);
        FUNCTION_LOG_PARAM(UINT64, sftpErrno);
        FUNCTION_LOG_PARAM(ERROR_TYPE, errorType);
        FUNCTION_LOG_PARAM(STRING, msg);
        FUNCTION_LOG_PARAM(STRING, hint);
    FUNCTION_LOG_END();

    ASSERT(errorType != NULL);

    THROWP_FMT(
        errorType,
        "%slibssh2 error [%d]%s%s",
        msg != NULL ? strZ(strNewFmt("%s: ", strZ(msg))) : "",
        ssh2Errno,
        ssh2Errno == LIBSSH2_ERROR_SFTP_PROTOCOL ?
            strZ(strNewFmt(": sftp error [%" PRIu64 "]", sftpErrno)) :
            "",
        hint != NULL ? strZ(strNewFmt("\n%s", strZ(hint))) : "");

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper function to get info for a file if it exists.  This logic can't live directly in storageSftpList() because there is a race
// condition where a file might exist while listing the directory but it is gone before stat() can be called.  In order to get
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

    this->wait = waitNew(this->timeoutConnect);

    do
    {
        sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(path), (unsigned int)strSize(path), 0, 0, LIBSSH2_SFTP_OPENDIR);
    }
    while (sftpHandle == NULL && libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    // If the directory could not be opened process errors and report missing directories
    if (sftpHandle == NULL)
    {
        // If sftpHandle == NULL is due to LIBSSH2_FX_NO_SUCH_FILE, do not throw error here, return NULL result
        if (!storageSftpLibSsh2FxNoSuchFile(this, libssh2_session_last_errno(this->session)))
        {
            storageSftpEvalLibSsh2Error(
                libssh2_session_last_errno(this->session), libssh2_sftp_last_error(this->sftpSession), &PathOpenError,
                strNewFmt(STORAGE_ERROR_LIST_INFO, strZ(path)), NULL);
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
                LIBSSH2_SFTP_ATTRIBUTES attrs;
                char filename[PATH_MAX] = {};
                int len = 0;

                this->wait = waitNew(this->timeoutConnect);

                // read the directory entries
                do
                {
                    len = libssh2_sftp_readdir_ex(sftpHandle, filename, PATH_MAX - 1, NULL, 0, &attrs);

                    if (len > 0)
                    {
                        filename[len] = '\0';

                        // Always skip . and ..
                        if (!strEqZ(DOT_STR, filename) && !strEqZ(DOTDOT_STR, filename))
                        {
                            if (level == storageInfoLevelExists)
                            {
                                storageLstAdd(
                                    result,
                                    &(StorageInfo)
                                    {
                                        .name = STR(filename),
                                        .level = storageInfoLevelExists,
                                        .exists = true,
                                    });
                            }
                            else
                                storageSftpListEntry(this, result, path, filename, level);
                        }
                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);

                        // Reset the timeout so we don't timeout before reading all entries
                        this->wait = waitNew(this->timeoutConnect);
                    }
                }
                while (len > 0 || (len == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait)));
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            int rc = 0;

            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_closedir(sftpHandle);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            if (rc)
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
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            if (param.errorOnMissing || !storageSftpLibSsh2FxNoSuchFile(this, rc))
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
                    strNewFmt("unable to remove '%s'", strZ(file)),
                    NULL);
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
            this, file, ignoreMissing, this->ioSession, this->session, this->sftpSession, this->sftpHandle, this->timeoutSession,
            this->timeoutConnect, param.offset, param.limit));
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

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteSftpNew(
            this, file, this->ioSession, this->session, this->sftpSession, this->sftpHandle, this->timeoutConnect,
            this->timeoutSession, param.modeFile, param.modePath, param.user, param.group, param.timeModified, param.createPath,
            param.syncFile, this->interface.pathSync != NULL ? param.syncPath : false, param.atomic, param.truncate));
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

    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    // Attempt to create the directory
    do
    {
        rc = libssh2_sftp_mkdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), (int)mode);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
         if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
         {
             uint64_t sftpErrno = libssh2_sftp_last_error(this->sftpSession);

            // libssh2 may return LIBSSH2_FX_FAILURE if the directory already exists
            // need to find out if we can determine server version
             if (sftpErrno == LIBSSH2_FX_FAILURE)
             {
                 // Check if the directory already exists
                 LIBSSH2_SFTP_ATTRIBUTES attrs;

                 this->wait = waitNew(this->timeoutConnect);

                 do
                 {
                     rc = libssh2_sftp_stat_ex(
                        this->sftpSession, strZ(path), (unsigned int)strSize(path), LIBSSH2_SFTP_STAT, &attrs);
                 }
                 while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

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
                THROW_FMT(PathCreateError, "unable to create path '%s'", strZ(path));
         }
         else
             THROW_FMT(PathCreateError, "unable to create path '%s'", strZ(path));
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
                        int rc = 0;

                        this->wait = waitNew(this->timeoutConnect);

                        do
                        {
                            rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
                        }
                        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

                        if (rc)
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
        int rc = 0;
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_rmdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path));
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        if (rc)
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
                THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));

                // Path does not exist
                result = false;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceSftp =
{
    .feature = 1 << storageFeaturePath,

    .info = storageSftpInfo,
    .linkCreate = storageSftpLinkCreate,
    .list = storageSftpList,
    .newRead = storageSftpNewRead,
    .newWrite = storageSftpNewWrite,
    .pathCreate = storageSftpPathCreate,
    .pathRemove = storageSftpPathRemove,
    .remove = storageSftpRemove,
};

Storage *
storageSftpNewInternal(
    StringId type, const String *const path, const String *const host, unsigned int port, const TimeMSec timeoutConnect,
    const TimeMSec timeoutSession, const String *const user, const String *const keyPub, const String *const keyPriv,
    const String *const keyPassphrase, const StringId hostkeyHash, const mode_t modeFile, const mode_t modePath, const bool write,
    StoragePathExpressionCallback pathExpressionFunction, const bool pathSync)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, keyPub);
        FUNCTION_LOG_PARAM(STRING, keyPriv);
        FUNCTION_LOG_PARAM(STRING, keyPassphrase);
        FUNCTION_LOG_PARAM(STRING_ID, hostkeyHash);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, pathSync);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Initialize user module
    userInit();

    // Create the object
    Storage *this = NULL;

    OBJ_NEW_BEGIN(StorageSftp, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        StorageSftp *driver = OBJ_NEW_ALLOC();

        *driver = (StorageSftp)
        {
            .interface = storageInterfaceSftp,
            .timeoutConnect = timeoutConnect,
            .timeoutSession = timeoutSession,
        };

        driver->libssh2_initStatus = libssh2_init(0);
        if (driver->libssh2_initStatus != 0)
            THROW_FMT(ServiceError, "unable to init libssh2");

        driver->ioSession = ioClientOpen(sckClientNew(host, port, timeoutConnect, timeoutSession));

        driver->session = libssh2_session_init();
        if (driver->session == NULL)
            THROW_FMT(ServiceError, "unable to init libssh2 session");

        // Returns void
        libssh2_session_set_blocking(driver->session, 0);

        Wait *wait = waitNew(timeoutConnect);

        do
        {
            driver->handshakeStatus = libssh2_session_handshake(driver->session, ioSessionFd(driver->ioSession));
        }
        while (driver->handshakeStatus == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

        if (driver->handshakeStatus != 0)
            THROW_FMT(ServiceError, "libssh2 handshake failed");

        int hash_type = 0;

        switch (hostkeyHash)
        {
            case STRID5("md5-ssh2-hostkey-hash", 0x9bd1be2273df48d4):
                hash_type = LIBSSH2_HOSTKEY_HASH_MD5;
                break;

            case STRID6("sha1-ssh2-hostkey-hash", 0x6de2134db7412135):
                hash_type = LIBSSH2_HOSTKEY_HASH_SHA1;
                break;

#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
            case STRID5("sha256-ssh2-hostkey-hash", 0xdf1139efdde05134):
                hash_type = LIBSSH2_HOSTKEY_HASH_SHA256;
                break;
#endif // LIBSSH2_HOSTKEY_HASH_SHA256

            default:
                THROW_FMT(ServiceError, "requested ssh2 hostkey hash type (%s) not available", strZ(strIdToStr(hostkeyHash)));
                break;
        }

        LOG_DEBUG_FMT("requested hostkey hash %s, set as %d", strZ(strIdToStr(hostkeyHash)), hash_type);

        //should we allow for stronger hash sha - newer versions accept sha256 I think. Should this be an option?
        const char *digest = libssh2_hostkey_hash(driver->session, hash_type);
        if (digest == NULL)
            THROW_FMT(ServiceError, "libssh2 hostkey hash failed: libssh2 errno [%d]", libssh2_session_last_errno(driver->session));

        int rc = 0;

        wait = waitNew(timeoutConnect);

        if (strZNull(user) != NULL && strZNull(keyPriv) != NULL)
        {
            LOG_DEBUG_FMT("attempting public key authentication");

            do
            {
                rc = libssh2_userauth_publickey_fromfile(
                        driver->session, strZ(user), strZNull(keyPub) == NULL ? NULL : strZ(keyPub), strZ(keyPriv),
                        strZNull(keyPassphrase) == NULL ? NULL : strZ(keyPassphrase));
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

            if (rc)
            {
                storageSftpEvalLibSsh2Error(
                    libssh2_session_last_errno(driver->session), libssh2_sftp_last_error(driver->sftpSession), &ServiceError,
                    STRDEF("public key authentication failed"),
                    STRDEF(
                        "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-keyfile and"
                        " --repo-sftp-public-keyfile to be provided\n"
                        "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to"
                        " generate the keypair"));
            }
        }
        else
            THROW_FMT(ParamRequiredError, "user and private key required");

        wait = waitNew(timeoutConnect);

        do
        {
            driver->sftpSession = libssh2_sftp_init(driver->session);
        }
        while (driver->sftpSession == NULL && waitMore(wait));

        if (driver->sftpSession == NULL)
            THROW_FMT(ServiceError, "unable to init libssh2_sftp session");

        // Disable path sync when not supported
        // libssh2 doesn't appear to support path sync. It returns LIBSSH2_FX_NO_SUCH_FILE.
        if (!pathSync)
            driver->interface.pathSync = NULL;

        // If this is a sftp driver then add link features
        if (type == STORAGE_SFTP_TYPE)
            driver->interface.feature |=
                1 << storageFeatureSymLink | 1 << storageFeatureInfoDetail;

        // Ensure libssh2/libssh2_sftp resources freed
        memContextCallbackSet(objMemContext(driver), storageSftpLibSsh2SessionFreeResource, driver);

        this = storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}

Storage *
storageSftpNew(const String *const path, const String *const host, const unsigned int port, const TimeMSec timeoutConnect,
        const TimeMSec timeoutSession, const StorageSftpNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.keyPub);
        FUNCTION_LOG_PARAM(STRING, param.keyPriv);
        FUNCTION_LOG_PARAM(STRING, param.keyPassphrase);
        FUNCTION_LOG_PARAM(STRING_ID, param.hostkeyHash);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(BOOL, param.write);
        FUNCTION_LOG_PARAM(FUNCTIONP, param.pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageSftpNewInternal(
            STORAGE_SFTP_TYPE, path, host, port, timeoutConnect, timeoutSession, param.user, param.keyPub, param.keyPriv,
            param.keyPassphrase, param.hostkeyHash, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, param.pathExpressionFunction, false));
}

#endif // HAVE_LIBSSH2
