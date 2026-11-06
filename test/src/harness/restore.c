/***********************************************************************************************************************************
Harness for Restore Testing
***********************************************************************************************************************************/
#include <build.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Include the coordinator headers before syscall macros so inline interface methods are not rewritten
#include "command/lock.h"
#include "command/restore/protocol.h"
#include "command/restore/restore.h"
#include "command/restore/timeline.h"
#include "common/regExp.h"
#include "common/user.h"
#include "config/config.h"
#include "config/exec.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

#include "harness/debug.h"
#include "harness/restore.h"
#include "harness/test.intern.h"

/***********************************************************************************************************************************
Observe the real writer's synchronization setting for one destination, without intercepting fixture or repository writes
***********************************************************************************************************************************/
static struct
{
    const char *file;
    bool syncFile;
    int fd;                                                         // Shared observation file inherited by local worker processes
} hrnRestoreWriteSyncLocal = {.fd = -1};

void
hrnRestoreWriteSyncSet(const char *const file, const bool syncFile)
{
    if (hrnRestoreWriteSyncLocal.fd != -1)
    {
        const int fd = hrnRestoreWriteSyncLocal.fd;
        hrnRestoreWriteSyncLocal.fd = -1;
        THROW_ON_SYS_ERROR(close(fd) == -1, FileCloseError, "unable to close writer observations");
    }

    hrnRestoreWriteSyncLocal.file = file;
    hrnRestoreWriteSyncLocal.syncFile = syncFile;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const path = strNewFmt("%s/restore-write-sync", hrnPath());

        if (file != NULL)
        {
            hrnRestoreWriteSyncLocal.fd = open(strZ(path), O_CREAT | O_TRUNC | O_RDWR | O_APPEND, 0600);
            THROW_ON_SYS_ERROR(hrnRestoreWriteSyncLocal.fd == -1, FileOpenError, "unable to open writer observations");
        }
        else
        {
            THROW_ON_SYS_ERROR(
                unlink(strZ(path)) == -1 && errno != ENOENT, FileRemoveError, "unable to remove writer observations");
        }
    }
    MEM_CONTEXT_TEMP_END();
}

unsigned int
hrnRestoreWriteSyncTotal(void)
{
    struct stat info;
    THROW_ON_SYS_ERROR(fstat(hrnRestoreWriteSyncLocal.fd, &info) == -1, FileInfoError, "unable to read writer observations");
    return (unsigned int)info.st_size;
}

static StorageWrite *
hrnRestoreNewWrite(const Storage *const storage, const String *const file, const StorageNewWriteParam param)
{
    StorageWrite *const result = storageNewWrite(storage, file, param);

    if (hrnRestoreWriteSyncLocal.file != NULL && strEqZ(storageWriteName(result), hrnRestoreWriteSyncLocal.file))
    {
        CHECK(
            AssertError, storageWriteSyncFile(result) == hrnRestoreWriteSyncLocal.syncFile,
            "restore destination writer synchronization is incorrect");
        THROW_ON_SYS_ERROR(
            write(hrnRestoreWriteSyncLocal.fd, "1", 1) != 1, FileWriteError, "unable to record writer observation");
    }

    return result;
}

/***********************************************************************************************************************************
Filesystem syscall script. Default behavior is the real syscall; only the included restore coordinator is intercepted.
***********************************************************************************************************************************/
#ifdef HAVE_SYNCFS

static struct
{
    bool installed;
    HrnRestoreSyncScript script[128];
    unsigned int size;
    unsigned int index;
} hrnRestoreSyncLocal;

static const HrnRestoreSyncScript *
hrnRestoreSyncScriptNext(const HrnRestoreSyncOp op, const int fd, const char *const path)
{
    CHECK(AssertError, hrnRestoreSyncLocal.index < hrnRestoreSyncLocal.size, "unexpected restore filesystem operation");
    const HrnRestoreSyncScript *const script = &hrnRestoreSyncLocal.script[hrnRestoreSyncLocal.index++];

    CHECK_FMT(AssertError, script->op == op, "expected restore filesystem operation %u but got %u", script->op, op);

    if (op == hrnRestoreSyncOpOpen)
    {
        CHECK_FMT(
            AssertError, strcmp(script->path, path) == 0, "expected restore filesystem path '%s' but got '%s'", script->path, path);
    }
    else
        CHECK_FMT(AssertError, script->fd == fd, "expected restore filesystem descriptor %d but got %d", script->fd, fd);

    if (script->callback != NULL)
        script->callback();

    errno = script->errNo;
    return script;
}

