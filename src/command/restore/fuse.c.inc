#define FUSE_USE_VERSION 31

#include <fuse.h>

#include <stdlib.h>

#include "common/fork.h"
#include "common/io/io.h"

typedef struct CmdRestoreMountFusePath
{
    const String *name;                                             // Name
    const String *link;                                             // Path is a virtual symlink
    mode_t mode;                                                    // Mode
    uid_t user;                                                     // User
    gid_t group;                                                    // Group
    time_t timestamp;                                               // Timestamp
} CmdRestoreMountFusePath;

typedef struct CmdRestoreMountFuseFile
{
    String *name;                                                   // Name
    uint32_t id;                                                    // Id used for file storage (0 means no file on disk)
    mode_t mode;                                                    // Mode
    uid_t user;                                                     // User
    gid_t group;                                                    // Group
    time_t timestamp;                                               // Timestamp
    uint64_t size;                                                  // Size
    const ManifestFilePack *filePack;                               // Manifest file pack
} CmdRestoreMountFuseFile;

typedef struct CmdRestoreMountFuseSession
{
    Manifest *manifest;                                             // Manifest containing base files to mount
    const Storage *storageRepo;                                     // Repository storage
    const Storage *storageSpool;                                    // Spool storage (for backing files)
    List *pathList;                                                 // Path list
    List *fileList;                                                 // File list
    uint32_t fileId;                                                // File id sequence
} CmdRestoreMountFuseSession;

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static CmdRestoreMountFuseSession *
restoreMountFuseSession(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_P(VOID, fuse_get_context()->private_data);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static struct CmdRestoreMountFusePath *
restoreMountFusePathFind(const char *const pathZ)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
    FUNCTION_TEST_END();

    const String *const pathStr = STR(pathZ);

    FUNCTION_TEST_RETURN_P(VOID, lstFind(restoreMountFuseSession()->pathList, &pathStr));
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static struct CmdRestoreMountFuseFile *
restoreMountFuseFileFind(const char *const fileZ)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
    FUNCTION_TEST_END();

    const String *const fileStr = STR(fileZ);

    FUNCTION_TEST_RETURN_P(VOID, lstFind(restoreMountFuseSession()->fileList, &fileStr));
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static String *
restoreMountFuseFileName(const CmdRestoreMountFuseFile *const file)
{
    // FUNCTION_TEST_BEGIN();
    //     FUNCTION_LOG_PARAM_P(VOID, file);
    // FUNCTION_TEST_END();

    // ASSERT(file != NULL);

    // FUNCTION_TEST_RETURN(
    //     STRING, storagePathP(restoreMountFuseSession()->storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id)));

    return storagePathP(restoreMountFuseSession()->storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id));
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static void *
restoreMountFuseInit(struct fuse_conn_info *const conn, struct fuse_config *const cfg)
{
    FUNCTION_TEST_BEGIN();
        (void)conn;
        (void)cfg;
    FUNCTION_TEST_END();

    ASSERT(cfg != NULL);

    // Disable kernel cache since the underlying files are cached
    cfg->kernel_cache = 1;

    // Enable direct io since caching is disabled
    cfg->direct_io = 1;

    // Use a large buffer
    conn->max_write = 1 * 1024 * 1024;

    // Our timestamps are only accurate to 1 second
    conn->time_gran = 1000000000;

    FUNCTION_TEST_RETURN_P(VOID, restoreMountFuseSession());
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseGetAttr(const char *const pathZ, struct stat *const stbuf, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        (void)stbuf;
        (void)fileInfo;
    FUNCTION_LOG_END();

    ASSERT(pathZ != NULL);
    ASSERT(stbuf != NULL);

    int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(pathZ);

        if (file != NULL)
        {
            *stbuf = (struct stat)
            {
                .st_mode = S_IFREG | file->mode,
                .st_nlink = 1,
                .st_uid = file->user,
                .st_gid = file->group,
                .st_size = (off_t)file->size,
                .st_atime = file->timestamp,
                .st_mtime = file->timestamp,
                .st_ctime = file->timestamp,
            };
        }
        else
        {
            const CmdRestoreMountFusePath *const path = restoreMountFusePathFind(pathZ);

            if (path != NULL)
            {
                if (path->link != NULL)
                {
                    *stbuf = (struct stat)
                    {
                        .st_mode = S_IFLNK | path->mode,
                        .st_nlink = 2,
                        .st_uid = path->user,
                        .st_gid = path->group,
                        .st_size = sizeof(CmdRestoreMountFusePath),
                        .st_atime = path->timestamp,
                        .st_mtime = path->timestamp,
                        .st_ctime = path->timestamp,
                    };
                }
                else
                {
                    *stbuf = (struct stat)
                    {
                        .st_mode = S_IFDIR | path->mode,
                        .st_nlink = 2,
                        .st_uid = path->user,
                        .st_gid = path->group,
                        .st_size = sizeof(CmdRestoreMountFusePath),
                        .st_atime = path->timestamp,
                        .st_mtime = path->timestamp,
                        .st_ctime = path->timestamp,
                    };
                }
            }
            else
                result = -ENOENT;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseReadDir(
    const char *const pathZ, void *const buffer, const fuse_fill_dir_t filler, const off_t offset,
    struct fuse_file_info *const fileInfo, const enum fuse_readdir_flags flags)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        (void)buffer;
        (void)filler;
        (void)offset;
        (void)fileInfo;
        (void)flags;
    FUNCTION_LOG_END();

    ASSERT(pathZ != NULL);
    ASSERT(buffer != NULL);
    ASSERT(filler != NULL);

    const CmdRestoreMountFuseSession *const session = restoreMountFuseSession();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const pathStr = strCatZ(strNew(), pathZ);
        const CmdRestoreMountFusePath *const path = restoreMountFusePathFind(pathZ);
        ASSERT(path != NULL);

        filler(buffer, ".", NULL, 0, 0);
        filler(buffer, "..", NULL, 0, 0);

        if (!strEq(pathStr, FSLASH_STR))
            strCatChr(pathStr, '/');

        for (unsigned int pathIdx = 1; pathIdx < lstSize(session->pathList); pathIdx++)
        {
            const CmdRestoreMountFusePath *const pathSub = lstGet(session->pathList, pathIdx);

            if (strBeginsWith(pathSub->name, pathStr))
            {
                if (strstr(strZ(pathSub->name) + strSize(pathStr), "/") == NULL)
                    filler(buffer, strBaseZ(pathSub->name), NULL, 0, 0);
            }
        }

        for (unsigned int fileIdx = 0; fileIdx < lstSize(session->fileList); fileIdx++)
        {
            const CmdRestoreMountFuseFile *const file = lstGet(session->fileList, fileIdx);

            if (strBeginsWith(file->name, pathStr))
            {
                if (strstr(strZ(file->name) + strSize(pathStr), "/") == NULL)
                    filler(buffer, strBaseZ(file->name), NULL, 0, 0);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, 0);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseCreate(const char *const pathZ, const mode_t mode, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM(MODE, mode);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    int result = 0;

    CmdRestoreMountFuseSession *const session = restoreMountFuseSession();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ASSERT(restoreMountFuseFileFind(pathZ) == NULL);

        MEM_CONTEXT_OBJ_BEGIN(session->fileList)
        {
            const CmdRestoreMountFuseFile fileNew =
            {
                .name = strNewZ(pathZ),
                .id = ++session->fileId,
                .mode = mode,
                .user = userId(),
                .group = groupId(),
                .timestamp = time(NULL),
            };

            lstAdd(session->fileList, &fileNew);
            lstSort(session->fileList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();

        const int fh = open(strZ(restoreMountFuseFileName(restoreMountFuseFileFind(pathZ))), fileInfo->flags, mode);

        if (fh == -1)
        {
            LOG_DEBUG_FMT("ERROR %s", strerror(errno));
            result = -errno;

        }
        else
            fileInfo->fh = (uint64_t)fh;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static void
restoreMountFusePersist(CmdRestoreMountFuseFile *const file)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, file);
    FUNCTION_LOG_END();

    if (file->id == 0)
    {
        CmdRestoreMountFuseSession *const session = restoreMountFuseSession();

        MEM_CONTEXT_TEMP_BEGIN()
        {
            ASSERT(file->filePack != NULL);
            const ManifestFile manifestFile = manifestFileUnpack(session->manifest, file->filePack);

            file->id = ++session->fileId;

            StorageRead *const storageRead = storageNewReadP(
                session->storageRepo,
                strNewFmt(
                    STORAGE_REPO_BACKUP "/%s/%s", strZ(manifestData(session->manifest)->backupLabel), strZ(manifestFile.name)));
            StorageWrite *const storageWrite = storageNewWriteP(
                session->storageSpool, restoreMountFuseFileName(file), .noAtomic = true, .noSyncPath = true, .noSyncFile = true);

            storageCopyP(storageRead, storageWrite);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseOpenInternal(CmdRestoreMountFuseFile *const file, const int flags)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, file);
    FUNCTION_LOG_END();

    int result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = open(strZ(restoreMountFuseFileName(file)), flags);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseOpen(const char *const fileZ, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    int result = 0;

    CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fileZ);

    if (file == NULL)
        result = -ENOENT;
    else
    {
        restoreMountFusePersist(file);
        const int fd = restoreMountFuseOpenInternal(file, fileInfo->flags);

        if (result == -1)
            result = -errno;
        else
            fileInfo->fh = (uint64_t)fd;
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseRead(
    const char *const fileZ, char *const buffer, const size_t size, const off_t offset, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
        (void)buffer;
        FUNCTION_LOG_PARAM(SIZE, size);
        FUNCTION_LOG_PARAM(INT64, offset);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    ASSERT(fileZ != NULL);
    ASSERT(buffer != NULL);
    ASSERT(size > 0);
    ASSERT(offset >= 0);

    int result = 0;

    // Read requested range
    int fd;

    if (fileInfo == NULL)
    {
        CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fileZ);
        ASSERT(file != NULL);

        restoreMountFusePersist(file);
        fd = restoreMountFuseOpenInternal(file, O_RDONLY);
    }
    else
        fd = (int)fileInfo->fh;

    if (fd == -1)
        result = -errno;
    else
    {
        result = (int)pread(fd, buffer, size, offset);

        if (result == -1)
            result = -errno;

        if (fileInfo == NULL)
            close(fd);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseWrite(
    const char *const fileZ, const char *const buffer, const size_t size, const off_t offset, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
        FUNCTION_LOG_PARAM_P(VOID, buffer);
        FUNCTION_LOG_PARAM(SIZE, size);
        FUNCTION_LOG_PARAM(INT64, offset);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    ASSERT(fileZ != NULL);
    ASSERT(buffer != NULL);
    ASSERT(size > 0);
    ASSERT(offset >= 0);

    CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fileZ);
    ASSERT(file != NULL);

    file->timestamp = time(NULL);

    int fd;

    if (fileInfo == NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            fd = open(strZ(restoreMountFuseFileName(file)), O_WRONLY);
        }
        MEM_CONTEXT_TEMP_END();
    }
    else
        fd = (int)fileInfo->fh;

    int result;

    if (fd == -1)
        result = -errno;
    else
    {
        result = (int)pwrite(fd, buffer, size, offset);

        if ((size_t)offset + size > file->size)
            file->size = (size_t)offset + size;

        if (result == -1)
            result = -errno;

        if (fileInfo == NULL)
            close(fd);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseRelease(const char *const pathZ, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    close((int)fileInfo->fh);

    FUNCTION_LOG_RETURN(INT, 0);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int restoreMountFuseMkDir(const char *const pathZ, const mode_t mode)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(restoreMountFusePathFind(pathZ) == NULL);

    const CmdRestoreMountFuseSession *const session = restoreMountFuseSession();

    MEM_CONTEXT_OBJ_BEGIN(session->pathList)
    {
        const CmdRestoreMountFusePath pathNew =
        {
            .name = strNewZ(pathZ),
            .mode = mode,
            .user = userId(),
            .group = groupId(),
            .timestamp = time(NULL),
        };

        lstAdd(session->pathList, &pathNew);
        lstSort(session->pathList, sortOrderAsc);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN(INT, 0);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int restoreMountFuseRmDir(const char *const pathZ)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
    FUNCTION_LOG_END();

    const String *const pathStr = STR(pathZ);
    ASSERT(restoreMountFusePathFind(pathZ) != NULL);

    lstRemove(restoreMountFuseSession()->pathList, &pathStr);

    FUNCTION_LOG_RETURN(INT, 0);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int restoreMountFuseRename(const char *const fromZ, const char *const toZ, const unsigned int flags)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fromZ);
        FUNCTION_LOG_PARAM(STRINGZ, toZ);
        FUNCTION_LOG_PARAM(UINT, flags);
    FUNCTION_LOG_END();

    const CmdRestoreMountFuseSession *const session = restoreMountFuseSession();
    int result = 0;

    // !!! MAKE SURE toZ does not exist

    if (flags) // !!! MAYBE MAKE THIS CHECK AN ASSERT?
        result = -EINVAL;
    else
    {
        CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fromZ);
        ASSERT(file != NULL);

        MEM_CONTEXT_OBJ_BEGIN(session->fileList)
        {
            strFree(file->name);
            file->name = strNewZ(toZ);

            lstSort(session->fileList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseUnlink(const char *const fileZ)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
    FUNCTION_LOG_END();

    int result = 0;

    const CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fileZ);

    if (file == NULL)
        result = -ENOENT;
    else
    {
        if (file->id != 0)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                THROW_ON_SYS_ERROR_FMT(
                    unlink(strZ(restoreMountFuseFileName(file))) == -1, FileRemoveError, "unable to remove '%s'", fileZ);
            }
            MEM_CONTEXT_TEMP_END();
        }

        const String *const fileStr = STR(fileZ);
        lstRemove(restoreMountFuseSession()->fileList, &fileStr);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseUTimeNS(const char *path, const struct timespec ts[2],
               struct fuse_file_info *const fileInfo)
{
    (void)path;
    (void)ts;
    (void)fileInfo;
    // int res;

    // /* don't use utime/utimes since they follow symlinks */
    // res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
    // if (res == -1)
    //     return -errno;

    return 0;
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int
restoreMountFuseTruncate(const char *const fileZ, const off_t size, struct fuse_file_info *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, fileZ);
        FUNCTION_LOG_PARAM(INT64, size);
        FUNCTION_LOG_PARAM_P(VOID, fileInfo);
    FUNCTION_LOG_END();

    int result = 0;

    CmdRestoreMountFuseFile *const file = restoreMountFuseFileFind(fileZ);

    if (file == NULL)
        result = -ENOENT;
    else
    {
        file->timestamp = time(NULL);
        file->size = (uint64_t)size;

        if (fileInfo != NULL)
            result = ftruncate((int)fileInfo->fh, size);
        else
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                result = truncate(strZ(restoreMountFuseFileName(file)), size);
            }
            MEM_CONTEXT_TEMP_END();
        }

        if (result == -1)
            result = -errno;
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int restoreMountFuseSymLink(const char *const fromZ, const char *const toZ)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        (void)fromZ;
        FUNCTION_LOG_PARAM(STRINGZ, toZ);
    FUNCTION_LOG_END();

    const CmdRestoreMountFuseSession *const session = restoreMountFuseSession();
    int result = restoreMountFuseMkDir(toZ, restoreMountFusePathFind("/")->mode);
    CmdRestoreMountFusePath *const path = restoreMountFusePathFind(toZ);

    MEM_CONTEXT_OBJ_BEGIN(restoreMountFuseSession()->pathList)
    {
        path->link = strNewFmt("../.pgbrfs/ts/%s", toZ + sizeof("/pg_tblspc"));

        CmdRestoreMountFusePath pathNew =
        {
            .name = strNewFmt("/.pgbrfs/ts/%s", toZ + sizeof("/pg_tblspc")),
            .mode = path->mode,
            .user = path->user,
            .group = path->group,
            .timestamp = time(NULL),
        };

        lstAdd(restoreMountFuseSession()->pathList, &pathNew);

        pathNew.name = strNewFmt(
            "%s/%s", strZ(pathNew.name),
            strZ(pgTablespaceId(manifestData(session->manifest)->pgVersion, manifestData(session->manifest)->pgCatalogVersion)));
        lstAdd(restoreMountFuseSession()->pathList, &pathNew);

        lstSort(restoreMountFuseSession()->pathList, sortOrderAsc);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static int restoreMountFuseReadLink(const char *const linkZ, char *const buffer, const size_t size)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, linkZ);
        FUNCTION_LOG_PARAM_P(VOID, buffer);
        FUNCTION_LOG_PARAM(SIZE, size);
    FUNCTION_LOG_END();

    CmdRestoreMountFusePath *const link = restoreMountFusePathFind(linkZ);

    // !!! NEED TO CHECK SIZE HERE
    strncpy(buffer, strZ(link->link), size - 1);
    buffer[size - 1] = '\0';

    return 0;
}
/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
static const struct fuse_operations restoreMountFuseOper =
{
    .init = restoreMountFuseInit,

    .getattr = restoreMountFuseGetAttr,
    .readdir = restoreMountFuseReadDir,
    // .chmod =
    // .chown =

    .mkdir = restoreMountFuseMkDir,
    .rmdir = restoreMountFuseRmDir,

    // !!! FOR SYMLINK WE COULD INTERCEPT SYMLINK CREATION AND JUST CREATE A DIR in PG_TBLSPC

    .create = restoreMountFuseCreate,
    .open = restoreMountFuseOpen,
    .read = restoreMountFuseRead,
    .write = restoreMountFuseWrite,
    // .lseek =
    .release = restoreMountFuseRelease,
    .utimens = restoreMountFuseUTimeNS,
    .truncate = restoreMountFuseTruncate,
    .unlink = restoreMountFuseUnlink,
    .rename = restoreMountFuseRename,
    .symlink = restoreMountFuseSymLink,
    .readlink = restoreMountFuseReadLink,
};

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
// Helper to convert manifest paths to pg paths
static String *
cmdRestoreAsyncName(const String *const manifestName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STRING, manifestName);
    FUNCTION_TEST_END();

    if (strBeginsWithZ(manifestName, MANIFEST_TARGET_PGDATA "/"))
    {
        if (strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL))
            FUNCTION_TEST_RETURN(STRING, strNewZ("/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "." STORAGE_FILE_TEMP_EXT));

        FUNCTION_TEST_RETURN(STRING, strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA) - 1));
    }

    if (strBeginsWithZ(manifestName, MANIFEST_TARGET_PGTBLSPC "/"))
    {
        if (strstr(strZ(manifestName) + sizeof(MANIFEST_TARGET_PGTBLSPC), "/") != NULL)
            FUNCTION_TEST_RETURN(STRING, strNewFmt("/.pgbrfs/ts/%s", strZ(manifestName) + sizeof(MANIFEST_TARGET_PGTBLSPC)));

        FUNCTION_TEST_RETURN(STRING, strNewFmt("/%s", strZ(manifestName)));
    }

    ASSERT(strEq(manifestName, MANIFEST_TARGET_PGDATA_STR));
    FUNCTION_TEST_RETURN(STRING, strNewZ("/"));
}

FN_EXTERN void
cmdRestoreAsync(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(cfgOptionSeq(cfgOptMount) != CFGOPTVAL_MOUNT_NONE);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get information for the current user
        userInit();

        // Initialize session
        CmdRestoreMountFuseSession session =
        {
            .manifest = manifestLoadFile(
                storageSpool(), STRDEF(STORAGE_SPOOL_MOUNT "/" BACKUP_MANIFEST_FILE), cipherTypeNone, NULL),
            .storageRepo = storageRepo(),
            .storageSpool = storageSpoolWrite(),
            .pathList = lstNewP(sizeof(CmdRestoreMountFusePath), .comparator = lstComparatorStr),
            .fileList = lstNewP(sizeof(CmdRestoreMountFuseFile), .comparator = lstComparatorStr),
        };

        // Load paths
        MEM_CONTEXT_OBJ_BEGIN(session.pathList)
        {
            bool fsAdded = false;

            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(session.manifest); pathIdx++)
            {
                const ManifestPath *const path = manifestPath(session.manifest, pathIdx);

                // Skip this path since is a duplicate of pg_data/pg_tblspc
                if (strEq(path->name, MANIFEST_TARGET_PGTBLSPC_STR))
                    continue;

                CmdRestoreMountFusePath pathNew =
                {
                    .name = cmdRestoreAsyncName(path->name),
                    .mode = path->mode,
                    .user = userIdFromName(path->user),
                    .group = groupIdFromName(path->group),
                    .timestamp = manifestData(session.manifest)->backupTimestampStop,
                };

                if (strBeginsWithZ(pathNew.name, "/" MANIFEST_TARGET_PGTBLSPC "/"))
                {
                    CmdRestoreMountFusePath pathFs =
                    {
                        .mode = path->mode,
                        .user = userIdFromName(path->user),
                        .group = groupIdFromName(path->group),
                        .timestamp = time(NULL),
                    };

                    if (!fsAdded)
                    {
                        pathFs.name = strNewZ("/.pgbrfs"),
                        lstAdd(session.pathList, &pathFs);

                        pathFs.name = strNewZ("/.pgbrfs/ts");
                        lstAdd(session.pathList, &pathFs);

                        fsAdded = true;
                    }

                    pathFs.name = strNewFmt("/.pgbrfs/ts/%s", strBaseZ(pathNew.name));
                    lstAdd(session.pathList, &pathFs);

                    pathNew.link = strNewFmt("../.pgbrfs/ts/%s", strBaseZ(pathNew.name));
                }

                lstAdd(session.pathList, &pathNew);
            }

            lstSort(session.pathList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();

        // Load files
        MEM_CONTEXT_OBJ_BEGIN(session.fileList)
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(session.manifest); fileIdx++)
            {
                const ManifestFilePack *const filePack = manifestFilePackGet(session.manifest, fileIdx);
                const ManifestFile file = manifestFileUnpack(session.manifest, filePack);

                const CmdRestoreMountFuseFile fileNew =
                {
                    .name = cmdRestoreAsyncName(file.name),
                    .mode = file.mode,
                    .user = userIdFromName(file.user),
                    .group = groupIdFromName(file.group),
                    .timestamp = file.timestamp,
                    .size = file.size,
                    .filePack = filePack,
                };

                lstAdd(session.fileList, &fileNew);
            }

            lstSort(session.fileList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();

        struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
        fuse_opt_add_arg(&args, PROJECT_NAME);

        struct fuse *const fuse = fuse_new(&args, &restoreMountFuseOper, sizeof(restoreMountFuseOper), &session);

        if (fuse == NULL) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_new() failed"); // {uncovered - !!!}

        if (fuse_mount(fuse, strZ(cfgOptionStrNull(cfgOptPgPath))) != 0) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_mount() failed"); // {uncovered - !!!}

        if (fuse_set_signal_handlers(fuse_get_session(fuse)) != 0) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_set_signal_handlers() failed"); // {uncovered - !!!}

        // Processing loop
        fuse_loop(fuse);

        // Unmount and destroy the filesystem
        fuse_remove_signal_handlers(fuse_get_session(fuse));
        fuse_unmount(fuse);
        fuse_destroy(fuse);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
