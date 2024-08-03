/***********************************************************************************************************************************
Storage Helper Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "storage/storage.h"

#include "common/harnessDebug.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HrnStorageTest
{
    STORAGE_COMMON_MEMBER;
    void *posix;
} HrnStorageTest;

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

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HRN_STORAGE_TEST_TYPE                                                                                         \
    HrnStorageTest *
#define FUNCTION_LOG_HRN_STORAGE_TEST_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "HrnStorageTest *", buffer, bufferSize)

/***********************************************************************************************************************************
Test storage driver interface functions.
***********************************************************************************************************************************/
static void
hrnStorageTestSecretCheck(const String *const file)
{
    if (strstr(strZ(file), HRN_STORAGE_TEST_SECRET) != NULL)
        THROW_FMT(AssertError, "path/file '%s' cannot contain " HRN_STORAGE_TEST_SECRET, strZ(file));
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

    FUNCTION_HARNESS_RETURN(STORAGE_INFO, hrnStorageInterfaceDummy.info(this->posix, file, level, param));
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

    FUNCTION_HARNESS_RETURN(STORAGE_LIST, hrnStorageInterfaceDummy.list(this->posix, path, level, param));
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

    FUNCTION_HARNESS_RETURN(STORAGE_READ, hrnStorageInterfaceDummy.newRead(this->posix, file, ignoreMissing, param));
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

    FUNCTION_HARNESS_RETURN(STORAGE_WRITE, hrnStorageInterfaceDummy.newWrite(this->posix, file, param));
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

    FUNCTION_HARNESS_RETURN(BOOL, hrnStorageInterfaceDummy.pathRemove(this->posix, path, recurse, param));
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

    hrnStorageInterfaceDummy.remove(this->posix, file, param);

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
            .posix = storageDriver(storagePosixNew(path, param)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(
        STORAGE,
        storageNew(
            STORAGE_POSIX_TYPE, path, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, param.pathExpressionFunction, this,
            this->interface));
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
