#define FUSE_USE_VERSION 31

#include <fuse.h>

typedef struct CmdRestoreMountFusePath
{
    const String *name;                                             // Name
    mode_t mode;                                                    // Mode
    uid_t user;                                                     // User
    gid_t group;                                                    // Group
    time_t timestamp;                                               // Timestamp
    const ManifestPath *path;                                       // Path
} CmdRestoreMountFusePath;

static struct CmdRestoreMountFuseLocal
{
    Manifest *manifest;                                             // Manifest containing base files to mount
    const Storage *storage;                                         // Repository storage
    const String *label;                                            // Backup label
    List *pathList;                                                 // Path list
} cmdRestoreMountFuseLocal;

static String *
restoreMountManifestPath(const char *const path)
{
    String *const result = strNew();

    if (strcmp(path, "/") == 0)
        strCatZ(result, MANIFEST_TARGET_PGDATA);
    else
        strCatFmt(result, MANIFEST_TARGET_PGDATA "%s", path);

    return result;
}

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

    fprintf(stdout, "!!!GETATTR %s\n", pathZ);fflush(stdout);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        const String *const pathStr = STR(pathZ);
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
            TRY_BEGIN()
            {
                String *const manifestName = restoreMountManifestPath(pathZ);
                const ManifestFile manifestFile = manifestFileFind(manifest, manifestName);

                *stbuf = (struct stat)
                {
                    .st_mode = S_IFREG | manifestFile.mode,
                    .st_nlink = 1,
                    .st_uid = userIdFromName(manifestFile.user),
                    .st_gid = groupIdFromName(manifestFile.group),
                    .st_size = (off_t)manifestFile.size,
                    .st_atime = manifestFile.timestamp,
                    .st_mtime = manifestFile.timestamp,
                    .st_ctime = manifestFile.timestamp,
                };
            }
            CATCH_FATAL()
            {
                fprintf(stdout, "!!!  GETATTR NOT FOUND %s\n", pathZ);fflush(stdout);
                res = -ENOENT;
            }
            TRY_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    return res;
}

static int
restoreMountFuseReadDir(
    const char *const path, void *const buf, const fuse_fill_dir_t filler, const off_t offset,
    struct fuse_file_info *const fi, const enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    fprintf(stdout, "!!!READIR %s\n", path);fflush(stdout);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        String *const manifestName = restoreMountManifestPath(path);
        String *const pathName = strCatZ(strNew(), path);
        const CmdRestoreMountFusePath *const pathParent = lstFind(cmdRestoreMountFuseLocal.pathList, &pathName);

        ASSERT(pathParent != NULL);

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        strCatChr(manifestName, '/');

        if (!strEq(pathName, FSLASH_STR))
            strCatChr(pathName, '/');

        for (unsigned int pathIdx = 0; pathIdx < lstSize(cmdRestoreMountFuseLocal.pathList); pathIdx++)
        {
            const CmdRestoreMountFusePath *const pathSub = lstGet(cmdRestoreMountFuseLocal.pathList, pathIdx);

            // fprintf(stdout, "!!!READIR CHECK SUB %s PARENT %s\n", strZ(pathSub->name), strZ(pathName));fflush(stdout);

            if (strBeginsWith(pathSub->name, pathName) && !strEq(pathSub->name, pathName))
            {
                if (strstr(strZ(pathSub->name) + strSize(pathName), "/") == NULL)
                    filler(buf, strBaseZ(pathSub->name), NULL, 0, 0);
            }
        }

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile manifestFileSub = manifestFile(manifest, fileIdx);

            if (strBeginsWith(manifestFileSub.name, manifestName))
            {
                if (strstr(strZ(manifestFileSub.name) + strSize(manifestName), "/") == NULL)
                    filler(buf, strBaseZ(manifestFileSub.name), NULL, 0, 0);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    return 0;
}

static int
restoreMountFuseOpen(const char *const path, struct fuse_file_info *const fi)
{
    (void)path;
    (void)fi;

    int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        String *const manifestName = restoreMountManifestPath(path);

        TRY_BEGIN()
        {
            manifestFileFind(manifest, manifestName);
        }
        CATCH_FATAL()
        {
            result = -ENOENT;
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        result = -EACCES;

    return result;
}

static int
restoreMountFuseRead(const char *const path, char *const buf, size_t size, const off_t offset, struct fuse_file_info *const fi)
{
    (void)fi;
    (void)buf;

    int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        String *const manifestName = restoreMountManifestPath(path);
        Buffer *const buffer = bufNew(size);

        TRY_BEGIN()
        {
            /*const ManifestFile manifestFile = */manifestFileFind(manifest, manifestName);
            StorageRead *const storageRead = storageNewReadP(
                cmdRestoreMountFuseLocal.storage,
                strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(cmdRestoreMountFuseLocal.label), strZ(manifestName)),
                .offset = (uint64_t)offset, .limit = VARUINT64(size));

            ioReadOpen(storageReadIo(storageRead));

            result = (int)ioRead(storageReadIo(storageRead), buffer);
            memcpy(buf, bufPtrConst(buffer), (size_t)result);
        }
        CATCH_FATAL()
        {
            result = -ENOENT;
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

static int restoreMountFuseMkDir(const char *const pathZ, const mode_t mode)
{
    fprintf(stdout, "!!!MKDIR %s\n", pathZ);fflush(stdout);

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
    fprintf(stdout, "!!!RMDIR %s\n", pathZ);fflush(stdout);

    const String *const pathStr = STR(pathZ);
    ASSERT(lstFind(cmdRestoreMountFuseLocal.pathList, &pathStr) != NULL);

    lstRemove(cmdRestoreMountFuseLocal.pathList, &pathStr);

    return 0;
}

static const struct fuse_operations restoreMountFuseOper =
{
    .init = restoreMountFuseInit,
    .getattr = restoreMountFuseGetAttr,
    .readdir = restoreMountFuseReadDir,
    .mkdir = restoreMountFuseMkDir,
    .rmdir = restoreMountFuseRmDir,
    .open = restoreMountFuseOpen,
    .read = restoreMountFuseRead,
};

static void
restoreMount(Manifest *const manifest, const Storage *const storage, const String *const label)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STORAGE, storage);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        cmdRestoreMountFuseLocal = (struct CmdRestoreMountFuseLocal)
        {
            .manifest = manifest,
            .storage = storage,
            .label = label,
            .pathList = lstNewP(sizeof(CmdRestoreMountFusePath), .comparator = lstComparatorStr),
        };

        MEM_CONTEXT_OBJ_BEGIN(cmdRestoreMountFuseLocal.pathList)
        {
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                const ManifestPath *const path = manifestPath(manifest, pathIdx);

                fprintf(stdout, "!!!PATH %s\n", strZ(path->name));fflush(stdout);

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

        if (fuse == NULL)
            THROW(AssertError, "fuse_new() failed");

        if (fuse_mount(fuse, strZ(cfgOptionStrNull(cfgOptPgPath))) != 0)
            THROW(AssertError, "fuse_mount() failed");

        // if (fuse_daemonize(false) != 0)
        //     THROW(AssertError, "fuse_daemonize() failed");

        if (fuse_set_signal_handlers(fuse_get_session(fuse)) != 0)
            THROW(AssertError, "fuse_set_signal_handlers() failed");

        fuse_loop(fuse);

        fuse_remove_signal_handlers(fuse_get_session(fuse));
        fuse_unmount(fuse);
        fuse_destroy(fuse);
        fprintf(stdout, "GOT HERE!!!\n");fflush(stdout);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
