/***********************************************************************************************************************************
Test Posix Storage
***********************************************************************************************************************************/
#include <unistd.h>
#include <utime.h>

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
String *
storageTestPathExpression(const String *expression, const String *path)
{
    String *result = NULL;

    if (strcmp(strPtr(expression), "<TEST>") == 0)
        result = strNewFmt("test%s", path == NULL ? "" : strPtr(strNewFmt("/%s", strPtr(path))));
    else if (strcmp(strPtr(expression), "<NULL>") != 0)
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    return result;
}

/***********************************************************************************************************************************
Macro to create a path and file that cannot be accessed
***********************************************************************************************************************************/
#define TEST_CREATE_NOPERM()                                                                                                       \
    TEST_RESULT_INT(                                                                                                               \
        system(                                                                                                                    \
            strPtr(strNewFmt("sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strPtr(pathNoPerm), strPtr(fileNoPerm), \
            strPtr(fileNoPerm)))),                                                                                                 \
        0, "create no perm path/file");

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);
    Storage *storageTmp = storagePosixNew(
        strNew("/tmp"), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);
    ioBufferSizeSet(2);

    // Directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    // Write file for testing if storage is read-only
    String *writeFile = strNewFmt("%s/writefile", testPath());

    // This test should always be first so the storage helper is uninitialized
    // *****************************************************************************************************************************
    if (testBegin("storageHelperDryRunInit()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is not initialized");

        TEST_ERROR(storagePgIdWrite(1), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoWrite(), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is true");

        storageHelperDryRunInit(true);

        TEST_ERROR(storagePgIdWrite(1), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoWrite(), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNew() and storageFree()"))
    {
        Storage *storageTest = NULL;
        TEST_ASSIGN(storageTest, storagePosixNew(strNew("/"), 0640, 0750, false, NULL), "new storage (defaults)");
        TEST_RESULT_STR_Z(storageTest->path, "/", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "     check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "    check expression function is not set");

        TEST_ASSIGN(
            storageTest, storagePosixNew(strNew("/path/to"), 0600, 0700, true, storageTestPathExpression),
            "new storage (non-default)");
        TEST_RESULT_STR_Z(storageTest->path, "/path/to", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "     check path mode");
        TEST_RESULT_BOOL(storageTest->write, true, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction != NULL, true, "    check expression function is set");

        TEST_RESULT_PTR(storageInterface(storageTest).info, storageTest->interface.info, "    check interface");
        TEST_RESULT_PTR(storageDriver(storageTest), storageTest->driver, "    check driver");
        TEST_RESULT_PTR(storageType(storageTest), storageTest->type, "    check type");
        TEST_RESULT_BOOL(storageFeature(storageTest, storageFeaturePath), true, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storageTest, storageFeatureCompress), true, "    check compress feature");

        TEST_RESULT_VOID(storageFree(storageTest), "free storage");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNew("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNew("missing"), .timeout = 100), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNew("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageExistsP(storageTest, fileNoPerm), FileOpenError,
            "unable to get info for path/file '%s': [13] Permission denied", strPtr(fileNoPerm));
        TEST_ERROR_FMT(
            storagePathExistsP(storageTest, fileNoPerm), FileOpenError,
            "unable to get info for path/file '%s': [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        String *pathExists = strNewFmt("%s/pathExists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");
        TEST_SYSTEM_FMT("mkdir %s", strPtr(pathExists));

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, pathExists), false, "not a file");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, fileExists), false, "not a path");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                sleepMSec(250);
                TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 1000), true, "file exists after wait");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO ": [13] Permission denied",
            strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/fileinfo", testPath());

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING ": [2] No such file or directory",
            strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, fileName, .ignoreMissing = true), "get file info (missing)");
        TEST_RESULT_BOOL(info.exists, false, "    check not exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info outside of base path");

        TEST_ERROR(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true), AssertError,
            hrnReplaceKey("absolute path '/etc' is not in base path '{[path]}'"));
        TEST_RESULT_BOOL(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true, .noPathEnforce = true).exists, true,
            "path not enforced");

        // -------------------------------------------------------------------------------------------------------------------------
        struct utimbuf utimeTest = {.actime = 1000000000, .modtime = 1555160000};
        THROW_ON_SYS_ERROR_FMT(utime(testPath(), &utimeTest) != 0, FileWriteError, "unable to set time for '%s'", testPath());

        TEST_ASSIGN(info, storageInfoP(storageTest, strNew(testPath())), "get path info");
        TEST_RESULT_PTR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0770, "    check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "    check mod time");
        TEST_RESULT_PTR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_UINT(info.userId, getuid(), "    check user id");
        TEST_RESULT_STR_Z(info.user, testUser(), "    check user");
        TEST_RESULT_UINT(info.groupId, getgid(), "    check group id");
        TEST_RESULT_STR_Z(info.group, testGroup(), "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), buffer), "put test file");

        utimeTest.modtime = 1555155555;
        THROW_ON_SYS_ERROR_FMT(utime(strPtr(fileName), &utimeTest) != 0, FileWriteError, "unable to set time for '%s'", testPath());

        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown 99999:99999 %s", strPtr(fileName)))), 0, "set invalid user/group");

        TEST_ASSIGN(info, storageInfoP(storageTest, fileName), "get file info");
        TEST_RESULT_PTR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "    check type");
        TEST_RESULT_UINT(info.size, 8, "    check size");
        TEST_RESULT_INT(info.mode, 0640, "    check mode");
        TEST_RESULT_INT(info.timeModified, 1555155555, "    check mod time");
        TEST_RESULT_PTR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_STR(info.user, NULL, "    check user");
        TEST_RESULT_STR(info.group, NULL, "    check group");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        String *linkName = strNewFmt("%s/testlink", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("ln -s /tmp %s", strPtr(linkName)))), 0, "create link");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName), "get link info");
        TEST_RESULT_PTR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0777, "    check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "/tmp", "    check link destination");
        TEST_RESULT_STR_Z(info.user, testUser(), "    check user");
        TEST_RESULT_STR_Z(info.group, testGroup(), "    check group");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName, .followLink = true), "get info from path pointed to by link");
        TEST_RESULT_PTR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0777, "    check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "    check link destination");
        TEST_RESULT_STR_Z(info.user, "root", "    check user");
        TEST_RESULT_STR_Z(info.group, "root", "    check group");

        storageRemoveP(storageTest, linkName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        String *pipeName = strNewFmt("%s/testpipe", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("mkfifo -m 666 %s", strPtr(pipeName)))), 0, "create pipe");

        TEST_ASSIGN(info, storageInfoP(storageTest, pipeName), "get info from pipe (special file)");
        TEST_RESULT_PTR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0666, "    check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "    check link destination");
        TEST_RESULT_STR_Z(info.user, testUser(), "    check user");
        TEST_RESULT_STR_Z(info.group, testGroup(), "    check group");

        storageRemoveP(storageTest, pipeName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfoList()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, strNew(BOGUS_STR), (StorageInfoListCallback)1, NULL, .errorOnMissing = true),
            PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, strPtr(strNewFmt("%s/BOGUS", testPath())));

        TEST_RESULT_BOOL(
            storageInfoListP(storageTest, strNew(BOGUS_STR), (StorageInfoListCallback)1, NULL), false, "ignore missing dir");

        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        HarnessStorageInfoListCallbackData callbackData =
        {
            .content = strNew(""),
        };

        TEST_RESULT_VOID(
            storagePosixInfoListEntry(
                (StoragePosix *)storageDriver(storageTest), strNew("pg"), strNew("missing"), storageInfoLevelBasic,
                hrnStorageInfoListCallback, &callbackData),
            "missing path");
        TEST_RESULT_STR_Z(callbackData.content, "", "    check content");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageTest, strNew("pg"), .mode = 0766);

        callbackData.content = strNew("");

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, strNew("pg"), hrnStorageInfoListCallback, &callbackData),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(
            callbackData.content, strPtr(strNewFmt(". {path, m=0766, u=%s, g=%s}\n", testUser(), testGroup())),
            "    check content");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageTest, strNew("pg/.include"), .mode = 0755);
        ASSERT(system(strPtr(strNewFmt("sudo chown 77777:77777 %s/pg/.include", testPath()))) == 0);

        storagePutP(storageNewWriteP(storageTest, strNew("pg/file"), .modeFile = 0660), BUFSTRDEF("TESTDATA"));

        ASSERT(system(strPtr(strNewFmt("ln -s ../file %s/pg/link", testPath()))) == 0);
        ASSERT(system(strPtr(strNewFmt("mkfifo -m 777 %s/pg/pipe", testPath()))) == 0);

        callbackData = (HarnessStorageInfoListCallbackData)
        {
            .content = strNew(""),
            .timestampOmit = true,
            .modeOmit = true,
            .modePath = 0766,
            .modeFile = 0600,
            .userOmit = true,
            .groupOmit = true,
        };

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, strNew("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path}\n"
            ".include {path, m=0755, u=77777, g=77777}\n"
            "file {file, s=8, m=0660}\n"
            "link {link, d=../file}\n"
            "pipe {special}\n",
            "    check content");

        // -------------------------------------------------------------------------------------------------------------------------
        ASSERT(system(strPtr(strNewFmt("sudo rmdir %s/pg/.include", testPath()))) == 0);
        storagePathCreateP(storageTest, strNew("pg/path"), .mode = 0700);
        storagePutP(storageNewWriteP(storageTest, strNew("pg/path/file"), .modeFile = 0600), BUFSTRDEF("TESTDATA"));

        callbackData.content = strNew("");

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, strNew("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderDesc, .recurse = true),
            "recurse descending");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "pipe {special}\n"
            "path/file {file, s=8}\n"
            "path {path, m=0700}\n"
            "link {link, d=../file}\n"
            "file {file, s=8, m=0660}\n"
            ". {path}\n",
            "    check content");

        // -------------------------------------------------------------------------------------------------------------------------
        callbackData.content = strNew("");

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, strNew("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc,
                .expression = STRDEF("^path")),
            "filter");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "path {path, m=0700}\n",
            "    check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageListP(storageTest, strNew(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
             strPtr(strNewFmt("%s/BOGUS", testPath())));

        TEST_RESULT_PTR(storageListP(storageTest, strNew(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListP(storageTest, strNew(BOGUS_STR))), 0, "empty list for missing dir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNew(".aaa.txt")), BUFSTRDEF("aaa")), "write aaa.text");
        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ", "), ".aaa.txt, noperm", "dir list");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNew("bbb.txt")), BUFSTRDEF("bbb")), "write bbb.text");
        TEST_RESULT_STR_Z(strLstJoin(storageListP(storageTest, NULL, .expression = strNew("^bbb")), ", "), "bbb.txt", "dir list");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageCopy()"))
    {
        String *sourceFile = strNewFmt("%s/source.txt", testPath());
        String *destinationFile = strNewFmt("%s/destination.txt", testPath());

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        TEST_ERROR_FMT(storageCopyP(source, destination), FileMissingError, STORAGE_ERROR_READ_MISSING, strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadP(storageTest, sourceFile, .ignoreMissing = true);

        TEST_RESULT_BOOL(storageCopyP(source, destination), false, "copy and ignore missing file");

        // -------------------------------------------------------------------------------------------------------------------------
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
        TEST_CREATE_NOPERM();

        String *sourceFile = strNewFmt("%s/source.txt", testPath());
        String *destinationFile = strNewFmt("%s/sub/destination.txt", testPath());

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMissingError,
            "unable to move missing source '%s': [2] No such file or directory", strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadP(storageTest, fileNoPerm);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMoveError,
            "unable to move '%s' to '%s': [13] Permission denied", strPtr(fileNoPerm), strPtr(destinationFile));

        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        storagePutP(storageNewWriteP(storageTest, sourceFile), buffer);

        source = storageNewReadP(storageTest, sourceFile);
        destination = storageNewWriteP(storageTest, destinationFile, .noCreatePath = true);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), PathMissingError,
            "unable to move '%s' to missing path '%s': [2] No such file or directory", strPtr(sourceFile),
            strPtr(strPath(destinationFile)));

        // -------------------------------------------------------------------------------------------------------------------------
        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, destinationFile), true, "check destination file exists");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, destinationFile))), "TESTFILE", "check destination contents");

        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = strNewFmt("%s/sub/destination2.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to same path");

        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to parent path (no sync)");

        // Move across filesystems
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = strNewFmt("/tmp/destination.txt");
        destination = storageNewWriteP(storageTmp, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to another filesystem");
        TEST_RESULT_BOOL(storageExistsP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTmp, destinationFile), true, "check destination file exists");

        // Move across filesystems without syncing the paths
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadP(storageTmp, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
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

        TEST_ASSIGN(storageTest, storagePosixNew(strNew("/"), 0640, 0750, false, NULL), "new storage /");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/", "    root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("/")), "/", "    same as root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("subdir")), "/subdir", "    simple subdir");

        TEST_ERROR(
            storagePathP(storageTest, strNew("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            storageTest, storagePosixNew(strNew("/path/to"), 0640, 0750, false, storageTestPathExpression),
            "new storage /path/to with expression");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/path/to", "    root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("/path/to")), "/path/to", "    absolute root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("is/a/subdir")), "/path/to/is/a/subdir", "    subdir");

        TEST_ERROR(
            storagePathP(storageTest, strNew("/bogus")), AssertError, "absolute path '/bogus' is not in base path '/path/to'");
        TEST_ERROR(
            storagePathP(storageTest, strNew("/path/toot")), AssertError,
            "absolute path '/path/toot' is not in base path '/path/to'");

        // Path enforcement disabled for a single call
        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("/bogus"), .noEnforce = true), "/bogus", "path enforce disabled");

        TEST_ERROR(storagePathP(storageTest, strNew("<TEST")), AssertError, "end > not found in path expression '<TEST'");
        TEST_ERROR(
            storagePathP(storageTest, strNew("<TEST>" BOGUS_STR)), AssertError,
            "'/' should separate expression and path '<TEST>BOGUS'");

        TEST_RESULT_STR_Z(storagePathP(storageTest, strNew("<TEST>")), "/path/to/test", "    expression");
        TEST_ERROR(strPtr(storagePathP(storageTest, strNew("<TEST>/"))), AssertError, "path '<TEST>/' should not end in '/'");

        TEST_RESULT_STR_Z(
            storagePathP(storageTest, strNew("<TEST>/something")), "/path/to/test/something", "    expression with path");

        TEST_ERROR(storagePathP(storageTest, strNew("<NULL>")), AssertError, "evaluated path '<NULL>' cannot be null");

        TEST_ERROR(storagePathP(storageTest, strNew("<WHATEVS>")), AssertError, "invalid expression '<WHATEVS>'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, strNew("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub1")), "create sub1 again");
        TEST_ERROR_FMT(
            storagePathCreateP(storageTest, strNew("sub1"), .errorOnExists = true), PathCreateError,
            "unable to create path '%s/sub1': [17] File exists", testPath());

        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoP(storageTest, strNew("sub2")).mode, 0777, "check sub2 dir mode");

        TEST_ERROR_FMT(
            storagePathCreateP(storageTest, strNew("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "unable to create path '%s/sub3/sub4': [2] No such file or directory", testPath());
        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub3/sub4")), "create sub3/sub4");

        TEST_RESULT_INT(system(strPtr(strNewFmt("rm -rf %s/sub*", testPath()))), 0, "remove sub paths");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove1 = strNewFmt("%s/remove1", testPath());

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strPtr(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove2 = strNewFmt("%s/remove2", strPtr(pathRemove1));

        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo mkdir -p -m 700 %s", strPtr(pathRemove2)))), 0, "create noperm paths");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2), PathRemoveError, STORAGE_ERROR_PATH_REMOVE ": [13] Permission denied",
            strPtr(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 777 %s", strPtr(pathRemove1)))), 0, "top path can be removed");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strPtr(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileRemove = strNewFmt("%s/remove.txt", strPtr(pathRemove2));

        TEST_RESULT_INT(
            system(strPtr(strNewFmt(
                "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strPtr(pathRemove2), strPtr(fileRemove),
                strPtr(fileRemove)))),
            0, "add no perm file");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_FILE ": [13] Permission denied", strPtr(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 777 %s", strPtr(pathRemove2)))), 0, "bottom path can be removed");

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("mkdir -p %s", strPtr(pathRemove2)))), 0, "create subpaths");

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storagePathSyncP(storageTest, fileNoPerm), PathOpenError, STORAGE_ERROR_PATH_SYNC_OPEN ": [13] Permission denied",
            strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathName = strNewFmt("%s/testpath", testPath());

        TEST_ERROR_FMT(
            storagePathSyncP(storageTest, pathName), PathMissingError, STORAGE_ERROR_PATH_SYNC_MISSING, strPtr(pathName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storagePathSyncP(
                storagePosixNew(strNew("/"), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL), strNew("/proc")),
            PathSyncError, STORAGE_ERROR_PATH_SYNC ": [22] Invalid argument", "/proc");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateP(storageTest, pathName), "create path to sync");
        TEST_RESULT_VOID(storagePathSyncP(storageTest, pathName), "sync path");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        StorageRead *file = NULL;
        String *fileName = strNewFmt("%s/readtest.txt", testPath());

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileName)))), 0, "create read file");

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "    open file");
        TEST_RESULT_INT(
            ioReadHandle(storageReadIo(file)), ((StorageReadPosix *)file->driver)->handle, "check read handle");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "    close file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        String *fileName = strNewFmt("%s/sub1/testfile", testPath());

        TEST_CREATE_NOPERM();
        StorageWrite *file = NULL;

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file (defaults)");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": [13] Permission denied",
            strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = strNew(testUser()), .group = strNew(testGroup()), .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");
        TEST_RESULT_INT(
            ioWriteHandle(storageWriteIo(file)), ((StorageWritePosix *)file->driver)->handle, "check write handle");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "   close file");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "    check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "    check file mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).timeModified, 1, "    check file modified times");

        // Test that a premature free (from error or otherwise) does not rename the file
        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub1/testfile-abort", testPath());
        String *fileNameTmp = strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strPtr(fileName));

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .user = strNew(testUser())), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), BUFSTRDEF("TESTDATA")), "write data");
        TEST_RESULT_VOID(ioWriteFlush(storageWriteIo(file)), "flush data");
        TEST_RESULT_VOID(ioWriteFree(storageWriteIo(file)), "   free file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileName), false, "destination file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, fileNameTmp), true, "destination tmp file exists");
        TEST_RESULT_UINT(storageInfoP(storageTest, fileNameTmp).size, 8, "    check temp file size");

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/testfile", testPath());

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .modePath = 0700, .modeFile = 0600, .group = strNew(testGroup())),
            "new write file (set mode)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "   close file");
        TEST_RESULT_VOID(storageWritePosixClose(file->driver), "   close file again");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "    check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "    check file mode");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storagePosixNew(strNew("/"), 0640, 0750, true, NULL);

        TEST_ERROR_FMT(
            storageGetP(storageNewReadP(storageTest, strNew(testPath()))), FileReadError,
            "unable to read '%s': [21] Is a directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        String *emptyFile = strNewFmt("%s/test.empty", testPath());
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_BOOL(storageExistsP(storageTest, emptyFile), true, "check empty file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/test.txt", testPath())), buffer), "put test file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR), .ignoreMissing = true)),
            NULL, "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.empty", testPath()))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            buffer, storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.txt", testPath())), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        TEST_ERROR_FMT(
            storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.txt", testPath())), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '%s/test.txt'", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(2);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        ioBufferSizeSet(2);

        TEST_ASSIGN(
            buffer, storageGetP(storageNewReadP(storageTest, strNewFmt("%s/test.txt", testPath()), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveP(storageTest, strNew("missing")), "remove missing file");
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, strNew("missing"), .errorOnMissing = true), FileRemoveError,
            "unable to remove '%s/missing': [2] No such file or directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");

        TEST_RESULT_VOID(storageRemoveP(storageTest, fileExists), "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, fileNoPerm), FileRemoveError,
            "unable to remove '%s': [13] Permission denied", strPtr(fileNoPerm));
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
        TEST_CREATE_NOPERM();
        StorageRead *file = NULL;

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true, .limit = VARUINT64(44)), "new read file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(file), true, "    check ignore missing");
        TEST_RESULT_STR(storageReadName(file), fileNoPerm, "    check name");
        TEST_RESULT_UINT(varUInt64(storageReadLimit(file)), 44, "    check limit");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.file", testPath());

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "   missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *outBuffer = bufNew(2);
        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), expectedBuffer), "write test file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "   open file");

        // Close the file handle so operations will fail
        close(((StorageReadPosix *)file->driver)->handle);

        TEST_ERROR_FMT(
            ioRead(storageReadIo(file), outBuffer), FileReadError,
            "unable to read '%s': [9] Bad file descriptor", strPtr(fileName));

        // Set file handle to -1 so the close on free will not fail
        ((StorageReadPosix *)file->driver)->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(0);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageReadMove(storageNewReadP(storageTest, fileName), memContextPrior()), "new read file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "   open file");
        TEST_RESULT_STR(storageReadName(file), fileName, "    check file name");
        TEST_RESULT_STR_Z(storageReadType(file), "posix", "    check file type");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), false, "    check file contents (not all loaded yet)");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "    no data to load");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "    buffer is empty");

        TEST_RESULT_VOID(storageReadPosix(file->driver, outBuffer, true), "    no data to load from driver either");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "    buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "    eof");
        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "    still eof");

        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "    close file");

        TEST_RESULT_VOID(storageReadFree(storageNewReadP(storageTest, fileName)), "   free file");

        TEST_RESULT_VOID(storageReadMove(NULL, memContextTop()), "   move null file");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageWrite"))
    {
        TEST_CREATE_NOPERM();
        StorageWrite *file = NULL;

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noCreatePath = true, .noSyncFile = true,
                .noSyncPath = true, .noAtomic = true),
            "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(file), false, "    check atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(file), false, "    check create path");
        TEST_RESULT_INT(storageWriteModeFile(file), 0444, "    check mode file");
        TEST_RESULT_INT(storageWriteModePath(file), 0555, "    check mode path");
        TEST_RESULT_STR(storageWriteName(file), fileNoPerm, "    check name");
        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "    check sync path");
        TEST_RESULT_BOOL(storageWriteSyncFile(file), false, "    check sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": [13] Permission denied",
            strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/sub1/test.file", testPath());

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noCreatePath = true, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileTmp = strNewFmt("%s.pgbackrest.tmp", strPtr(fileName));
        ioBufferSizeSet(10);
        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "    check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");

        // Close the file handle so operations will fail
        close(((StorageWritePosix *)file->driver)->handle);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        TEST_ERROR_FMT(
            storageWritePosix(file->driver, buffer), FileWriteError,
            "unable to write '%s.pgbackrest.tmp': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageWritePosixClose(file->driver), FileSyncError, STORAGE_ERROR_WRITE_SYNC ": [9] Bad file descriptor",
            strPtr(fileTmp));

        // Disable file sync so close() can be reached
        ((StorageWritePosix *)file->driver)->interface.syncFile = false;

        TEST_ERROR_FMT(
            storageWritePosixClose(file->driver), FileCloseError, STORAGE_ERROR_WRITE_CLOSE ": [9] Bad file descriptor",
            strPtr(fileTmp));

        // Set file handle to -1 so the close on free with not fail
        ((StorageWritePosix *)file->driver)->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "    check file name");
        TEST_RESULT_STR_Z(storageWriteType(file), "posix", "    check file type");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");

        // Rename the file back to original name from tmp -- this will cause the rename in close to fail
        TEST_RESULT_INT(rename(strPtr(fileTmp), strPtr(fileName)), 0, " rename tmp file");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileMoveError,
            "unable to move '%s' to '%s': [2] No such file or directory", strPtr(fileTmp), strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        ((StorageWritePosix *)file->driver)->handle = -1;

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageWriteMove(storageNewWriteP(storageTest, fileName), memContextPrior()), "new write file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), NULL), "   write null buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), bufNew(0)), "   write zero buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "   write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "   close file");
        TEST_RESULT_VOID(storageWriteFree(storageNewWriteP(storageTest, fileName)), "   free file");
        TEST_RESULT_VOID(storageWriteMove(NULL, memContextTop()), "   move null file");

        Buffer *expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "    check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "    check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/test.file", testPath());

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "   write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "   close file");

        expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "    check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "    check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageLocal() and storageLocalWrite()"))
    {
        const Storage *storage = NULL;

        TEST_RESULT_PTR(storageHelper.storageLocal, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocal(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocal, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocal(), storage, "get cached storage");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), "/", "check base path");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
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
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ERROR(storageRepoGet(strNew(BOGUS_STR), false), AssertError, "invalid storage type 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        const Storage *storage = NULL;

        TEST_RESULT_PTR(storageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageRepo, storage, "repo storage cached");
        TEST_RESULT_PTR(storageRepo(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(storagePathP(storage, strNew("<BOGUS>/path")), AssertError, "invalid expression '<BOGUS>'");
        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), testPath(), "check base path");
        TEST_RESULT_STR(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), strNewFmt("%s/archive/db", testPath()), "check archive path");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_ARCHIVE "/simple")), strNewFmt("%s/archive/db/simple", testPath()),
            "check simple path");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/700000007000000070000000")),
            strNewFmt("%s/archive/db/9.4-1/7000000070000000/700000007000000070000000", testPath()), "check segment path");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/00000008.history")),
            strNewFmt("%s/archive/db/9.4-1/00000008.history", testPath()), "check history path");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/000000010000014C0000001A.00000028.backup")),
            strNewFmt("%s/archive/db/9.4-1/000000010000014C/000000010000014C0000001A.00000028.backup", testPath()),
            "check archive backup path");
        TEST_RESULT_STR(
            storagePathP(storage, STORAGE_REPO_BACKUP_STR), strNewFmt("%s/backup/db", testPath()), "check backup path");

        // Change the stanza to NULL with the stanzaInit flag still true, make sure helper does not fail when stanza option not set
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        harnessCfgLoad(cfgCmdInfo, argList);

        TEST_ASSIGN(storage, storageRepo(), "new repo storage no stanza");
        TEST_RESULT_PTR(storageHelper.stanza, NULL, "stanza NULL");

        TEST_RESULT_STR(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), strNewFmt("%s/archive", testPath()),
            "check archive path - NULL stanza");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_ARCHIVE "/simple")),
            strNewFmt("%s/archive/simple", testPath()), "check simple archive path - NULL stanza");
        TEST_RESULT_STR(
            storagePathP(storage, STORAGE_REPO_BACKUP_STR), strNewFmt("%s/backup", testPath()),
            "check backup path - NULL stanza");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_REPO_BACKUP "/simple")),
            strNewFmt("%s/backup/simple", testPath()), "check simple backup path - NULL stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageRepoWrite, NULL, "repo write storage not cached");
        TEST_ASSIGN(storage, storageRepoWrite(), "new write storage");
        TEST_RESULT_PTR(storageHelper.storageRepoWrite, storage, "repo write storage cached");
        TEST_RESULT_PTR(storageRepoWrite(), storage, "get cached storage");

        TEST_RESULT_BOOL(storage->write, true, "get write enabled");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSpool() and storageSpoolWrite()"))
    {
        const Storage *storage = NULL;

        // Load configuration to set spool-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAdd(argList, strNewFmt("--pg2-path=%s/db2", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpool, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(storagePathP(storage, NULL), testPath(), "check base path");
        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_SPOOL_ARCHIVE_OUT)), strNewFmt("%s/archive/db/out", testPath()),
            "check spool out path");
        TEST_RESULT_STR(
            storagePathP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, "file.ext")),
            strNewFmt("%s/archive/db/out/file.ext", testPath()), "check spool out file");

        TEST_RESULT_STR(
            storagePathP(storage, strNew(STORAGE_SPOOL_ARCHIVE_IN)), strNewFmt("%s/archive/db/in", testPath()),
            "check spool in path");
        TEST_RESULT_STR(
            storagePathP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_IN, "file.ext")),
            strNewFmt("%s/archive/db/in/file.ext", testPath()), "check spool in file");

        TEST_ERROR(storagePathP(storage, strNew("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpoolWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, storage, "storage cached");
        TEST_RESULT_PTR(storageSpoolWrite(), storage, "get cached storage");

        TEST_RESULT_VOID(storageNewWriteP(storage, writeFile), "writes are allowed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storagePg, NULL, "pg storage not cached");
        TEST_ASSIGN(storage, storagePg(), "new pg storage");
        TEST_RESULT_PTR(storageHelper.storagePg[0], storage, "pg storage cached");
        TEST_RESULT_PTR(storagePg(), storage, "get cached pg storage");

        TEST_RESULT_STR(storage->path, strNewFmt("%s/db", testPath()), "check pg storage path");
        TEST_RESULT_BOOL(storage->write, false, "check pg storage write");
        TEST_RESULT_STR(storagePgId(2)->path, strNewFmt("%s/db2", testPath()), "check pg 2 storage path");

        TEST_RESULT_PTR(storageHelper.storagePgWrite, NULL, "pg write storage not cached");
        TEST_ASSIGN(storage, storagePgWrite(), "new pg write storage");
        TEST_RESULT_PTR(storageHelper.storagePgWrite[0], storage, "pg write storage cached");
        TEST_RESULT_PTR(storagePgWrite(), storage, "get cached pg write storage");
        TEST_RESULT_STR(storagePgIdWrite(2)->path, strNewFmt("%s/db2", testPath()), "check pg 2 write storage path");

        TEST_RESULT_STR(storage->path, strNewFmt("%s/db", testPath()), "check pg write storage path");
        TEST_RESULT_BOOL(storage->write, true, "check pg write storage write");

        // Pg storage from another host id
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptHostId, cfgSourceParam, VARUINT64(2));
        cfgOptionValidSet(cfgOptHostId, true);

        TEST_RESULT_STR(storagePg()->path, strNewFmt("%s/db2", testPath()), "check pg-2 storage path");

        // Change the stanza to NULL, stanzaInit flag to false and make sure helper fails because stanza is required
        // -------------------------------------------------------------------------------------------------------------------------
        storageHelper.storageSpool = NULL;
        storageHelper.storageSpoolWrite = NULL;
        storageHelper.stanzaInit = false;
        storageHelper.stanza = NULL;

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        harnessCfgLoad(cfgCmdInfo, argList);

        TEST_ERROR(storageSpool(), AssertError, "stanza cannot be NULL for this storage object");
        TEST_ERROR(storageSpoolWrite(), AssertError, "stanza cannot be NULL for this storage object");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
