/***********************************************************************************************************************************
Storage Helper Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "storage/posix/write.h"
#include "storage/storage.h"

#include "common/harnessDebug.h"
#include "common/harnessStorage.h"

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
    void *base;                                                     // Driver for base file
    void *version;                                                  // Driver for version file
} HrnStorageWriteTest;

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
// STRING_STATIC(HRN_STORAGE_TEST_SECRET_STR,                          HRN_STORAGE_TEST_SECRET); !!!

#define STORAGE_TEST_TYPE                                           STRID5("test", 0xa4cb40)

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HRN_STORAGE_TEST_TYPE                                                                                         \
    HrnStorageTest *
#define FUNCTION_LOG_HRN_STORAGE_TEST_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "HrnStorageTest *", buffer, bufferSize)

#define FUNCTION_LOG_HRN_STORAGE_WRITE_TEST_TYPE                                                                                   \
    HrnStorageWriteTest *
#define FUNCTION_LOG_HRN_STORAGE_WRITE_TEST_FORMAT(value, buffer, bufferSize)                                                      \
    objNameToLog(value, "HrnStorageWriteTest *", buffer, bufferSize)

/***********************************************************************************************************************************
Test storage driver interface functions
***********************************************************************************************************************************/
static void
hrnStorageTestSecretCheck(const String *const file)
{
    if (strstr(strZ(file), HRN_STORAGE_TEST_SECRET) != NULL)
        THROW_FMT(AssertError, "path/file '%s' cannot contain " HRN_STORAGE_TEST_SECRET, strZ(file));
}

static void
hrnStorageWriteTestOpen(THIS_VOID)
{
    THIS(HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    hrnStorageWriteInterfaceDummy.open(this->base);
    hrnStorageWriteInterfaceDummy.open(this->version);

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

    hrnStorageWriteInterfaceDummy.write(this->base, buffer);
    hrnStorageWriteInterfaceDummy.write(this->version, buffer);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
hrnStorageWriteTestClose(THIS_VOID)
{
    THIS(HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    hrnStorageWriteInterfaceDummy.close(this->base);
    hrnStorageWriteInterfaceDummy.close(this->version);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
hrnStorageWriteTestFd(const THIS_VOID)
{
    THIS(const HrnStorageWriteTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_WRITE_TEST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    FUNCTION_HARNESS_RETURN(INT, hrnStorageWriteInterfaceDummy.fd(this->base));
}

FN_EXTERN StorageWrite *
hrnStorageWriteTestNew(
    Storage *const storagePosix, const String *const name, const mode_t modeFile, const mode_t modePath,
    const String *const user, const String *const group, const time_t timeModified, const bool createPath, const bool syncFile,
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
    ASSERT(timeModified == 0);
    ASSERT(createPath);

    OBJ_NEW_BEGIN(HrnStorageWriteTest, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        // Make sure the main file and the version both have the same timestamp
        const time_t timeModified = time(NULL);

        StorageWrite *const posix = storageWritePosixNew(
            storageDriver(storagePosix), name, modeFile, modePath, user, group, timeModified, createPath, false, false, false,
            truncate);

        // Copy the interface and update with our functions
        StorageWriteInterface interface = *storageWriteInterface(posix);
        interface.ioInterface.close = hrnStorageWriteTestClose;
        interface.ioInterface.fd = hrnStorageWriteTestFd;
        interface.ioInterface.open = hrnStorageWriteTestOpen;
        interface.ioInterface.write = hrnStorageWriteTest;

        // Find a version that has not been used
        const String *version;

        for (unsigned int versionId = 1; ; versionId++)
        {
            version = strNewFmt("%s/" HRN_STORAGE_TEST_SECRET "/%s/v%04u", strZ(strPath(name)), strZ(strBase(name)), versionId);

            if (!storageInfoP(storagePosix, version, .ignoreMissing = true).exists)
                break;
        }

        *this = (HrnStorageWriteTest)
        {
            .base = storageWriteDriver(posix),
            .version = storageWriteDriver(
                storageWritePosixNew(
                    storageDriver(storagePosix), version, modeFile, modePath, user, group, timeModified, createPath, false, false,
                    false, truncate)),
            .interface = interface,
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(STORAGE_WRITE, storageWriteNew(this, &this->interface));
}

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
        (void)param;                                                // No parameters are used
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    hrnStorageTestSecretCheck(path);

    FUNCTION_HARNESS_RETURN(STORAGE_LIST, hrnStorageInterfaceDummy.list(storageDriver(this->storagePosix), path, level, param));
}

static StorageRead *
hrnStorageTestNewRead(THIS_VOID, const String *const file, const bool ignoreMissing, const StorageInterfaceNewReadParam param)
{
    THIS(HrnStorageTest);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_STORAGE_TEST, this);
        FUNCTION_HARNESS_PARAM(STRING, file);
        FUNCTION_HARNESS_PARAM(BOOL, ignoreMissing);
        FUNCTION_HARNESS_PARAM(UINT64, param.offset);
        FUNCTION_HARNESS_PARAM(VARIANT, param.limit);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    hrnStorageTestSecretCheck(file);

    FUNCTION_HARNESS_RETURN(
        STORAGE_READ, hrnStorageInterfaceDummy.newRead(storageDriver(this->storagePosix), file, ignoreMissing, param));
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

    FUNCTION_HARNESS_RETURN(BOOL, hrnStorageInterfaceDummy.pathRemove(storageDriver(this->storagePosix), path, recurse, param));
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

    hrnStorageInterfaceDummy.remove(storageDriver(this->storagePosix), file, param);

    FUNCTION_HARNESS_RETURN_VOID();
}

FN_EXTERN Storage *
hrnStorageTestNew(const String *const path, const StoragePosixNewParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(MODE, param.modeFile);
        FUNCTION_HARNESS_PARAM(MODE, param.modePath);
        FUNCTION_HARNESS_PARAM(BOOL, param.write);
        FUNCTION_HARNESS_PARAM(FUNCTIONP, param.pathExpressionFunction);
    FUNCTION_HARNESS_END();

    static const StorageInterface hrnStorageInterfaceTest =
    {
        .feature = 0, // !!! NEED TO ADD VERSION

        .info = hrnStorageTestInfo,
        .list = hrnStorageTestList,
        .newRead = hrnStorageTestNewRead,
        .newWrite = hrnStorageTestNewWrite,
        .pathRemove = hrnStorageTestPathRemove,
        .remove = hrnStorageTestRemove,
    };

    OBJ_NEW_BEGIN(HrnStorageTest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HrnStorageTest)
        {
            .interface = hrnStorageInterfaceTest,
            .storagePosix = storagePosixNew(path, param),
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(
        STORAGE,
        storageNew(
            STORAGE_TEST_TYPE, path, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, 0, param.pathExpressionFunction,
            this, this->interface));
}

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
            hrnStorageTestNew(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx),
                (StoragePosixNewParam){.write = write, .pathExpressionFunction = storageRepoPathExpression}));
    }

    FUNCTION_HARNESS_RETURN(STORAGE, storageRepoGet_SHIMMED(repoIdx, write));
}

/**********************************************************************************************************************************/
void
hrnStorageHelperRepoShimSet(const bool enabled)
{
    hrnStorageHelperLocal.shim = enabled;
}
