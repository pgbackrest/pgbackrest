/***********************************************************************************************************************************
Restore Filesystem Synchronization

Before workers write, retain one descriptor for every filesystem containing a restored path. After workers finish, synchronize every
retained filesystem before publishing pg_control. Destination mounts and links must remain unchanged throughout the restore.
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
Retain a descriptor for the filesystem containing a path, closing duplicate descriptors immediately
***********************************************************************************************************************************/
static void
restoreSyncPathAdd(RestoreSync *const this, const String *const path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(RESTORE_SYNC, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    this->pendingFd = open(strZ(path), O_RDONLY | O_CLOEXEC | O_DIRECTORY, 0);
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
Open restored paths before restore jobs write data. Keeping the descriptor open allows syncfs() to report writeback errors since it
was opened. Multiple paths on one filesystem need only one descriptor.
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

        // Manifest paths include the data directory, tablespaces, path links and directly mounted paths, e.g. pg_wal
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                const String *const name = manifestPath(manifest, pathIdx)->name;

                if (!strEq(name, MANIFEST_TARGET_PGTBLSPC_STR))
                    restoreSyncPathAdd(this, storagePathP(storagePg(), manifestPathPg(name)));
            }
            MEM_CONTEXT_TEMP_END();
        }
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
