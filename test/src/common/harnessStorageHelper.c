/***********************************************************************************************************************************
Storage Helper Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "storage/posix/read.h"
#include "storage/posix/write.h"
#include "storage/storage.h"

#include "common/harnessDebug.h"
#include "common/harnessStorage.h"
#include "common/harnessStorageHelper.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct HrnStorageTest
{
    STORAGE_COMMON_MEMBER;
    Storage *storagePosix;                                          // Posix storage
} HrnStorageTest;

typedef struct HrnStorageWriteTest
{
    StorageWriteInterface interface;                                // Interface
    IoWrite *base;                                                  // Posix IO for base file
    IoWrite *version;                                               // Posix IO for version file
} HrnStorageWriteTest;

typedef struct HrnStorageReadTest
{
    StorageReadInterface interface;                                 // Interface
    IoRead *posix;                                                  // Posix IO for the file
    bool version;                                                   // Load version?
    const String *versionId;                                        // Version to load
} HrnStorageReadTest;

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct HrnStorageHelperLocal
{
    bool shim;                                                      // Is the storage being shimmed?
} hrnStorageHelperLocal;

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define HRN_STORAGE_TEST_SECRET                                     ".pgbfs"
#define STORAGE_TEST_TYPE                                           STRID5("test", 0xa4cb40)

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HRN_STORAGE_TEST_TYPE                                                                                         \
    HrnStorageTest *
#define FUNCTION_LOG_HRN_STORAGE_TEST_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "HrnStorageTest *", buffer, bufferSize)

#define FUNCTION_LOG_HRN_STORAGE_READ_TEST_TYPE                                                                                    \
    HrnStorageReadTest *
#define FUNCTION_LOG_HRN_STORAGE_READ_TEST_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(value, "HrnStorageReadTest *", buffer, bufferSize)

#define FUNCTION_LOG_HRN_STORAGE_WRITE_TEST_TYPE                                                                                   \
    HrnStorageWriteTest *
#define FUNCTION_LOG_HRN_STORAGE_WRITE_TEST_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(value, "HrnStorageWriteTest *", buffer, bufferSize)

/***********************************************************************************************************************************
Error if secret path is specified
***********************************************************************************************************************************/
static void
hrnStorageTestSecretCheck(const String *const file)
{
    if (strstr(strZ(file), HRN_STORAGE_TEST_SECRET) != NULL)
        THROW_FMT(AssertError, "path/file '%s' cannot contain " HRN_STORAGE_TEST_SECRET, strZ(file));
}

/***********************************************************************************************************************************
Find an unused file version
***********************************************************************************************************************************/
static String *
hrnStorageTestVersionFind(const Storage *const storage, const String *const file)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, file);
    FUNCTION_HARNESS_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *version;

        for (unsigned int versionId = 1;; versionId++)
        {
            version = strNewFmt("%s/" HRN_STORAGE_TEST_SECRET "/%s/v%04u", strZ(strPath(file)), strZ(strBase(file)), versionId);

            if (!storageInfoP(storage, version, .ignoreMissing = true).exists &&
                !storageInfoP(storage, strNewFmt("%s.delete", strZ(version)), .ignoreMissing = true).exists)
            {
                break;
            }
        }

        strCat(result, version);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Test storage read driver
***********************************************************************************************************************************/
static bool
hrnStorageReadTestOpen(THIS_VOID)
{
    THIS(HrnStorageReadTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_READ_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    // If the version is missing
    if (this->version && this->versionId == NULL)
        FUNCTION_HARNESS_RETURN(BOOL, false);

    FUNCTION_HARNESS_RETURN(BOOL, ioReadInterface(this->posix)->open(ioReadDriver(this->posix)));
}

static size_t
hrnStorageReadTest(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(HrnStorageReadTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_READ_TEST, this);
        FUNCTION_HARNESS_PARAM(BUFFER, buffer);
        FUNCTION_HARNESS_PARAM(BOOL, block);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_HARNESS_RETURN(SIZE, ioReadInterface(this->posix)->read(ioReadDriver(this->posix), buffer, block));
}

static void
hrnStorageReadTestClose(THIS_VOID)
{
    THIS(HrnStorageReadTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_READ_TEST, this);
    FUNCTION_HARNESS_END();

    ioReadInterface(this->posix)->close(ioReadDriver(this->posix));

    FUNCTION_HARNESS_RETURN_VOID();
}

static bool
hrnStorageReadTestEof(THIS_VOID)
{
    THIS(HrnStorageReadTest);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HRN_STORAGE_READ_TEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, ioReadInterface(this->posix)->eof(ioReadDriver(this->posix)));
}

