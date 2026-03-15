#define FUSE_USE_VERSION 31

#include <fuse.h>

typedef struct CmdRestoreMountFusePath
{
    const String *name;                                             // Name
    mode_t mode;                                                    // Mode
    uid_t user;                                                     // User
    gid_t group;                                                    // Group
    time_t timestamp;                                               // Timestamp
    const ManifestPath *path;                                       // Manifest path
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

static struct CmdRestoreMountFuseLocal
{
    Manifest *manifest;                                             // Manifest containing base files to mount
    const Storage *storageRepo;                                     // Repository storage
    const Storage *storageSpool;                                    // Spool storage
    const String *label;                                            // Backup label
    List *pathList;                                                 // Path list
    List *fileList;                                                 // File list
    uint32_t fileId;                                                // File id sequence
} cmdRestoreMountFuseLocal;

// static String *
// restoreMountManifestPath(const char *const path)
// {
//     String *const result = strNew();

//     if (strcmp(path, "/") == 0)
//         strCatZ(result, MANIFEST_TARGET_PGDATA);
//     else
//         strCatFmt(result, MANIFEST_TARGET_PGDATA "%s", path);

//     return result;
// }

static void *
restoreMountFuseInit(struct fuse_conn_info *const conn, struct fuse_config *const cfg)
{
    (void) conn;
    cfg->kernel_cache = 1;

    return NULL;
}

static int
restoreMountFuseGetAttr(const char *const pathZ, struct stat *const stbuf, struct fuse_file_info *const fi)
{
    (void) fi;
    int res = 0;

    // fprintf(stdout, "!!!GETATTR %s\n", pathZ);fflush(stdout);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const pathStr = STR(pathZ);
        const CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);

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
            const CmdRestoreMountFusePath *const path = lstFind(cmdRestoreMountFuseLocal.pathList, &pathStr);

            if (path != NULL)
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
            else
            {
                // fprintf(stdout, "!!!  GETATTR NOT FOUND %s\n", pathZ);fflush(stdout);
                res = -ENOENT;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    return res;
}

static int
restoreMountFuseReadDir(
    const char *const pathZ, void *const buf, const fuse_fill_dir_t filler, const off_t offset,
    struct fuse_file_info *const fi, const enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    // fprintf(stdout, "!!!READIR %s\n", pathZ);fflush(stdout);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const pathStr = strCatZ(strNew(), pathZ);
        const CmdRestoreMountFusePath *const path = lstFind(cmdRestoreMountFuseLocal.pathList, &pathStr);

        ASSERT(path != NULL);

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        if (!strEq(pathStr, FSLASH_STR))
            strCatChr(pathStr, '/');

        for (unsigned int pathIdx = 1; pathIdx < lstSize(cmdRestoreMountFuseLocal.pathList); pathIdx++)
        {
            const CmdRestoreMountFusePath *const pathSub = lstGet(cmdRestoreMountFuseLocal.pathList, pathIdx);

            // fprintf(stdout, "!!!READIR CHECK SUB %s PARENT %s\n", strZ(pathSub->name), strZ(pathStr));fflush(stdout);

            if (strBeginsWith(pathSub->name, pathStr))
            {
                if (strstr(strZ(pathSub->name) + strSize(pathStr), "/") == NULL)
                    filler(buf, strBaseZ(pathSub->name), NULL, 0, 0);
            }
        }

        for (unsigned int fileIdx = 0; fileIdx < lstSize(cmdRestoreMountFuseLocal.fileList); fileIdx++)
        {
            const CmdRestoreMountFuseFile *const file = lstGet(cmdRestoreMountFuseLocal.fileList, fileIdx);

            if (strBeginsWith(file->name, pathStr))
            {
                if (strstr(strZ(file->name) + strSize(pathStr), "/") == NULL)
                    filler(buf, strBaseZ(file->name), NULL, 0, 0);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    return 0;
}

static int
restoreMountFuseCreate(const char *const pathZ, const mode_t mode, struct fuse_file_info *const fi)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM(MODE, mode);
        FUNCTION_LOG_PARAM_P(VOID, fi);
    FUNCTION_LOG_END();

    int result = 0;

    fprintf(stdout, "!!!CREATE %s\n", pathZ);fflush(stdout);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const pathStr = STR(pathZ);
        ASSERT(lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr) == NULL);
        uint32_t fileId;

        MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.fileList)
        {
            const CmdRestoreMountFuseFile fileNew =
            {
                .name = strNewZ(pathZ),
                .id = ++cmdRestoreMountFuseLocal.fileId,
                .mode = mode,
                .user = userId(),
                .group = groupId(),
                .timestamp = time(NULL),
            };

            lstAdd(cmdRestoreMountFuseLocal.fileList, &fileNew);
            lstSort(cmdRestoreMountFuseLocal.fileList, sortOrderAsc);

            fileId = fileNew.id;
        }
        MEM_CONTEXT_OBJ_END();

        const String *const fileStr = storagePathP(
            cmdRestoreMountFuseLocal.storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", fileId));

        const int fh = open(strZ(fileStr), fi->flags, mode);

        // fprintf(stdout, "!!!  CREATE %s RESULT %d\n", strZ(fileStr), fh);fflush(stdout);

        if (fh == -1)
        {
            LOG_DEBUG_FMT("ERROR %s", strerror(errno));
            result = -errno;

        }
        else
            fi->fh = (uint64_t)fh;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

static void
restoreMountFusePersist(CmdRestoreMountFuseFile *const file)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, file);
    FUNCTION_LOG_END();

    if (file->id == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            ASSERT(file->filePack != NULL);
            const ManifestFile manifestFile = manifestFileUnpack(cmdRestoreMountFuseLocal.manifest, file->filePack);

            file->id = ++cmdRestoreMountFuseLocal.fileId;

            StorageRead *const storageRead = storageNewReadP(
                cmdRestoreMountFuseLocal.storageRepo,
                strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(cmdRestoreMountFuseLocal.label), strZ(manifestFile.name)));
            StorageWrite *const storageWrite = storageNewWriteP(
                cmdRestoreMountFuseLocal.storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id), .noAtomic = true,
                .noSyncPath = true, .noSyncFile = true);

            storageCopyP(storageRead, storageWrite);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

