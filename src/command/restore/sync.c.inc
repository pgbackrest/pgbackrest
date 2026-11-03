/***********************************************************************************************************************************
Restore Filesystem Synchronization

Before workers write, retain one descriptor for every destination filesystem. After workers finish, synchronize every retained
filesystem before publishing pg_control. Destination mounts and links must remain unchanged throughout the restore.
***********************************************************************************************************************************/
#ifdef HAVE_SYNCFS

#define FUNCTION_LOG_RESTORE_SYNC_TYPE                                                                                             \
    RestoreSync *
#define FUNCTION_LOG_RESTORE_SYNC_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "RestoreSync", buffer, bufferSize)

typedef struct RestoreSyncFileSystem
{
    dev_t device;                                                   // Device used to identify the filesystem
    int fd;                                                         // Descriptor retained until synchronization completes
    const String *path;                                             // Target path for error reporting
} RestoreSyncFileSystem;

typedef struct RestoreSync
{
    List *fileSystemList;                                           // One descriptor per destination filesystem
    int pendingFd;                                                  // Descriptor being added to the list
} RestoreSync;

/***********************************************************************************************************************************
Release descriptors on error without masking the original error or synchronizing incomplete work
***********************************************************************************************************************************/
static void
restoreSyncFreeResource(void *const data)
{
    RestoreSync *const this = data;

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, this);
    FUNCTION_TEST_END();

    if (this->pendingFd != -1)
        (void)close(this->pendingFd);

    for (unsigned int fileSystemIdx = 0; fileSystemIdx < lstSize(this->fileSystemList); fileSystemIdx++)
    {
        const RestoreSyncFileSystem *const fileSystem = lstGet(this->fileSystemList, fileSystemIdx);

        if (fileSystem->fd != -1)
            (void)close(fileSystem->fd);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Retain a descriptor for a destination filesystem, closing duplicate descriptors immediately
***********************************************************************************************************************************/
static void
restoreSyncPathAdd(RestoreSync *const this, const String *const path, const bool file)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(RESTORE_SYNC, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, file);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    this->pendingFd = open(strZ(path), O_RDONLY | O_CLOEXEC | (file ? 0 : O_DIRECTORY), 0);

    // Force restore can overwrite a file that is writable but not readable. Do not require more access than the worker needs.
    if (file && this->pendingFd == -1 && errno == EACCES)
        this->pendingFd = open(strZ(path), O_WRONLY | O_CLOEXEC, 0);

    THROW_ON_SYS_ERROR_FMT(this->pendingFd == -1, PathOpenError, "unable to open path '%s' for synchronization", strZ(path));

    struct stat info;
    THROW_ON_SYS_ERROR_FMT(
        fstat(this->pendingFd, &info) == -1, FileInfoError, "unable to get info for path '%s'", strZ(path));

    bool found = false;

    for (unsigned int fileSystemIdx = 0; fileSystemIdx < lstSize(this->fileSystemList); fileSystemIdx++)
    {
        if (((RestoreSyncFileSystem *)lstGet(this->fileSystemList, fileSystemIdx))->device == info.st_dev)
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        const int fd = this->pendingFd;
        this->pendingFd = -1;
        THROW_ON_SYS_ERROR_FMT(close(fd) == -1, FileCloseError, "unable to close path '%s'", strZ(path));
    }
    else
    {
        MEM_CONTEXT_OBJ_BEGIN(this->fileSystemList)
        {
            lstAdd(
                this->fileSystemList, &(RestoreSyncFileSystem){.device = info.st_dev, .fd = this->pendingFd, .path = strDup(path)});
        }
        MEM_CONTEXT_OBJ_END();

        this->pendingFd = -1;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Cover the effective destination of a file link. A delta target may already be a link itself, including a dangling link whose file
will be created on another filesystem. An existing regular file may also be a mount point, so retain its descriptor as well as the
parent. Only manifest file targets are inspected, not every restored file.
***********************************************************************************************************************************/
static void
restoreSyncFileAdd(RestoreSync *const this, const String *const path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(RESTORE_SYNC, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *file = path;
        unsigned int linkTotal = 0;

        while (true)
        {
            const StorageInfo info = storageInfoP(storageLocal(), file, .ignoreMissing = true);

            if (!info.exists || info.type != storageTypeLink)
            {
                // The original parent has already been retained. Open a different effective parent even when the file is missing.
                const String *const parent = strPath(file);

                if (!strEq(parent, strPath(path)))
                    restoreSyncPathAdd(this, parent, false);

                if (info.exists)
                {
                    CHECK_FMT(
                        LinkDestinationError, info.type == storageTypeFile,
                        "file link destination '%s' is not a regular file", strZ(file));
                    restoreSyncPathAdd(this, file, true);
                }

                break;
            }

            CHECK_FMT(LinkDestinationError, linkTotal++ < 40, "too many links in file link destination '%s'", strZ(path));

            // Preserve relative path components so directory links followed by '..' have the same meaning as the worker's open().
            file = strBeginsWith(info.linkDestination, FSLASH_STR) ? info.linkDestination :
                       strNewFmt("%s/%s", strZ(strPath(file)), strZ(info.linkDestination));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Decode a mount path from the Linux mount inventory
***********************************************************************************************************************************/
static String *
restoreSyncMountPathDecode(const String *const encodedMountPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, encodedMountPath);
    FUNCTION_LOG_END();

    ASSERT(encodedMountPath != NULL);

    String *const mountPath = strNew();

    for (size_t idx = 0; idx < strSize(encodedMountPath); idx++)
    {
        unsigned char chr = (unsigned char)strZ(encodedMountPath)[idx];

        if (chr == '\\')
        {
            CHECK(FormatError, idx + 3 < strSize(encodedMountPath), "invalid restore mount path escape");
            unsigned int value = 0;

            for (unsigned int digitIdx = 0; digitIdx < 3; digitIdx++)
            {
                const char digit = strZ(encodedMountPath)[++idx];
                CHECK(FormatError, digit >= '0' && digit <= '7', "invalid restore mount path escape");
                value = value * 8 + (unsigned int)(digit - '0');
            }

            CHECK(FormatError, value > 0 && value <= 255, "invalid restore mount path escape");
            chr = (unsigned char)value;
        }

        strCatChr(mountPath, (char)chr);
    }

    CHECK(FormatError, strBeginsWith(mountPath, FSLASH_STR), "restore mount path is not absolute");

    FUNCTION_LOG_RETURN(STRING, mountPath);
}

/***********************************************************************************************************************************
Cover regular files mounted inside a destination directory. These are not represented as manifest targets. Inspecting the Linux
mount inventory avoids inspecting every manifest file. Directory mounts are already covered by the manifest paths.
***********************************************************************************************************************************/
static void
restoreSyncFileMountAdd(RestoreSync *const this, const StringList *const restoreRootList, const String *const mountInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(RESTORE_SYNC, this);
        FUNCTION_TEST_PARAM(STRING_LIST, restoreRootList);
        FUNCTION_TEST_PARAM(STRING, mountInfo);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(restoreRootList != NULL);
    ASSERT(mountInfo != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const lines = strLstNewSplitZ(mountInfo, "\n");
        unsigned int mountTotal = 0;

        for (unsigned int lineIdx = 0; lineIdx < strLstSize(lines); lineIdx++)
        {
            const String *const line = strLstGet(lines, lineIdx);

            if (strSize(line) == 0)
                continue;

            MEM_CONTEXT_TEMP_BEGIN()
            {
                const StringList *const fields = strLstNewSplitZ(line, " ");
                CHECK(
                    FormatError, strLstSize(fields) >= 10 && strEqZ(strLstGet(fields, strLstSize(fields) - 4), "-"),
                    "invalid restore mount info");
                mountTotal++;

                const String *const mountPath = restoreSyncMountPathDecode(strLstGet(fields, 4));

                for (unsigned int pathIdx = 0; pathIdx < strLstSize(restoreRootList); pathIdx++)
                {
                    const String *const restoreRoot = strLstGet(restoreRootList, pathIdx);

                    if (strEq(restoreRoot, FSLASH_STR) ||
                        (strBeginsWith(mountPath, restoreRoot) &&
                         (strSize(mountPath) == strSize(restoreRoot) || strZ(mountPath)[strSize(restoreRoot)] == '/')))
                    {
                        // An overmounted entry may no longer be visible. Inspect the visible object that the restore will actually
                        // open.
                        const StorageInfo info = storageInfoP(storageLocal(), mountPath, .ignoreMissing = true);

                        if (info.exists && info.type == storageTypeFile)
                            restoreSyncPathAdd(this, mountPath, true);

                        break;
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        CHECK(FormatError, mountTotal > 0, "restore mount info is empty");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Build the PostgreSQL version directory within a tablespace
***********************************************************************************************************************************/
static String *
restoreSyncTablespacePath(const Manifest *const manifest, const String *const targetPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, targetPath);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);
    ASSERT(targetPath != NULL);

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const tablespaceId =
            pgTablespaceId(manifestData(manifest)->pgVersion, manifestData(manifest)->pgCatalogVersion);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewFmt("%s/%s", strZ(targetPath), strZ(tablespaceId));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Resolve the data roots that may contain file mounts. Tablespace roots can contain data for other PostgreSQL versions, and their
version subdirectories can be links, so resolve only the version being restored. File targets have their own descriptors.
***********************************************************************************************************************************/
static StringList *
restoreSyncMountRootList(const Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    StringList *const result = strLstNew();

    for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
    {
        const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

        if (target->file == NULL)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                const String *path = manifestTargetPath(manifest, target);

                if (target->tablespaceId != 0)
                    path = restoreSyncTablespacePath(manifest, path);

                char canonical[PATH_MAX];
                THROW_ON_SYS_ERROR_FMT(
                    realpath(strZ(path), canonical) == NULL, PathOpenError,
                    "unable to resolve restore path '%s' for mount discovery", strZ(path));
                strLstAddIfMissing(result, strNewZ(canonical));
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Open destination paths before restore jobs write data. Keeping the descriptor open allows syncfs() to report writeback errors since
it was opened. File links cover both their parent directory and effective file destination. Multiple targets on one filesystem need
only one descriptor.
***********************************************************************************************************************************/
static RestoreSync *
restoreSyncNew(const Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    OBJ_NEW_BEGIN(RestoreSync, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (RestoreSync){.fileSystemList = lstNewP(sizeof(RestoreSyncFileSystem)), .pendingFd = -1};
        memContextCallbackSet(objMemContext(this), restoreSyncFreeResource, this);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Manifest targets cover data, tablespaces and mapped destinations, including effective file-link destinations
            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    const ManifestTarget *const target = manifestTarget(manifest, targetIdx);
                    const String *const targetPath = manifestTargetPath(manifest, target);

                    restoreSyncPathAdd(this, targetPath, false);

                    // Tablespace contents may be mounted separately from the root metadata. Cover both filesystems.
                    if (target->tablespaceId != 0)
                        restoreSyncPathAdd(this, restoreSyncTablespacePath(manifest, targetPath), false);

                    if (target->file != NULL)
                        restoreSyncFileAdd(this, strNewFmt("%s/%s", strZ(targetPath), strZ(target->file)));
                }
                MEM_CONTEXT_TEMP_END();
            }

            // Manifest directories cover mounts without a separate target, e.g. directly mounted pg_wal. This checks directories,
            // not individual restored files.
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    const String *const name = manifestPath(manifest, pathIdx)->name;

                    if (!strEq(name, MANIFEST_TARGET_PGTBLSPC_STR))
                        restoreSyncPathAdd(this, storagePathP(storagePg(), manifestPathPg(name)), false);
                }
                MEM_CONTEXT_TEMP_END();
            }

            // Existing file mounts need the mount inventory: delta/force can overwrite them without a manifest target. Fresh
            // restore rejects existing ordinary files.
            if (cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce))
            {
                restoreSyncFileMountAdd(
                    this, restoreSyncMountRootList(manifest),
                    strNewBuf(storageGetP(storageNewReadP(storageLocal(), STRDEF("/proc/self/mountinfo")))));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(RESTORE_SYNC, this);
}

/***********************************************************************************************************************************
Synchronize all destination filesystems before pg_control is published. A failure must abort restore, even if earlier filesystems
were synchronized successfully. Close descriptors only after every barrier succeeds, with error cleanup handling any remaining ones.
***********************************************************************************************************************************/
static void
restoreFileSystemSync(RestoreSync *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(RESTORE_SYNC, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    for (unsigned int fileSystemIdx = 0; fileSystemIdx < lstSize(this->fileSystemList); fileSystemIdx++)
    {
        const RestoreSyncFileSystem *const fileSystem = lstGet(this->fileSystemList, fileSystemIdx);

        LOG_DETAIL_FMT("sync filesystem containing '%s'", strZ(fileSystem->path));
        THROW_ON_SYS_ERROR_FMT(
            syncfs(fileSystem->fd) == -1, FileSyncError, "unable to sync filesystem containing '%s'", strZ(fileSystem->path));
    }

    for (unsigned int fileSystemIdx = 0; fileSystemIdx < lstSize(this->fileSystemList); fileSystemIdx++)
    {
        RestoreSyncFileSystem *const fileSystem = lstGet(this->fileSystemList, fileSystemIdx);
        const int fd = fileSystem->fd;
        fileSystem->fd = -1;
        THROW_ON_SYS_ERROR_FMT(close(fd) == -1, FileCloseError, "unable to close path '%s'", strZ(fileSystem->path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

#endif
