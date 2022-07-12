/***********************************************************************************************************************************
Test SFTP Storage
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/io/socket/client.h"
#include "common/time.h"
#include "storage/read.h"
#include "storage/sftp/storage.h"
#include "storage/write.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Macro to create a path and file that cannot be accessed
***********************************************************************************************************************************/
#define TEST_CREATE_NOPERM()                                                                                                       \
    HRN_SYSTEM_FMT(                                                                                                                \
        "sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strZ(pathNoPerm), strZ(fileNoPerm), strZ(fileNoPerm))

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = strNewZ("vagrant"), .password = strNewZ("vagrant"), .write = true);
//    Storage *storageTmp = storageSftpNewP(STRDEF("/tmp"), .write = true);
    ioBufferSizeSet(2);

    /*
    // Directory and file that cannot be accessed to test permissions errors
#ifdef TEST_CONTAINER_REQUIRED
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);
#endif // TEST_CONTAINER_REQUIRED

*/
    // This test should always be first so the storage helper is uninitialized
    // *****************************************************************************************************************************
    if (testBegin("storageHelperDryRunInit()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is not initialized");

        TEST_ERROR(storagePgIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is true");

        storageHelperDryRunInit(true);

        TEST_ERROR(storagePgIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults");

        Storage *storageTest = NULL;
        TEST_ASSIGN(storageTest, storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 10000, 10000, .user = strNewZ("vagrant"),
                    .password = strNewZ("vagrant")), "new storage (defaults)");
        TEST_RESULT_STR_Z(storageTest->path, "/tmp", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/path/to"), STRDEF("localhost"), 22, 10000, 10000, .user = strNewZ("vagrant"), .password = strNewZ("vagrant"),
                .modeFile = 0600, .modePath = 0700, .write = true), "new storage (non-default)");
        TEST_RESULT_STR_Z(storageTest->path, "/path/to", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, true, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is empty");

        TEST_RESULT_PTR(storageInterface(storageTest).info, storageTest->pub.interface.info, "check interface");
        TEST_RESULT_PTR(storageDriver(storageTest), storageTest->pub.driver, "check driver");
        TEST_RESULT_UINT(storageType(storageTest), storageTest->pub.type, "check type");
        TEST_RESULT_BOOL(storageFeature(storageTest, storageFeaturePath), true, "check path feature");

    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
/*
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED
*/

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 1000), false, "file does not exist");
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path");

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file and path");

        const String *fileExists = STRDEF(TEST_PATH "/exists");
        const String *pathExists = STRDEF(TEST_PATH "/pathExists");
        HRN_SYSTEM_FMT("touch %s", strZ(fileExists));
        HRN_SYSTEM_FMT("mkdir %s", strZ(pathExists));

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, pathExists), false, "not a file");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, fileExists), false, "not a path");
        HRN_SYSTEM_FMT("rm %s", strZ(fileExists));


        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file after wait");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                sleepMSec(250);
                HRN_SYSTEM_FMT("touch %s", strZ(fileExists));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 10000), true, "file exists after wait");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        HRN_SYSTEM_FMT("rm %s", strZ(fileExists));


    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        // -----------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / exists");

        TEST_RESULT_BOOL(storageInfoP(storagePosixNewP(FSLASH_STR), NULL).exists, true, "info for /");
        // -----------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / does not exist with no path feature");

        Storage *storageRootNoPath = storagePosixNewP(FSLASH_STR);
        storageRootNoPath->pub.interface.feature ^= 1 << storageFeaturePath;

        TEST_RESULT_BOOL(storageInfoP(storageRootNoPath, NULL, .ignoreMissing = true).exists, false, "no info for /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory does not exists");

        const String *fileName = STRDEF(TEST_PATH "/fileinfo");

        //!!! validate/verify this change -- sftp returns 2 file not found, should we overwrite errno with 2
        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING ": [11] Resource temporarily unavailable",
            //storageInfoP(storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING ": [2] No such file or directory",
            strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file does not exists");

        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, fileName, .ignoreMissing = true), "get file info (missing)");
        TEST_RESULT_BOOL(info.exists, false, "check not exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info outside of base path");

        TEST_ERROR(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true), AssertError,
            "absolute path '/etc' is not in base path '" TEST_PATH "'");
        TEST_RESULT_BOOL(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true, .noPathEnforce = true).exists, true,
            "path not enforced");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - path");

        HRN_STORAGE_TIME(storageTest, TEST_PATH, 1555160000);

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR), "get path info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0770, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info basic - path");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR), "get path info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, 0, "check user id");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_UINT(info.groupId, 0, "check group id");
        TEST_RESULT_STR(info.group, NULL, "check group");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - file");

        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), buffer), "put test file");

        HRN_STORAGE_TIME(storageTest, strZ(fileName), 1555155555);

        TEST_ASSIGN(info, storageInfoP(storageTest, fileName), "get file info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 8, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555155555, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

    }


    FUNCTION_HARNESS_RETURN_VOID();
}