static int
restoreMountFuseOpenInternal(CmdRestoreMountFuseFile *const file, const int flags)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, file);
    FUNCTION_LOG_END();

    int result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const spoolPathStr = storagePathP(
            cmdRestoreMountFuseLocal.storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id));

        result = open(strZ(spoolPathStr), flags);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

static int
restoreMountFuseOpen(const char *const pathZ, struct fuse_file_info *const fi)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM_P(VOID, fi);
    FUNCTION_LOG_END();

    int result = 0;

    fprintf(stdout, "!!!OPEN %s\n", pathZ);fflush(stdout);

    const String *const pathStr = STR(pathZ);
    CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);

    restoreMountFusePersist(file);
    const int fd = restoreMountFuseOpenInternal(file, fi->flags);

    fprintf(stdout, "!!!  OPEN INTERNAL %s ID %u FD %d\n", pathZ, file->id, fd);fflush(stdout);

    if (result == -1)
        result = -errno;
    else
    {

            /* Enable direct_io when open has flags O_DIRECT to enjoy the feature
            parallel_direct_writes (i.e., to get a shared lock, not exclusive lock,
            for writes to the same file). */
        // if (fi->flags & O_DIRECT) {
        //     fi->direct_io = 1;
        //     fi->parallel_direct_writes = 1;
        // }

        fi->fh = (uint64_t)fd;
    }

    FUNCTION_LOG_RETURN(INT, result);
}

static int
restoreMountFuseRead(const char *const pathZ, char *const buffer, size_t size, const off_t offset, struct fuse_file_info *const fi)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
        FUNCTION_LOG_PARAM_P(VOID, buffer);
        FUNCTION_LOG_PARAM(SIZE, size);
        FUNCTION_LOG_PARAM(INT64, offset);
        FUNCTION_LOG_PARAM_P(VOID, fi);
    FUNCTION_LOG_END();

    int result = 0;

    fprintf(stdout, "!!!READ %s OFFSET %zu SIZE %zu\n", pathZ, (size_t)offset, size);fflush(stdout);

    // Read requested range
    int fd;

    if (fi == NULL)
    {
        const String *const pathStr = STR(pathZ);
        CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);

        restoreMountFusePersist(file);
        fd = restoreMountFuseOpenInternal(file, O_RDONLY);
    }
    else
        fd = (int)fi->fh;

    if (fd == -1)
        result = -errno;
    else
    {
        result = (int)pread(fd, buffer, size, offset);

        fprintf(stdout, "!!!  READ RESULT OFFSET %zu SIZE %zu FD %d RESULT %d ERR %d STR %s\n", (size_t)offset, size, fd, result, errno, strerror(errno));fflush(stdout);

        if (result == -1)
            result = -errno;

        if (fi == NULL)
            close(fd);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

static int
restoreMountFuseWrite(
    const char *const pathZ, const char *const buffer, const size_t size, const off_t offset, struct fuse_file_info *const fi)
{
    int fd;
    int result;
    ASSERT(offset >= 0);

    fprintf(stdout, "!!!WRITE %s OFFSET %zu SIZE %zu\n", pathZ, (size_t)offset, size);fflush(stdout);

    const String *const pathStr = STR(pathZ);
    CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);
    ASSERT(file != NULL);

    if (fi == NULL)
    {
        String *const fileStr = storagePathP(
            cmdRestoreMountFuseLocal.storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id));

        fd = open(strZ(fileStr), O_WRONLY);
        strFree(fileStr);
    }
    else
        fd = (int)fi->fh;

    if (fd == -1)
        return -errno;

    result = (int)pwrite(fd, buffer, size, offset);

    if ((size_t)offset + size > file->size)
        file->size = (size_t)offset + size;

    if (result == -1)
        result = -errno;

    if (fi == NULL)
        close(fd);

    return result;
}

