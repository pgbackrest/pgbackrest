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

    // Directory and file that cannot be accessed to test permissions errors
#ifdef TEST_CONTAINER_REQUIRED
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);
#endif // TEST_CONTAINER_REQUIRED

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
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 1000), false, "file does not exist");
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
    if (testBegin("storageInfoList()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageInfoListP(storageTest, STRDEF(BOGUS_STR), (StorageInfoListCallback)1, NULL, .errorOnMissing = true),
            PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        TEST_RESULT_BOOL(
            storageInfoListP(storageTest, STRDEF(BOGUS_STR), (StorageInfoListCallback)1, NULL), false, "ignore missing dir");

#ifdef TEST_CONTAINER_REQUIRED
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": [13] Permission denied", strZ(pathNoPerm));
#endif // TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storagePosixInfoListEntry()");

        HarnessStorageInfoListCallbackData callbackData =
        {
            .content = strNew(),
        };

        TEST_RESULT_VOID(
            storageSftpInfoListEntry(
                (StorageSftp *)storageDriver(storageTest), STRDEF("pg"), STRDEF("missing"), storageInfoLevelBasic,
                hrnStorageInfoListCallback, &callbackData),
            "missing path");
        TEST_RESULT_STR_Z(callbackData.content, "", "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

        storagePathCreateP(storageTest, STRDEF("pg"), .mode = 0766);

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(callbackData.content, ". {path, m=0766, u=" TEST_USER ", g=" TEST_GROUP "}\n", "check content");
        // jrt !!!
        // NOTE: in my tests with --vm=none the resultant file would have the incorrect permissions. The request was coming through
        // properly but the umask resulted in altered permissions.
        //
        // vagrant@pgbackrest-test:~$ umask
        // 0002
        //
        // Jul 13 16:38:33 localhost sftp-server[340225]: mkdir name "/home/vagrant/test/test-0/pg" mode 0766
        // But the directory would be created with 0764
        // test/test-0:
        // total 0
        // drwxrw-r-- 2 vagrant vagrant 40 Jul 14 15:37 pg
        //
        // Updating sshd_config to
        // Subsystem	sftp	/usr/lib/openssh/sftp-server -u 000
        // would result in the file being created with 0766 permissions

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

#ifdef TEST_CONTAINER_REQUIRED
        storagePathCreateP(storageTest, STRDEF("pg/.include"), .mode = 0755);
        HRN_SYSTEM("sudo chown 77777:77777 " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/file"), .modeFile = 0660), BUFSTRDEF("TESTDATA"));

        HRN_SYSTEM("ln -s ../file " TEST_PATH "/pg/link");
        HRN_SYSTEM("mkfifo -m 777 " TEST_PATH "/pg/pipe");

        callbackData = (HarnessStorageInfoListCallbackData)
        {
            .content = strNew(),
            .timestampOmit = true,
            .modeOmit = true,
            .modePath = 0766,
            .modeFile = 0600,
            .userOmit = true,
            .groupOmit = true,
        };

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path}\n"
#ifdef TEST_CONTAINER_REQUIRED
            ".include {path, m=0755, u=77777, g=77777}\n"
#endif // TEST_CONTAINER_REQUIRED
            "file {file, s=8, m=0660}\n"
            "link {link, d=../file}\n"
            "pipe {special}\n",
            "check content");

#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM("sudo rmdir " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse");

        storagePathCreateP(storageTest, STRDEF("pg/path"), .mode = 0700);
        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/path/file"), .modeFile = 0600), BUFSTRDEF("TESTDATA"));

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderDesc, .recurse = true),
            "recurse descending");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "pipe {special}\n"
            "path/file {file, s=8}\n"
            "path {path, m=0700}\n"
            "link {link, d=../file}\n"
            "file {file, s=8, m=0660}\n"
            ". {path}\n",
            "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path basic info - recurse");

        storagePathCreateP(storageTest, STRDEF("pg/path"), .mode = 0700);
        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/path/file"), .modeFile = 0600), BUFSTRDEF("TESTDATA"));

        callbackData.content = strNew();

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderDesc, .recurse = true),
            "recurse descending");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "pipe {special}\n"
            "path/file {file, s=8}\n"
            "path {path}\n"
            "link {link}\n"
            "file {file, s=8}\n"
            ". {path}\n",
            "check content");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - filter");

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc,
                .expression = STRDEF("^path")),
            "filter");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "path {path, m=0700}\n",
            "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc, .recurse = true,
                .expression = STRDEF("\\/file$")),
            "filter");
        TEST_RESULT_STR_Z(callbackData.content, "path/file {file, s=8}\n", "check content");
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

/*
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
        */





    FUNCTION_HARNESS_RETURN_VOID();
}
