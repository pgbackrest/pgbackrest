/***********************************************************************************************************************************
Test Posix/CIFS Storage
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/time.h"
#include "storage/read.h"
#include "storage/write.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test function for path expression
***********************************************************************************************************************************/
static String *
storageTestPathExpression(const String *expression, const String *path)
{
    String *result = NULL;

    if (strcmp(strZ(expression), "<TEST>") == 0)
        result = strNewFmt("test%s", path == NULL ? "" : zNewFmt("/%s", strZ(path)));
    else if (strcmp(strZ(expression), "<NULL>") != 0)
        THROW_FMT(AssertError, "invalid expression '%s'", strZ(expression));

    return result;
}

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
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
    Storage *storageTmp = storagePosixNewP(STRDEF("/tmp"), .write = true);
    ioBufferSizeSet(2);

    // Directory and file that cannot be accessed to test permissions errors
#ifdef TEST_CONTAINER_REQUIRED
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);
#endif // TEST_CONTAINER_REQUIRED

    // Write file for testing if storage is read-only
    const String *writeFile = STRDEF(TEST_PATH "/writefile");

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
        TEST_ASSIGN(storageTest, storagePosixNewP(STRDEF("/")), "new storage (defaults)");
        TEST_RESULT_STR_Z(storageTest->path, "/", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        TEST_ASSIGN(
            storageTest,
            storagePosixNewP(
                STRDEF("/path/to"), .modeFile = 0600, .modePath = 0700, .write = true,
                .pathExpressionFunction = storageTestPathExpression),
            "new storage (non-default)");
        TEST_RESULT_STR_Z(storageTest->path, "/path/to", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, true, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction != NULL, true, "check expression function is set");

        TEST_RESULT_PTR(storageInterface(storageTest).info, storageTest->pub.interface.info, "check interface");
        TEST_RESULT_PTR(storageDriver(storageTest), storageTest->pub.driver, "check driver");
        TEST_RESULT_UINT(storageType(storageTest), storageTest->pub.type, "check type");
        TEST_RESULT_BOOL(storageFeature(storageTest, storageFeaturePath), true, "check path feature");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 100), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path");

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ERROR_FMT(
            storageExistsP(storageTest, fileNoPerm), FileOpenError,
            "unable to get info for path/file '%s': [13] Permission denied", strZ(fileNoPerm));
        TEST_ERROR_FMT(
            storagePathExistsP(storageTest, fileNoPerm), FileOpenError,
            "unable to get info for path/file '%s': [13] Permission denied", strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

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
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 1000), true, "file exists after wait");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        HRN_SYSTEM_FMT("rm %s", strZ(fileExists));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO ": [13] Permission denied", strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

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

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING ": [2] No such file or directory",
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