static int
restoreMountFuseRelease(const char *const pathZ, struct fuse_file_info *const fi)
{
    fprintf(stdout, "!!!RELEASE %s\n", pathZ);fflush(stdout);

    (void)pathZ;
    close((int)fi->fh);
    return 0;
}

static int restoreMountFuseMkDir(const char *const pathZ, const mode_t mode)
{
    // fprintf(stdout, "!!!MKDIR %s\n", pathZ);fflush(stdout);

    ASSERT_DECLARE(const String *const pathStr = STR(pathZ));
    ASSERT(lstFind(cmdRestoreMountFuseLocal.pathList, &pathStr) == NULL);

    MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.pathList)
    {
        const CmdRestoreMountFusePath pathNew =
        {
            .name = strNewZ(pathZ),
            .mode = mode,
            .user = userId(),
            .group = groupId(),
            .timestamp = time(NULL),
        };

        lstAdd(cmdRestoreMountFuseLocal.pathList, &pathNew);
        lstSort(cmdRestoreMountFuseLocal.pathList, sortOrderAsc);
    }
    MEM_CONTEXT_OBJ_END();

    return 0;
}

static int restoreMountFuseRmDir(const char *const pathZ)
{
    // fprintf(stdout, "!!!RMDIR %s\n", pathZ);fflush(stdout);

    const String *const pathStr = STR(pathZ);
    ASSERT(lstFind(cmdRestoreMountFuseLocal.pathList, &pathStr) != NULL);

    lstRemove(cmdRestoreMountFuseLocal.pathList, &pathStr);

    return 0;
}

static int restoreMountFuseRename(const char *const fromZ, const char *const toZ, const unsigned int flags)
{
    // fprintf(stdout, "!!!RENAME %s to %s FLAGS %u\n", fromZ, toZ, flags);fflush(stdout);

    if (flags)
        return -EINVAL;

    const String *const pathStr = STR(fromZ);
    CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);
    ASSERT(file != NULL);

    // fprintf(stdout, "!!!  GOT HERE\n");fflush(stdout);

    MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.fileList)
    {
        // strFree(file->name);
        file->name = strNewZ(toZ);
    }
    MEM_CONTEXT_OBJ_END();

    return 0;
}

static int
restoreMountFuseUnlink(const char *const pathZ)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRINGZ, pathZ);
    FUNCTION_LOG_END();

    fprintf(stdout, "!!!UNLINK %s\n", pathZ);fflush(stdout);

    int result = 0;

    const String *const pathStr = STR(pathZ);
    CmdRestoreMountFuseFile *const file = lstFind(cmdRestoreMountFuseLocal.fileList, &pathStr);

    if (file == NULL)
        result = -ENOENT;
    else
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *const fileStr = storagePathP(
                cmdRestoreMountFuseLocal.storageSpool, strNewFmt(STORAGE_SPOOL_MOUNT "/%u", file->id));

            lstRemove(cmdRestoreMountFuseLocal.fileList, &pathStr);
            THROW_ON_SYS_ERROR_FMT(unlink(strZ(fileStr)) == -1, FileRemoveError, "unable to remove '%s'", strZ(fileStr));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(INT, result);
}