static int
hrnStorageReadTestFd(const THIS_VOID)
{
    THIS(const HrnStorageReadTest);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HRN_STORAGE_READ_TEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, ioReadInterface(this->posix)->fd(ioReadDriver(this->posix)));
}

static StorageRead *
hrnStorageReadTestNew(
    StoragePosix *const storage, const String *name, const bool ignoreMissing, const uint64_t offset,
    const Variant *const limit, const bool version, const String *const versionId)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, name);
        FUNCTION_HARNESS_PARAM(BOOL, ignoreMissing);
        FUNCTION_HARNESS_PARAM(UINT64, offset);
        FUNCTION_HARNESS_PARAM(VARIANT, limit);
        FUNCTION_HARNESS_PARAM(BOOL, version);
        FUNCTION_HARNESS_PARAM(STRING, versionId);
    FUNCTION_HARNESS_END();

    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(HrnStorageReadTest, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        // Update file name when versionId is set
        if (versionId)
            name = strNewFmt("%s/" HRN_STORAGE_TEST_SECRET "/%s/%s", strZ(strPath(name)), strZ(strBase(name)), strZ(versionId));

        StorageRead *const posix = storageReadPosixNew(storage, name, ignoreMissing, offset, limit);

        // Copy the interface and update with our functions
        StorageReadInterface interface = *storageReadInterface(posix);
        interface.ioInterface.close = hrnStorageReadTestClose;
        interface.ioInterface.eof = hrnStorageReadTestEof;
        interface.ioInterface.fd = hrnStorageReadTestFd;
        interface.ioInterface.open = hrnStorageReadTestOpen;
        interface.ioInterface.read = hrnStorageReadTest;

        *this = (HrnStorageReadTest)
        {
            .interface = interface,
            .posix = storageReadIo(posix),
            .version = version,
            .versionId = strDup(versionId),
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(STORAGE_READ, storageReadNew(this, &this->interface));
}

/***********************************************************************************************************************************
Test storage write driver
***********************************************************************************************************************************/
static void
hrnStorageWriteTestOpen(THIS_VOID)
{
    THIS(HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    ioWriteInterface(this->base)->open(ioWriteDriver(this->base));
    ioWriteInterface(this->version)->open(ioWriteDriver(this->version));

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
hrnStorageWriteTest(THIS_VOID, const Buffer *const buffer)
{
    THIS(HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
        FUNCTION_HARNESS_PARAM(BUFFER, buffer);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    ioWriteInterface(this->base)->write(ioWriteDriver(this->base), buffer);
    ioWriteInterface(this->version)->write(ioWriteDriver(this->version), buffer);

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
hrnStorageWriteTestClose(THIS_VOID)
{
    THIS(HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    ioWriteInterface(this->base)->close(ioWriteDriver(this->base));
    ioWriteInterface(this->version)->close(ioWriteDriver(this->version));

    FUNCTION_HARNESS_RETURN_VOID();
}

static int
hrnStorageWriteTestFd(const THIS_VOID)
{
    THIS(const HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    FUNCTION_HARNESS_RETURN(INT, ioWriteInterface(this->base)->fd(ioWriteDriver(this->base)));
}

static StorageWrite *
hrnStorageWriteTestNew(
    Storage *const storagePosix, const String *const name, const mode_t modeFile, const mode_t modePath,
    const String *const user, const String *const group, time_t timeModified, const bool createPath, const bool syncFile,
    const bool syncPath, const bool atomic, const bool truncate)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE_POSIX, storagePosix);
        FUNCTION_HARNESS_PARAM(STRING, name);
        FUNCTION_HARNESS_PARAM(MODE, modeFile);
        FUNCTION_HARNESS_PARAM(MODE, modePath);
        FUNCTION_HARNESS_PARAM(STRING, user);
        FUNCTION_HARNESS_PARAM(STRING, group);
        FUNCTION_HARNESS_PARAM(TIME, timeModified);
        FUNCTION_HARNESS_PARAM(BOOL, createPath);
        FUNCTION_HARNESS_PARAM(BOOL, syncFile);
        FUNCTION_HARNESS_PARAM(BOOL, syncPath);
        FUNCTION_HARNESS_PARAM(BOOL, atomic);
        FUNCTION_HARNESS_PARAM(BOOL, truncate);
    FUNCTION_HARNESS_END();

    ASSERT(storagePosix != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);
    ASSERT(truncate);
    ASSERT(createPath);

    OBJ_NEW_BEGIN(HrnStorageWriteTest, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        // Make sure the main file and the version both have the same timestamp
        if (timeModified == 0)
            timeModified = (time_t)(timeMSec() / MSEC_PER_SEC);

        StorageWrite *const posix = storageWritePosixNew(
            storageDriver(storagePosix), name, modeFile, modePath, user, group, timeModified, createPath, false, false, false,
            truncate);

        // Copy the interface and update with our functions
        StorageWriteInterface interface = *storageWriteInterface(posix);
        interface.ioInterface.close = hrnStorageWriteTestClose;
        interface.ioInterface.fd = hrnStorageWriteTestFd;
        interface.ioInterface.open = hrnStorageWriteTestOpen;
        interface.ioInterface.write = hrnStorageWriteTest;

        *this = (HrnStorageWriteTest)
        {
            .interface = interface,
            .base = storageWriteIo(posix),
            .version = storageWriteIo(
                storageWritePosixNew(
                    storageDriver(storagePosix), hrnStorageTestVersionFind(storagePosix, name), modeFile, modePath, user, group,
                    timeModified, createPath, false, false, false, truncate)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(STORAGE_WRITE, storageWriteNew(this, &this->interface));
}

/***********************************************************************************************************************************
Test storage driver
***********************************************************************************************************************************/
static StorageInfo
hrnStorageTestInfo(THIS_VOID, const String *const file, const StorageInfoLevel level, const StorageInterfaceInfoParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, file);
        FUNCTION_HARNESS_PARAM(ENUM, level);
        FUNCTION_HARNESS_PARAM(BOOL, param.followLink);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    hrnStorageTestSecretCheck(file);

    FUNCTION_HARNESS_RETURN(STORAGE_INFO, hrnStorageInterfaceDummy.info(storageDriver(this->storagePosix), file, level, param));
}

static StorageList *
hrnStorageTestList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(ENUM, level);
        FUNCTION_HARNESS_PARAM(TIME, param.targetTime);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    hrnStorageTestSecretCheck(path);

    StorageList *result = NULL;
    const StorageList *const list = storageInterfaceListP(storageDriver(this->storagePosix), path, level);

    if (list != NULL)
    {
        result = storageLstNew(level);

        if (param.targetTime != 0)
        {
            // Get just the paths
            for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
            {
                const StorageInfo info = storageLstGet(list, listIdx);

                if (strstr(strZ(info.name), HRN_STORAGE_TEST_SECRET) != NULL || info.type != storageTypePath)
                    continue;

                storageLstAdd(result, &info);
            }

            // Get file versions
            const StorageList *const list = storageInterfaceListP(
                storageDriver(this->storagePosix), strNewFmt("%s/" HRN_STORAGE_TEST_SECRET, strZ(path)), level);

            if (list != NULL)
            {
                for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
                {
                    const StorageInfo info = storageLstGet(list, listIdx);
                    StorageList *const versionList =
                        storageInterfaceListP(
                            storageDriver(this->storagePosix),
                            strNewFmt("%s/" HRN_STORAGE_TEST_SECRET "/%s", strZ(path), strZ(info.name)), level);
                    storageLstSort(versionList, sortOrderDesc);

                    for (unsigned int versionIdx = 0; versionIdx < storageLstSize(versionList); versionIdx++)
                    {
                        StorageInfo versionInfo = storageLstGet(versionList, versionIdx);

                        // Return version if within the time limit
                        if (versionInfo.timeModified <= param.targetTime)
                        {
                            // If the most recent version is a delete marker then skip the file
                            if (strEndsWithZ(versionInfo.name, ".delete"))
                                break;

                            // Return the version
                            versionInfo.versionId = versionInfo.name;
                            versionInfo.name = info.name;

                            storageLstAdd(result, &versionInfo);
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            // Return everything except the contents of the secret path
            for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
            {
                const StorageInfo info = storageLstGet(list, listIdx);

                if (strstr(strZ(info.name), HRN_STORAGE_TEST_SECRET) != NULL)
                    continue;

                storageLstAdd(result, &info);
            }
        }
    }

    FUNCTION_HARNESS_RETURN(STORAGE_LIST, result);
}

static StorageRead *
hrnStorageTestNewRead(THIS_VOID, const String *file, const bool ignoreMissing, const StorageInterfaceNewReadParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, file);
        FUNCTION_HARNESS_PARAM(BOOL, ignoreMissing);
        FUNCTION_HARNESS_PARAM(UINT64, param.offset);
        FUNCTION_HARNESS_PARAM(VARIANT, param.limit);
        FUNCTION_HARNESS_PARAM(BOOL, param.version);
        FUNCTION_HARNESS_PARAM(STRING, param.versionId);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    hrnStorageTestSecretCheck(file);

    FUNCTION_HARNESS_RETURN(
        STORAGE_READ,
        hrnStorageReadTestNew(
            storageDriver(this->storagePosix), file, ignoreMissing, param.offset, param.limit, param.version, param.versionId));
}

static StorageWrite *
hrnStorageTestNewWrite(THIS_VOID, const String *const file, const StorageInterfaceNewWriteParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, file);
        FUNCTION_HARNESS_PARAM(MODE, param.modeFile);
        FUNCTION_HARNESS_PARAM(MODE, param.modePath);
        FUNCTION_HARNESS_PARAM(STRING, param.user);
        FUNCTION_HARNESS_PARAM(STRING, param.group);
        FUNCTION_HARNESS_PARAM(TIME, param.timeModified);
        FUNCTION_HARNESS_PARAM(BOOL, param.createPath);
        FUNCTION_HARNESS_PARAM(BOOL, param.syncFile);
        FUNCTION_HARNESS_PARAM(BOOL, param.syncPath);
        FUNCTION_HARNESS_PARAM(BOOL, param.atomic);
        FUNCTION_HARNESS_PARAM(BOOL, param.truncate);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    hrnStorageTestSecretCheck(file);

    FUNCTION_HARNESS_RETURN(
        STORAGE_WRITE,
        hrnStorageWriteTestNew(
            this->storagePosix, file, param.modeFile, param.modePath, param.user, param.group, param.timeModified,
            param.createPath, param.syncFile, param.syncPath, param.atomic, param.truncate));
}

static void
hrnStorageTestPathCreate(
    THIS_VOID, const String *const path, const bool errorOnExists, const bool noParentCreate, const mode_t mode,
    const StorageInterfacePathCreateParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE_POSIX, this);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(BOOL, errorOnExists);
        FUNCTION_HARNESS_PARAM(BOOL, noParentCreate);
        FUNCTION_HARNESS_PARAM(MODE, mode);
        (void)param;                                                // No parameters are used
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    storageInterface(this->storagePosix).pathCreate(
        storageDriver(this->storagePosix), path, errorOnExists, noParentCreate, mode, param);

    FUNCTION_HARNESS_RETURN_VOID();
}

static bool
hrnStorageTestPathRemove(THIS_VOID, const String *const path, const bool recurse, const StorageInterfacePathRemoveParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    hrnStorageTestSecretCheck(path);

    // Get time to use for delete markers
    const time_t timeModified = (time_t)(timeMSec() / MSEC_PER_SEC);

    // Remove files recursively
    StorageIterator *const storageItr = storageNewItrP(
        this->storagePosix, path, .level = storageInfoLevelBasic, .nullOnMissing = true, .sortOrder = sortOrderAsc,
        .recurse = true);

    if (storageItr != NULL)
    {
        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Only proceed if file and not in the secret path
            if (info.type != storageTypeFile || strstr(strZ(info.name), HRN_STORAGE_TEST_SECRET) != NULL)
                continue;

            // Remove file
            const String *const file = strNewFmt("%s/%s", strZ(path), strZ(info.name));

            storageInterface(this->storagePosix).remove(
                storageDriver(this->storagePosix), file, (StorageInterfaceRemoveParam){.errorOnMissing = true});

            // Write delete marker
            storagePutP(
                storageNewWriteP(
                    this->storagePosix, strNewFmt("%s.delete", strZ(hrnStorageTestVersionFind(this->storagePosix, file))),
                    .timeModified = timeModified, .noAtomic = true, .noSyncFile = true, .noSyncPath = true),
                NULL);
        }
    }

    FUNCTION_HARNESS_RETURN(BOOL, storageItr != NULL);
}

static void
hrnStorageTestRemove(THIS_VOID, const String *const file, const StorageInterfaceRemoveParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, file);
        FUNCTION_HARNESS_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    hrnStorageTestSecretCheck(file);

    // Set modified time and determine if file exists
    const time_t timeModified = (time_t)(timeMSec() / MSEC_PER_SEC);
    const bool exists = storageExistsP(this->storagePosix, file);

    hrnStorageInterfaceDummy.remove(storageDriver(this->storagePosix), file, param);

    // If the file existed then write a delete marker
    if (exists)
    {
        storagePutP(
            storageNewWriteP(
                this->storagePosix, strNewFmt("%s.delete", strZ(hrnStorageTestVersionFind(this->storagePosix, file))),
                .timeModified = timeModified, .noAtomic = true, .noSyncFile = true, .noSyncPath = true),
            NULL);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

static Storage *
hrnStorageTestNew(
    const String *const path, const bool write, const time_t targetTime, StoragePathExpressionCallback pathExpressionFunction)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(BOOL, write);
        FUNCTION_HARNESS_PARAM(TIME, targetTime);
        FUNCTION_HARNESS_PARAM(FUNCTIONP, pathExpressionFunction);
    FUNCTION_HARNESS_END();

    static const StorageInterface hrnStorageInterfaceTest =
    {
        .feature = 1 << storageFeaturePath | 1 << storageFeatureInfoDetail | 1 << storageFeatureVersioning,

        .info = hrnStorageTestInfo,
        .list = hrnStorageTestList,
        .newRead = hrnStorageTestNewRead,
        .newWrite = hrnStorageTestNewWrite,
        .pathCreate = hrnStorageTestPathCreate,
        .pathRemove = hrnStorageTestPathRemove,
        .remove = hrnStorageTestRemove,
    };

    OBJ_NEW_BEGIN(HrnStorageTest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HrnStorageTest)
        {
            .interface = hrnStorageInterfaceTest,
            .storagePosix = storagePosixNewP(
                path, .write = write, .modeFile = STORAGE_MODE_FILE_DEFAULT, .modePath = STORAGE_MODE_PATH_DEFAULT,
                .pathExpressionFunction = pathExpressionFunction),
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(
        STORAGE,
        storageNew(
            STORAGE_TEST_TYPE, path, STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, targetTime,
            pathExpressionFunction, this, this->interface));
}

/***********************************************************************************************************************************
storageRepoGet() shim
***********************************************************************************************************************************/
static Storage *
storageRepoGet(const unsigned int repoIdx, const bool write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, repoIdx);
        FUNCTION_HARNESS_PARAM(BOOL, write);
    FUNCTION_HARNESS_END();

    if (hrnStorageHelperLocal.shim && repoIsLocal(repoIdx) && cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_POSIX_TYPE)
    {
        FUNCTION_HARNESS_RETURN(
            STORAGE,
            hrnStorageTestNew(cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write, storageRepoTargetTime(), storageRepoPathExpression));
    }

    FUNCTION_HARNESS_RETURN(STORAGE, storageRepoGet_SHIMMED(repoIdx, write));
}

/**********************************************************************************************************************************/
void
hrnStorageHelperRepoShimSet(const bool enabled)
{
    hrnStorageHelperLocal.shim = enabled;
}
