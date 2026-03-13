#define FUSE_USE_VERSION 30

#include <fuse.h>

// typedef struct CmdRestoreMountFusePath
// {
//     const String *name;                                             // Name
//     mode_t mode;                                                    // Mode
//     uid_t user;                                                     // User
//     gid_t group;                                                    // Group
//     const ManifestPath *path;                                       // Path
// } CmdRestoreMountFusePath;

static struct CmdRestoreMountFuseLocal
{
    Manifest *manifest;                                             // Manifest containing base files to mount
    const Storage *storage;                                         // Repository storage
    const String *label;                                            // Backup label
    // List *pathList;                                                 // Path list
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
restoreMountFuseGetAttr(const char *const path, struct stat *const stbuf, struct fuse_file_info *const fi)
{
    (void) fi;
    int res = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        String *const manifestName = restoreMountManifestPath(path);
        const ManifestPath *const manifestPath = manifestPathFindDefault(manifest, manifestName, NULL);

        if (manifestPath != NULL)
        {
            *stbuf = (struct stat)
            {
                .st_mode = S_IFDIR | manifestPath->mode,
                .st_nlink = 2,
                .st_uid = userIdFromName(manifestPath->user),
                .st_gid = groupIdFromName(manifestPath->group),
                .st_size = sizeof(ManifestPath), // !!! Change this to sizeof new path struct
                .st_mtime = manifestData(manifest)->backupTimestampStop,
            };
        }
        else
        {
            TRY_BEGIN()
            {
                const ManifestFile manifestFile = manifestFileFind(manifest, manifestName);

                *stbuf = (struct stat)
                {
                    .st_mode = S_IFREG | manifestFile.mode,
                    .st_nlink = 1,
                    .st_uid = userIdFromName(manifestFile.user),
                    .st_gid = groupIdFromName(manifestFile.group),
                    .st_size = (off_t)manifestFile.size,
                    .st_mtime = manifestFile.timestamp,
                };
            }
            CATCH_FATAL()
            {
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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifest = cmdRestoreMountFuseLocal.manifest;
        String *const manifestName = restoreMountManifestPath(path);
        const ManifestPath *const manifestPathParent = manifestPathFindDefault(manifest, manifestName, NULL);

        if (manifestPathParent == NULL)
            return -ENOENT;

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        strCatChr(manifestName, '/');

        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        {
            const ManifestPath *const manifestPathSub = manifestPath(manifest, pathIdx);

            if (strBeginsWith(manifestPathSub->name, manifestName))
            {
                if (strstr(strZ(manifestPathSub->name) + strSize(manifestName), "/") == NULL)
                    filler(buf, strBaseZ(manifestPathSub->name), NULL, 0, 0);
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

static const struct fuse_operations restoreMountFuseOper =
{
    .init = restoreMountFuseInit,
    .getattr = restoreMountFuseGetAttr,
    .readdir = restoreMountFuseReadDir,
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
        char bin[] = "bin";
        char mount[1024];
        strcpy(mount, strZ(cfgOptionStrNull(cfgOptPgPath)));
        char *argList[] = {bin, mount, 0};

        struct fuse_args args = FUSE_ARGS_INIT(2, argList);

        cmdRestoreMountFuseLocal = (struct CmdRestoreMountFuseLocal)
        {
            .manifest = manifest,
            .storage = storage,
            .label = label,
        };

        // cmdRestoreMountFuseLocal.pathList = lstNewP(sizeof(CmdRestoreMountFusePath), .comparator = lstComparatorStr);

        // for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        // {
        //     const ManifestPath *const path = manifestPath(manifest, pathIdx);

        //     if (strBeginsWithZ(path->name, "pg_data/"))
        //     {
        //         if (strstr(strZ(path->name) + 8, "/") == NULL)
        //             filler(buf, strBaseZ(path->name), NULL, 0, 0);
        //     }
        // }

        fuse_main(args.argc, args.argv, &restoreMountFuseOper, NULL);
        fuse_opt_free_args(&args);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