static const struct fuse_operations restoreMountFuseOper =
{
    .init = restoreMountFuseInit,
    .getattr = restoreMountFuseGetAttr,
    .readdir = restoreMountFuseReadDir,
    .mkdir = restoreMountFuseMkDir,
    .rmdir = restoreMountFuseRmDir,
    .unlink = restoreMountFuseUnlink,
    .rename = restoreMountFuseRename,
    .create = restoreMountFuseCreate,
    .open = restoreMountFuseOpen,
    .read = restoreMountFuseRead,
    .write = restoreMountFuseWrite,
    .release = restoreMountFuseRelease,
};

static void
restoreMount(
    Manifest *const manifest, const Storage *const storageRepo, const Storage *const storageSpool, const String *const label)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STORAGE, storageSpool);
        FUNCTION_LOG_PARAM(STRING, label);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        cmdRestoreMountFuseLocal = (struct CmdRestoreMountFuseLocal)
        {
            .manifest = manifest,
            .storageRepo = storageRepo,
            .storageSpool = storageSpool,
            .label = label,
            .pathList = lstNewP(sizeof(CmdRestoreMountFusePath), .comparator = lstComparatorStr),
            .fileList = lstNewP(sizeof(CmdRestoreMountFuseFile), .comparator = lstComparatorStr),
        };

        // Load paths
        MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.pathList)
        {
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                const ManifestPath *const path = manifestPath(manifest, pathIdx);

                // fprintf(stdout, "!!!PATH %s\n", strZ(path->name));fflush(stdout);

                const CmdRestoreMountFusePath pathNew =
                {
                    .name =
                        strEq(path->name, MANIFEST_TARGET_PGDATA_STR) ?
                            strNewZ("/") : strSub(path->name, sizeof(MANIFEST_TARGET_PGDATA) - 1),
                    .mode = path->mode,
                    .user = userIdFromName(path->user),
                    .group = groupIdFromName(path->group),
                    .timestamp = manifestData(manifest)->backupTimestampStop,
                    .path = path,
                };

                lstAdd(cmdRestoreMountFuseLocal.pathList, &pathNew);
            }

            lstSort(cmdRestoreMountFuseLocal.pathList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();

        // Load files
        MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.fileList)
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFilePack *const filePack = manifestFilePackGet(manifest, fileIdx);
                const ManifestFile file = manifestFileUnpack(manifest, filePack);

                // fprintf(stdout, "!!!FILE %s\n", strZ(file.name));fflush(stdout);

                const CmdRestoreMountFuseFile fileNew =
                {
                    .name = strSub(file.name, sizeof(MANIFEST_TARGET_PGDATA) - 1),
                    .mode = file.mode,
                    .user = userIdFromName(file.user),
                    .group = groupIdFromName(file.group),
                    .timestamp = file.timestamp,
                    .size = file.size,
                    .filePack = filePack,
                };

                lstAdd(cmdRestoreMountFuseLocal.fileList, &fileNew);
            }

            lstSort(cmdRestoreMountFuseLocal.fileList, sortOrderAsc);
        }
        MEM_CONTEXT_OBJ_END();

        // char bin[] = "bin";
        // char mount[1024];
        // strcpy(mount, strZ(cfgOptionStrNull(cfgOptPgPath)));
        // char *argList[] = {bin, mount, 0};

        // struct fuse_args args = FUSE_ARGS_INIT(2, argList);

        // fuse_main(args.argc, args.argv, &restoreMountFuseOper, NULL);
        // fuse_opt_free_args(&args);

        // char bin[] = "bin";
        // char *argList[] = {bin, 0};

        struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
        fuse_opt_add_arg(&args, PROJECT_NAME);

        struct fuse *const fuse = fuse_new(&args, &restoreMountFuseOper, sizeof(restoreMountFuseOper), NULL);

        if (fuse == NULL) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_new() failed"); // {uncovered - !!!}

        if (fuse_mount(fuse, strZ(cfgOptionStrNull(cfgOptPgPath))) != 0) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_mount() failed"); // {uncovered - !!!}

        // if (fuse_daemonize(false) != 0)
        //     THROW(AssertError, "fuse_daemonize() failed");

        if (fuse_set_signal_handlers(fuse_get_session(fuse)) != 0) // {uncovered_branch - !!!}
            THROW(AssertError, "fuse_set_signal_handlers() failed"); // {uncovered - !!!}

        fuse_loop(fuse);

        fuse_remove_signal_handlers(fuse_get_session(fuse));
        fuse_unmount(fuse);
        fuse_destroy(fuse);
        // fprintf(stdout, "GOT HERE!!!\n");fflush(stdout);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