static int
hrnRestoreSyncOpen(const char *const path, const int flags, const mode_t mode)
{
    if (!hrnRestoreSyncLocal.installed)
        return open(path, flags, mode);

    const HrnRestoreSyncScript *const script = hrnRestoreSyncScriptNext(hrnRestoreSyncOpOpen, -1, path);
    CHECK(
        AssertError, flags == ((script->writeOnly ? O_WRONLY : O_RDONLY) | O_CLOEXEC | (script->file ? 0 : O_DIRECTORY)),
        "restore filesystem descriptor flags are incorrect");
    return script->errNo == 0 ? script->fd : -1;
}

static int
hrnRestoreSyncStat(const int fd, struct stat *const info)
{
    if (!hrnRestoreSyncLocal.installed)
        return fstat(fd, info);

    const HrnRestoreSyncScript *const script = hrnRestoreSyncScriptNext(hrnRestoreSyncOpStat, fd, NULL);
    *info = (struct stat){.st_dev = script->device, .st_mode = (script->file ? S_IFREG : S_IFDIR) | 0700};
    return script->errNo == 0 ? 0 : -1;
}

static int
hrnRestoreSync(const int fd)
{
    if (!hrnRestoreSyncLocal.installed)
        return syncfs(fd);

    return hrnRestoreSyncScriptNext(hrnRestoreSyncOpSync, fd, NULL)->errNo == 0 ? 0 : -1;
}

static int
hrnRestoreSyncClose(const int fd)
{
    if (!hrnRestoreSyncLocal.installed)
        return close(fd);

    return hrnRestoreSyncScriptNext(hrnRestoreSyncOpClose, fd, NULL)->errNo == 0 ? 0 : -1;
}

/**********************************************************************************************************************************/
void
hrnRestoreSyncScriptSet(const HrnRestoreSyncScript *const script, const unsigned int scriptSize)
{
    FUNCTION_HARNESS_VOID();

    CHECK(
        AssertError, scriptSize > 0 && scriptSize <= LENGTH_OF(hrnRestoreSyncLocal.script),
        "invalid restore filesystem script size");
    CHECK(
        AssertError, !hrnRestoreSyncLocal.installed || hrnRestoreSyncLocal.index == hrnRestoreSyncLocal.size,
        "previous restore filesystem script is incomplete");

    memcpy(hrnRestoreSyncLocal.script, script, sizeof(*script) * scriptSize);
    hrnRestoreSyncLocal.size = scriptSize;
    hrnRestoreSyncLocal.index = 0;
    hrnRestoreSyncLocal.installed = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
hrnRestoreSyncScriptDone(void)
{
    return hrnRestoreSyncLocal.installed && hrnRestoreSyncLocal.index == hrnRestoreSyncLocal.size;
}

/**********************************************************************************************************************************/
void
hrnRestoreSyncScriptClear(void)
{
    memset(&hrnRestoreSyncLocal, 0, sizeof(hrnRestoreSyncLocal));
}

#define open(...)                                                  hrnRestoreSyncOpen(__VA_ARGS__)
#define fstat(...)                                                 hrnRestoreSyncStat(__VA_ARGS__)
#define syncfs(...)                                                hrnRestoreSync(__VA_ARGS__)
#define close(...)                                                 hrnRestoreSyncClose(__VA_ARGS__)

#endif

/***********************************************************************************************************************************
Include the real restore implementation with only individual filesystem calls intercepted
***********************************************************************************************************************************/
#define storageNewWrite(...)                                       hrnRestoreNewWrite(__VA_ARGS__)

{[SHIM_MODULE]}

#undef storageNewWrite

#ifdef HAVE_SYNCFS
#undef open
#undef fstat
#undef syncfs
#undef close
#endif

/**********************************************************************************************************************************/
void
hrnCmdRestore(void)
{
    FUNCTION_HARNESS_VOID();

    lockInit(STR(testPath()), cfgOptionStr(cfgOptExecId));
    cmdLockAcquireP();

    TRY_BEGIN()
    {
        cmdRestore();
    }
    FINALLY()
    {
        cmdLockReleaseP();
    }
    TRY_END();

    FUNCTION_HARNESS_RETURN_VOID();
}