#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM_FMT("sudo chown 99999:99999 %s", strZ(fileName));
#endif // TEST_CONTAINER_REQUIRED

        TEST_ASSIGN(info, storageInfoP(storageTest, fileName), "get file info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 8, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555155555, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
#ifdef TEST_CONTAINER_REQUIRED
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");
#endif // TEST_CONTAINER_REQUIRED

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link");

        const String *linkName = STRDEF(TEST_PATH "/testlink");
        HRN_SYSTEM_FMT("ln -s /tmp %s", strZ(linkName));

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName), "get link info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "/tmp", "check link destination");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName, .followLink = true), "get info from path pointed to by link");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR_Z(info.user, "root", "check user");
        TEST_RESULT_STR_Z(info.group, strEqZ(info.group, "wheel") ? "wheel" : "root", "check group");

        storageRemoveP(storageTest, linkName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - pipe");

        const String *pipeName = STRDEF(TEST_PATH "/testpipe");
        HRN_SYSTEM_FMT("mkfifo -m 666 %s", strZ(pipeName));

        TEST_ASSIGN(info, storageInfoP(storageTest, pipeName), "get info from pipe (special file)");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        storageRemoveP(storageTest, pipeName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewItrP()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError,
            STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageNewItrP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "ignore missing dir");

#ifdef TEST_CONTAINER_REQUIRED
        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm), PathOpenError, STORAGE_ERROR_LIST_INFO ": [13] Permission denied",
            strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm), PathOpenError, STORAGE_ERROR_LIST_INFO ": [13] Permission denied",
            strZ(pathNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storagePosixListEntry()");

        TEST_RESULT_VOID(
            storagePosixListEntry(
                (StoragePosix *)storageDriver(storageTest), storageLstNew(storageInfoLevelBasic), STRDEF("pg"), "missing",
                storageInfoLevelBasic),
            "missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

        storagePathCreateP(storageTest, STRDEF("pg"), .mode = 0766);

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER ", g=" TEST_GROUP ", m=0766}\n",
            .level = storageInfoLevelDetail, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

#ifdef TEST_CONTAINER_REQUIRED
        storagePathCreateP(storageTest, STRDEF("pg/.include"), .mode = 0755);
        HRN_SYSTEM("sudo chown 77777:77777 " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        storagePutP(
            storageNewWriteP(storageTest, STRDEF("pg/file"), .modeFile = 0660, .timeModified = 1656433838), BUFSTRDEF("TESTDATA"));

        HRN_SYSTEM("ln -s ../file " TEST_PATH "/pg/link");
        HRN_SYSTEM("mkfifo -m 777 " TEST_PATH "/pg/pipe");

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER ", g=" TEST_GROUP ", m=0766}\n"
#ifdef TEST_CONTAINER_REQUIRED
            ".include/ {u=77777, g=77777, m=0755}\n"
#endif // TEST_CONTAINER_REQUIRED
            "file {s=8, t=1656433838, u=" TEST_USER ", g=" TEST_GROUP ", m=0660}\n"
            "link> {d=../file, u=" TEST_USER ", g=" TEST_GROUP "}\n"
            "pipe*\n",
            .level = storageInfoLevelDetail, .includeDot = true);

#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM("sudo rmdir " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageItrMore() twice in a row");

        StorageIterator *storageItr = NULL;

        TEST_ASSIGN(storageItr, storageNewItrP(storageTest, STRDEF("pg")), "new iterator");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more again");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse desc");

        storagePathCreateP(storageTest, STRDEF("pg/path"), .mode = 0700);
        storagePutP(
            storageNewWriteP(storageTest, STRDEF("pg/path/file"), .modeFile = 0600, .timeModified = 1656434296),
            BUFSTRDEF("TESTDATA"));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "pipe*\n"
            "path/file {s=8, t=1656434296}\n"
            "path/\n"
            "link> {d=../file}\n"
            "file {s=8, t=1656433838}\n"
            "./\n",
            .level = storageInfoLevelBasic, .includeDot = true, .sortOrder = sortOrderDesc);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse asc");

        // Create a path with a subpath that will always be last to make sure lists are not freed too early in the iterator
        storagePathCreateP(storageTest, STRDEF("pg/zzz/yyy"), .mode = 0700);

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./\n"
            "file {s=8, t=1656433838}\n"
            "link> {d=../file}\n"
            "path/\n"
            "path/file {s=8, t=1656434296}\n"
            "pipe*\n"
            "zzz/\n"
            "zzz/yyy/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path basic info - recurse");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "zzz/yyy/\n"
            "zzz/\n"
            "pipe*\n"
            "path/file {s=8, t=1656434296}\n"
            "path/\n"
            "link> {d=../file}\n"
            "file {s=8, t=1656433838}\n"
            "./\n",
            .levelForce = true, .includeDot = true, .sortOrder = sortOrderDesc);

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("empty path - filter");

        storagePathCreateP(storageTest, STRDEF("pg/empty"), .mode = 0700);

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "empty/\n",
            .level = storageInfoLevelType, .expression = "^empty");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "path/file {s=8, t=1656434296}\n",
            .level = storageInfoLevelBasic, .expression = "\\/file$");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
            TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageListP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListP(storageTest, STRDEF(BOGUS_STR))), 0, "empty list for missing dir");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing, regardless of errorOnMissing setting");

        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list - path with files");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF(".aaa.txt")), BUFSTRDEF("aaa")), "write aaa.text");
        TEST_RESULT_STRLST_Z(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ".aaa.txt\n"
#ifdef TEST_CONTAINER_REQUIRED
            "noperm\n"
#endif // TEST_CONTAINER_REQUIRED
            , "dir list");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF("bbb.txt")), BUFSTRDEF("bbb")), "write bbb.text");
        TEST_RESULT_STRLST_Z(storageListP(storageTest, NULL, .expression = STRDEF("^bbb")), "bbb.txt\n", "dir list");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageCopy()"))
    {
        const String *sourceFile = STRDEF(TEST_PATH "/source.txt");
        const String *const destinationFile = STRDEF(TEST_PATH "/destination.txt");

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - missing source");

        TEST_ERROR_FMT(storageCopyP(source, destination), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - ignore missing source");

        source = storageNewReadP(storageTest, sourceFile, .ignoreMissing = true);

        TEST_RESULT_BOOL(storageCopyP(source, destination), false, "copy and ignore missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - success");

        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, sourceFile), expectedBuffer), "write source file");

        source = storageNewReadP(storageTest, sourceFile);

        TEST_RESULT_BOOL(storageCopyP(source, destination), true, "copy file");
        TEST_RESULT_BOOL(bufEq(expectedBuffer, storageGetP(storageNewReadP(storageTest, destinationFile))), true, "check file");

        storageRemoveP(storageTest, sourceFile, .errorOnMissing = true);
        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageMove()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        const String *sourceFile = STRDEF(TEST_PATH "/source.txt");
        const String *destinationFile = STRDEF(TEST_PATH "/sub/destination.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - missing source");

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMissingError,
            "unable to move missing source '%s': [2] No such file or directory", strZ(sourceFile));

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - source file permission denied");

        source = storageNewReadP(storageTest, fileNoPerm);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMoveError,
            "unable to move '%s' to '%s': [13] Permission denied", strZ(fileNoPerm), strZ(destinationFile));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - missing destination");

        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        storagePutP(storageNewWriteP(storageTest, sourceFile), buffer);

        source = storageNewReadP(storageTest, sourceFile);
        destination = storageNewWriteP(storageTest, destinationFile, .noCreatePath = true);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), PathMissingError,
            "unable to move '%s' to missing path '%s': [2] No such file or directory", strZ(sourceFile),
            strZ(strPath(destinationFile)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - success");

        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, destinationFile), true, "check destination file exists");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, destinationFile))), "TESTFILE", "check destination contents");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - same path");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = STRDEF(TEST_PATH "/sub/destination2.txt");
        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to same path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - parent path");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = STRDEF(TEST_PATH "/source.txt");
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to parent path (no sync)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - across filesystems");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = strNewFmt("/tmp/destination.txt");
        destination = storageNewWriteP(storageTmp, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to another filesystem");
        TEST_RESULT_BOOL(storageExistsP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTmp, destinationFile), true, "check destination file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move across filesystems without syncing the paths");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTmp, sourceFile);
        destinationFile = STRDEF(TEST_PATH "/source.txt");
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to another filesystem without path sync");
        TEST_RESULT_BOOL(storageExistsP(storageTmp, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, destinationFile), true, "check destination file exists");

        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storageTest = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - root path");

        TEST_ASSIGN(storageTest, storagePosixNewP(STRDEF("/")), "new storage /");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/")), "/", "same as root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("subdir")), "/subdir", "simple subdir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - expressions");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        TEST_ASSIGN(
            storageTest, storagePosixNewP(STRDEF("/path/to"), .pathExpressionFunction = storageTestPathExpression),
            "new storage /path/to with expression");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/path/to", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/path/to")), "/path/to", "absolute root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("is/a/subdir")), "/path/to/is/a/subdir", "subdir");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("/bogus")), AssertError, "absolute path '/bogus' is not in base path '/path/to'");
        TEST_ERROR(
            storagePathP(storageTest, STRDEF("/path/toot")), AssertError,
            "absolute path '/path/toot' is not in base path '/path/to'");

        // Path enforcement disabled for a single call
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/bogus"), .noEnforce = true), "/bogus", "path enforce disabled");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<TEST")), AssertError, "end > not found in path expression '<TEST'");
        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>" BOGUS_STR)), AssertError,
            "'/' should separate expression and path '<TEST>BOGUS'");

        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("<TEST>")), "/path/to/test", "expression");
        TEST_ERROR(strZ(storagePathP(storageTest, STRDEF("<TEST>/"))), AssertError, "path '<TEST>/' should not end in '/'");

        TEST_RESULT_STR_Z(
            storagePathP(storageTest, STRDEF("<TEST>/something")), "/path/to/test/something", "expression with path");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<NULL>")), AssertError, "evaluated path '<NULL>' cannot be null");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<WHATEVS>")), AssertError, "invalid expression '<WHATEVS>'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1"), .errorOnExists = true), PathCreateError,
            "unable to create path '" TEST_PATH "/sub1': [17] File exists");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub2")).mode, 0777, "check sub2 dir mode");

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "unable to create path '" TEST_PATH "/sub3/sub4': [2] No such file or directory");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub3/sub4")), "create sub3/sub4");

        HRN_SYSTEM("rm -rf " TEST_PATH "/sub*");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing");

        const String *pathRemove1 = STRDEF(TEST_PATH "/remove1");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove2 = strNewFmt("%s/remove2", strZ(pathRemove1));

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - parent/subpath permission denied");

        HRN_SYSTEM_FMT("sudo mkdir -p -m 700 %s", strZ(pathRemove2));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2), PathRemoveError, STORAGE_ERROR_PATH_REMOVE ": [13] Permission denied",
            strZ(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - subpath permission denied");

        HRN_SYSTEM_FMT("sudo chmod 777 %s", strZ(pathRemove1));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - file in subpath, permission denied");

        String *fileRemove = strNewFmt("%s/remove.txt", strZ(pathRemove2));

        HRN_SYSTEM_FMT(
            "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strZ(pathRemove2), strZ(fileRemove), strZ(fileRemove));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_FILE ": [13] Permission denied", strZ(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath and file removed");

        HRN_SYSTEM_FMT("sudo chmod 777 %s", strZ(pathRemove2));

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath removed");

        HRN_SYSTEM_FMT("mkdir -p %s", strZ(pathRemove2));

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storagePathSyncP(storageTest, fileNoPerm), PathOpenError, STORAGE_ERROR_PATH_SYNC_OPEN ": [13] Permission denied",
            strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync - error on missing");

        const String *pathName = STRDEF(TEST_PATH "/testpath");

        TEST_ERROR_FMT(
            storagePathSyncP(storageTest, pathName), PathMissingError, STORAGE_ERROR_PATH_SYNC_MISSING, strZ(pathName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync - error on invalid absolute path");

        TEST_ERROR_FMT(
            storagePathSyncP(storagePosixNewP(STRDEF("/"), .write = true), STRDEF("/proc")),
            PathSyncError, STORAGE_ERROR_PATH_SYNC ": [22] Invalid argument", "/proc");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync - success");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, pathName), "create path to sync");
        TEST_RESULT_VOID(storagePathSyncP(storageTest, pathName), "sync path");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageLinkCreate()"))
    {
        StorageInfo info = {0};
        const String *backupLabel = STRDEF("20181119-152138F");
        const String *latestLabel = STRDEF("latest");
        int invalidLinkType = 9;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create soft link to BACKUPLABEL");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, backupLabel), "create path to link to");
        TEST_RESULT_VOID(storageLinkCreateP(storageTest, backupLabel, latestLabel), "create symlink");
        TEST_ASSIGN(info, storageInfoP(storageTest, latestLabel, .ignoreMissing = false), "get link info");
        TEST_RESULT_STR(info.linkDestination, backupLabel, "match link destination");
        TEST_RESULT_VOID(storageRemoveP(storageTest, latestLabel), "remove symlink");

        TEST_RESULT_VOID(storageLinkCreateP(storageTest, backupLabel, latestLabel, .linkType = storageLinkSym), "create symlink");
        TEST_ASSIGN(info, storageInfoP(storageTest, latestLabel, .ignoreMissing = false), "get link info");
        TEST_RESULT_STR(info.linkDestination, backupLabel, "match link destination");
        TEST_RESULT_VOID(storageRemoveP(storageTest, latestLabel), "remove symlink");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("hardlink success/fail");

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_VOID(
            storageLinkCreateP(storageTest, emptyFile, latestLabel, .linkType = storageLinkHard), "hardlink to empty file");
        TEST_RESULT_VOID(storageRemoveP(storageTest, emptyFile), "remove empty file");
        TEST_RESULT_VOID(storageRemoveP(storageTest, latestLabel), "remove latest label");

        TEST_ERROR(
            storageLinkCreateP(storageTest, backupLabel, latestLabel, .linkType = storageLinkHard), FileOpenError,
            "unable to create hardlink \'latest\' to \'20181119-152138F\': [1] Operation not permitted");
        TEST_ASSIGN(info, storageInfoP(storageTest, latestLabel, .ignoreMissing = true), "get link info");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, backupLabel), "remove backup path");
        TEST_ERROR(
            storageLinkCreateP(storageTest, backupLabel, latestLabel, .linkType = invalidLinkType), AssertError,
            "assertion '(param.linkType == storageLinkSym && storageFeature(this, storageFeatureSymLink)) ||"
            " (param.linkType == storageLinkHard && storageFeature(this, storageFeatureHardLink))' failed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        HRN_SYSTEM_FMT("touch %s", strZ(fileName));

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), ((StorageReadPosix *)file->driver)->fd, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");
        StorageWrite *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file (defaults)");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": [13] Permission denied",
            strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - defaults");

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), ((StorageWritePosix *)file->driver)->fd, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "check file mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).timeModified, 1, "check file modified times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("test that a premature free (from error or otherwise) does not rename the file");

        fileName = STRDEF(TEST_PATH "/sub1/testfile-abort");
        String *fileNameTmp = strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(fileName));

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), BUFSTRDEF("TESTDATA")), "write data");
        TEST_RESULT_VOID(ioWriteFlush(storageWriteIo(file)), "flush data");
        TEST_RESULT_VOID(ioWriteFree(storageWriteIo(file)), "free file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileName), false, "destination file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, fileNameTmp), true, "destination tmp file exists");
        TEST_RESULT_UINT(storageInfoP(storageTest, fileNameTmp).size, 8, "check temp file size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - set mode");

        fileName = STRDEF(TEST_PATH "/sub2/testfile");

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .modePath = 0700, .modeFile = 0600, .group = TEST_GROUP_STR),
            "new write file (set mode)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_VOID(storageWritePosixClose(file->driver), "close file again");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "check file mode");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storagePosixNewP(STRDEF("/"), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get error - attempt to get directory");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, TEST_PATH_STR)), FileReadError,
            "unable to read '" TEST_PATH "': [21] Is a directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file");

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_BOOL(storageExistsP(storageTest, emptyFile), true, "check empty file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), buffer), "put test file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - ignore missing");

        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/" BOGUS_STR), .ignoreMissing = true)), NULL,
            "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - empty file");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.empty"))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - file with contents");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size smaller");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size larger");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '" TEST_PATH "/test.txt'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - smaller buffer size");

        ioBufferSizeSet(2);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read offset bytes");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = UINT64_MAX)), FileOpenError,
            "unable to seek to 18446744073709551615 in file '" TEST_PATH "/test.txt': [22] Invalid argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset bytes");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4)), "get");
        TEST_RESULT_UINT(bufSize(buffer), 5, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset/limited bytes");

        TEST_ASSIGN(
            buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4,
            .limit = VARUINT64(4))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing");

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("missing")), "remove missing file");
        TEST_ERROR(
            storageRemoveP(storageTest, STRDEF("missing"), .errorOnMissing = true), FileRemoveError,
            "unable to remove '" TEST_PATH "/missing': [2] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file exists");

        const String *fileExists = STRDEF(TEST_PATH "/exists");
        HRN_SYSTEM_FMT("touch %s", strZ(fileExists));

        TEST_RESULT_VOID(storageRemoveP(storageTest, fileExists), "remove exists file");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - permission denied");

        TEST_ERROR_FMT(
            storageRemoveP(storageTest, fileNoPerm), FileRemoveError, "unable to remove '%s': [13] Permission denied",
            strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
        StorageRead *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing - file with no permission to read");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(file), true, "check ignore missing");
        TEST_RESULT_STR(storageReadName(file), fileNoPerm, "check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": [13] Permission denied", strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/test.file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad file descriptor");

        Buffer *outBuffer = bufNew(2);
        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), expectedBuffer), "write test file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");

        // Close the file descriptor so operations will fail
        close(((StorageReadPosix *)file->driver)->fd);

        TEST_ERROR_FMT(
            ioRead(storageReadIo(file), outBuffer), FileReadError, "unable to read '%s': [9] Bad file descriptor", strZ(fileName));

        // Set file descriptor to -1 so the close on free will not fail
        ((StorageReadPosix *)file->driver)->fd = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental load");

        Buffer *buffer = bufNew(0);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(
                file,
                storageReadMove(storageNewReadP(storageTest, fileName, .limit = VARUINT64(44)), memContextPrior()),
                "new read file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_STR(storageReadName(file), fileName, "check file name");
        TEST_RESULT_UINT(storageReadType(file), STORAGE_POSIX_TYPE, "check file type");
        TEST_RESULT_UINT(storageReadOffset(file), 0, "check offset");
        TEST_RESULT_UINT(varUInt64(storageReadLimit(file)), 44, "check limit");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), false, "check file contents (not all loaded yet)");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "no data to load");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_VOID(storageReadPosix(file->driver, outBuffer, true), "no data to load from driver either");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "eof");
        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "still eof");

        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        TEST_RESULT_VOID(storageReadFree(storageNewReadP(storageTest, fileName)), "free file");

        TEST_RESULT_VOID(storageReadMove(NULL, memContextTop()), "move null file");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageWrite"))
    {
        StorageWrite *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check getters");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noCreatePath = true, .noSyncFile = true,
                .noSyncPath = true, .noAtomic = true),
            "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(file), false, "check atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(file), false, "check create path");
        TEST_RESULT_INT(storageWriteModeFile(file), 0444, "check mode file");
        TEST_RESULT_INT(storageWriteModePath(file), 0555, "check mode path");
        TEST_RESULT_STR(storageWriteName(file), fileNoPerm, "check name");
        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "check sync path");
        TEST_RESULT_BOOL(storageWriteSyncFile(file), false, "check sync file");
        TEST_RESULT_BOOL(storageWriteTruncate(file), true, "file will be truncated");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": [13] Permission denied",
            strZ(fileNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/sub1/test.file");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noCreatePath = true, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad file descriptor");

        String *fileTmp = strNewFmt("%s.pgbackrest.tmp", strZ(fileName));
        ioBufferSizeSet(10);
        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        // Close the file descriptor so operations will fail
        close(((StorageWritePosix *)file->driver)->fd);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        TEST_ERROR_FMT(
            storageWritePosix(file->driver, buffer), FileWriteError, "unable to write '%s.pgbackrest.tmp': [9] Bad file descriptor",
            strZ(fileName));
        TEST_ERROR_FMT(
            storageWritePosixClose(file->driver), FileSyncError, STORAGE_ERROR_WRITE_SYNC ": [9] Bad file descriptor",
            strZ(fileTmp));

        // Disable file sync so close() can be reached
        ((StorageWritePosix *)file->driver)->interface.syncFile = false;

        TEST_ERROR_FMT(
            storageWritePosixClose(file->driver), FileCloseError, STORAGE_ERROR_WRITE_CLOSE ": [9] Bad file descriptor",
            strZ(fileTmp));

        // Set file descriptor to -1 so the close on free with not fail
        ((StorageWritePosix *)file->driver)->fd = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail rename in close");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_UINT(storageWriteType(file), STORAGE_POSIX_TYPE, "check file type");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        // Rename the file back to original name from tmp -- this will cause the rename in close to fail
        TEST_RESULT_INT(rename(strZ(fileTmp), strZ(fileName)), 0, "rename tmp file");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileMoveError, "unable to move '%s' to '%s': [2] No such file or directory",
            strZ(fileTmp), strZ(fileName));

        // Set file descriptor to -1 so the close on free with not fail
        ((StorageWritePosix *)file->driver)->fd = -1;

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file success");

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageWriteMove(storageNewWriteP(storageTest, fileName), memContextPrior()), "new write file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), NULL), "write null buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), bufNew(0)), "write zero buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_VOID(storageWriteFree(storageNewWriteP(storageTest, fileName)), "free file");
        TEST_RESULT_VOID(storageWriteMove(NULL, memContextTop()), "move null file");

        Buffer *expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write subpath and file success");

        fileName = STRDEF(TEST_PATH "/sub2/test.file");

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no truncate");

        HRN_STORAGE_PUT_Z(storageTest, "no-truncate", "ABC", .modeFile = 0600);

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, STRDEF("no-truncate"), .modeFile = 0660, .timeModified = 77777, .noAtomic = true,
            .noTruncate = true), "new write file");
        TEST_RESULT_BOOL(storageWriteTruncate(file), false, "file will not be truncated");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        TEST_STORAGE_GET(storageTest, "no-truncate", "ABC");
        TEST_RESULT_UINT(storageInfoP(storageTest, STRDEF("no-truncate")).mode, 0600, "check mode");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("no-truncate")).timeModified, 77777, "check time");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageLocal() and storageLocalWrite()"))
    {
        const Storage *storage = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageLocal()");

        TEST_RESULT_PTR(storageHelper.storageLocal, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocal(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocal, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocal(), storage, "get cached storage");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), "/", "check base path");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageLocalWrite()");

        TEST_RESULT_PTR(storageHelper.storageLocalWrite, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocalWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocalWrite(), storage, "get cached storage");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), "/", "check base path");

        TEST_RESULT_VOID(storageNewWriteP(storage, writeFile), "writes are allowed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepo*()"))
    {
        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storage = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - cached/not cached");

        TEST_RESULT_PTR(storageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageRepo[0], storage, "repo storage cached");
        TEST_RESULT_PTR(storageRepo(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - confirm settings");

        TEST_ERROR(storagePathP(storage, STRDEF("<BOGUS>/path")), AssertError, "invalid expression '<BOGUS>'");
        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), TEST_PATH, "check base path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), TEST_PATH "/archive/db", "check archive path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/simple")), TEST_PATH "/archive/db/simple",
            "check simple path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/700000007000000070000000")),
            TEST_PATH "/archive/db/9.4-1/7000000070000000/700000007000000070000000", "check segment path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/00000008.history")),
            TEST_PATH "/archive/db/9.4-1/00000008.history", "check history path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/000000010000014C0000001A.00000028.backup")),
            TEST_PATH "/archive/db/9.4-1/000000010000014C/000000010000014C0000001A.00000028.backup",
            "check archive backup path");
        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_REPO_BACKUP_STR), TEST_PATH "/backup/db", "check backup path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - helper does not fail when stanza option not set");

        // Change the stanza to NULL with the stanzaInit flag still true, make sure helper does not fail when stanza option not set
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_ASSIGN(storage, storageRepo(), "new repo storage no stanza");
        TEST_RESULT_STR(storageHelper.stanza, NULL, "stanza NULL");

        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), TEST_PATH "/archive", "check archive path - NULL stanza");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/simple")), TEST_PATH "/archive/simple",
            "check simple archive path - NULL stanza");
        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_REPO_BACKUP_STR), TEST_PATH "/backup", "check backup path - NULL stanza");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_BACKUP "/simple")), TEST_PATH "/backup/simple",
            "check simple backup path - NULL stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepoWrite() - confirm write enabled");

        TEST_RESULT_PTR(storageHelper.storageRepoWrite, NULL, "repo write storage not cached");
        TEST_ASSIGN(storage, storageRepoWrite(), "new write storage");
        TEST_RESULT_PTR(storageHelper.storageRepoWrite[0], storage, "repo write storage cached");
        TEST_RESULT_PTR(storageRepoWrite(), storage, "get cached storage");

        TEST_RESULT_BOOL(storage->write, true, "get write enabled");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSpool(), storageSpoolWrite() and storagePg*()"))
    {
        const Storage *storage = NULL;

        // Load configuration to set spool-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, TEST_PATH "/db2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool() - cached/not cached");

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpool, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("STORAGE_SPOOL_ARCHIVE expression");

        TEST_RESULT_STR_Z(storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE)), TEST_PATH "/archive/db", "check spool path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE, "file.ext")), TEST_PATH "/archive/db/file.ext",
            "check spool file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool() - confirm settings");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), TEST_PATH, "check base path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_SPOOL_ARCHIVE_OUT_STR), TEST_PATH "/archive/db/out", "check spool out path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/file.ext")), TEST_PATH "/archive/db/out/file.ext",
            "check spool out file");

        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_SPOOL_ARCHIVE_IN_STR), TEST_PATH "/archive/db/in", "check spool in path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE_IN "/file.ext")), TEST_PATH "/archive/db/in/file.ext",
            "check spool in file");

        TEST_ERROR(storagePathP(storage, STRDEF("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpoolWrite() - confirm write enabled");

        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpoolWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, storage, "storage cached");
        TEST_RESULT_PTR(storageSpoolWrite(), storage, "get cached storage");

        TEST_RESULT_VOID(storageNewWriteP(storage, writeFile), "writes are allowed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storagePg() - confirm settings for read, write and index functions");

        TEST_RESULT_PTR(storageHelper.storagePg, NULL, "pg storage not cached");
        TEST_ASSIGN(storage, storagePg(), "new pg storage");
        TEST_RESULT_PTR(storageHelper.storagePg[0], storage, "pg storage cached");
        TEST_RESULT_PTR(storagePg(), storage, "get cached pg storage");

        TEST_RESULT_STR_Z(storage->path, TEST_PATH "/db", "check pg storage path");
        TEST_RESULT_BOOL(storage->write, false, "check pg storage write");
        TEST_RESULT_STR_Z(storagePgIdx(1)->path, TEST_PATH "/db2", "check pg 2 storage path");

        TEST_RESULT_PTR(storageHelper.storagePgWrite, NULL, "pg write storage not cached");
        TEST_ASSIGN(storage, storagePgWrite(), "new pg write storage");
        TEST_RESULT_PTR(storageHelper.storagePgWrite[0], storage, "pg write storage cached");
        TEST_RESULT_PTR(storagePgWrite(), storage, "get cached pg write storage");
        TEST_RESULT_STR_Z(storagePgIdxWrite(1)->path, TEST_PATH "/db2", "check pg 2 write storage path");

        TEST_RESULT_STR_Z(storage->path, TEST_PATH "/db", "check pg write storage path");
        TEST_RESULT_BOOL(storage->write, true, "check pg write storage write");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool - helper fails because stanza is required");

        // Change the stanza to NULL, stanzaInit flag to false and make sure helper fails because stanza is required
        storageHelper.storageSpool = NULL;
        storageHelper.storageSpoolWrite = NULL;
        storageHelper.stanzaInit = false;
        storageHelper.stanza = NULL;

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_ERROR(storageSpool(), AssertError, "stanza cannot be NULL for this storage object");
        TEST_ERROR(storageSpoolWrite(), AssertError, "stanza cannot be NULL for this storage object");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet() and StorageDriverCifs"))
    {
        // Load configuration
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoType, "cifs");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid storage type");

        static const StorageHelper storageHelperListError[] = {{.type = STORAGE_POSIX_TYPE}, STORAGE_END_HELPER};
        storageHelperInit(storageHelperListError);

        TEST_ERROR(storageRepoGet(0, true), AssertError, "invalid storage type");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage configuration");

        // Set storage helper
        static const StorageHelper storageHelperList[] = {STORAGE_CIFS_HELPER, STORAGE_END_HELPER};
        storageHelperInit(storageHelperList);

        const Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, true), "get cifs repo storage");
        TEST_RESULT_UINT(storageType(storage), STORAGE_CIFS_TYPE, "check storage type");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), true, "check path feature");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write object path sync false");

        // Create a FileWrite object with path sync enabled and ensure that path sync is false in the write object
        StorageWrite *file = NULL;
        TEST_ASSIGN(file, storageNewWriteP(storage, STRDEF("somefile"), .noSyncPath = false), "new file write");

        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "path sync is disabled");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync result is noop");

        // Test the path sync function -- pass a bogus path to ensure that this is a noop
        TEST_RESULT_VOID(storagePathSyncP(storage, STRDEF(BOGUS_STR)), "path sync is a noop");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
