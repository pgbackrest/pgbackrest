/***********************************************************************************************************************************
Test Posix Storage Driver
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/time.h"
#include "storage/fileRead.h"
#include "storage/fileWrite.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

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
    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));
    Storage *storageTmp = storageDriverPosixInterface(
        storageDriverPosixNew(strNew("/tmp"), 0, 0, true, NULL));
    ioBufferSizeSet(2);

    // Directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    // Write file for testing if storage is read-only
    String *writeFile = strNewFmt("%s/writefile", testPath());

    // *****************************************************************************************************************************
    if (testBegin("storageDriverPosixFile*()"))
    {
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storageDriverPosixFileOpen(pathNoPerm, O_RDONLY, 0, false, false, "test"), PathOpenError,
            "unable to open '%s' for test: [13] Permission denied", strPtr(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.file", testPath());

        TEST_ERROR_FMT(
            storageDriverPosixFileOpen(fileName, O_RDONLY, 0, false, true, "read"), FileMissingError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        TEST_RESULT_INT(storageDriverPosixFileOpen(fileName, O_RDONLY, 0, true, true, "read"), -1, "missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        int handle = -1;

        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileName)))), 0, "create read file");
        TEST_ASSIGN(handle, storageDriverPosixFileOpen(fileName, O_RDONLY, 0, false, true, "read"), "open read file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageDriverPosixFileSync(-99, fileName, false, false), PathSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageDriverPosixFileSync(-99, fileName, true, true), FileSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));

        TEST_RESULT_VOID(storageDriverPosixFileSync(handle, fileName, true, false), "sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageDriverPosixFileClose(-99, fileName, true), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        TEST_RESULT_VOID(storageDriverPosixFileClose(handle, fileName, true), "close file");

        TEST_RESULT_INT(system(strPtr(strNewFmt("rm %s", strPtr(fileName)))), 0, "remove read file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNew() and storageFree()"))
    {
        Storage *storageTest = NULL;
        TEST_ASSIGN(
            storageTest, storageDriverPosixInterface(storageDriverPosixNew(strNew("/"), 0640, 0750, false, NULL)), "new storage (defaults)");
        TEST_RESULT_STR(strPtr(storageTest->path), "/", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "     check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "    check expression function is not set");

        TEST_ASSIGN(
            storageTest, storageDriverPosixInterface(storageDriverPosixNew(strNew("/path/to"), 0600, 0700, true, storageTestPathExpression)),
            "new storage (non-default)");
        TEST_RESULT_STR(strPtr(storageTest->path), "/path/to", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "     check path mode");
        TEST_RESULT_BOOL(storageTest->write, true, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction != NULL, true, "    check expression function is set");

        TEST_RESULT_VOID(storageFree(storageTest), "free storage");
        TEST_RESULT_VOID(storageFree(NULL), "free null storage");

        TEST_RESULT_VOID(storageDriverPosixFree(storageDriverPosixNew(strNew("/"), 0640, 0750, false, NULL)), "free posix storage");
        TEST_RESULT_VOID(storageDriverPosixFree(NULL), "free null posix storage");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNew("missing"), .timeout = .1), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageExistsNP(storageTest, fileNoPerm), FileOpenError,
            "unable to stat '%s': [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                sleepMSec(250);
                TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");
            }

            HARNESS_FORK_PARENT()
            {
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 1), true, "file exists after wait");
            }
        }
        HARNESS_FORK_END();

        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storageInfoNP(storageTest, fileNoPerm), FileOpenError,
            "unable to get info for '%s': [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/fileinfo", testPath());

        TEST_ERROR_FMT(
            storageInfoNP(storageTest, fileName), FileOpenError,
            "unable to get info for '%s': [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, fileName, .ignoreMissing = true), "get file info (missing)");
        TEST_RESULT_BOOL(info.exists, false, "    check not exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNew(testPath())), "get path info");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "    check type");
        TEST_RESULT_INT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0770, "    check mode");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewZ("TESTFILE");
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, fileName), buffer), "put test file");

        TEST_ASSIGN(info, storageInfoNP(storageTest, fileName), "get file info");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "    check type");
        TEST_RESULT_INT(info.size, 8, "    check size");
        TEST_RESULT_INT(info.mode, 0640, "    check mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        String *linkName = strNewFmt("%s/testlink", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("ln -s /tmp %s", strPtr(linkName)))), 0, "create link");

        TEST_ASSIGN(info, storageInfoNP(storageTest, linkName), "get link info");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "    check type");
        TEST_RESULT_INT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0777, "    check mode");

        storageRemoveP(storageTest, linkName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        String *pipeName = strNewFmt("%s/testpipe", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("mkfifo %s", strPtr(pipeName)))), 0, "create pipe");

        TEST_ERROR_FMT(storageInfoNP(storageTest, pipeName), FileInfoError, "invalid type for '%s'", strPtr(pipeName));

        storageRemoveP(storageTest, pipeName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageListP(storageTest, strNew(BOGUS_STR), .errorOnMissing = true), PathOpenError,
            "unable to open path '%s/BOGUS' for read: [2] No such file or directory", testPath());

        TEST_RESULT_PTR(storageListNP(storageTest, strNew(BOGUS_STR)), NULL, "ignore missing dir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageListNP(storageTest, pathNoPerm), PathOpenError,
            "unable to open path '%s' for read: [13] Permission denied", strPtr(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListNP(storageTest, pathNoPerm), PathOpenError,
            "unable to open path '%s' for read: [13] Permission denied", strPtr(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNew("aaa.txt")), bufNewZ("aaa")), "write aaa.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListNP(storageTest, NULL), ", ")), "aaa.txt, noperm",
            "dir list");

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNew("bbb.txt")), bufNewZ("bbb")), "write bbb.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListP(storageTest, NULL, .expression = strNew("^bbb")), ", ")), "bbb.txt", "dir list");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageCopy()"))
    {
        String *sourceFile = strNewFmt("%s/source.txt", testPath());
        String *destinationFile = strNewFmt("%s/destination.txt", testPath());

        StorageFileRead *source = storageNewReadNP(storageTest, sourceFile);
        StorageFileWrite *destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageCopyNP(source, destination), FileMissingError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadP(storageTest, sourceFile, .ignoreMissing = true);

        TEST_RESULT_BOOL(storageCopyNP(source, destination), false, "copy and ignore missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *expectedBuffer = bufNewZ("TESTFILE\n");
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, sourceFile), expectedBuffer), "write source file");

        source = storageNewReadNP(storageTest, sourceFile);

        TEST_RESULT_BOOL(storageCopyNP(source, destination), true, "copy file");
        TEST_RESULT_BOOL(bufEq(expectedBuffer, storageGetNP(storageNewReadNP(storageTest, destinationFile))), true, "check file");

        storageRemoveP(storageTest, sourceFile, .errorOnMissing = true);
        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageMove()"))
    {
        TEST_CREATE_NOPERM();

        String *sourceFile = strNewFmt("%s/source.txt", testPath());
        String *destinationFile = strNewFmt("%s/sub/destination.txt", testPath());

        StorageFileRead *source = storageNewReadNP(storageTest, sourceFile);
        StorageFileWrite *destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageMoveNP(storageTest, source, destination), FileMissingError,
            "unable to move missing file '%s': [2] No such file or directory", strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadNP(storageTest, fileNoPerm);

        TEST_ERROR_FMT(
            storageMoveNP(storageTest, source, destination), FileMoveError,
            "unable to move '%s' to '%s': [13] Permission denied", strPtr(fileNoPerm), strPtr(destinationFile));

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewZ("TESTFILE");
        storagePutNP(storageNewWriteNP(storageTest, sourceFile), buffer);

        source = storageNewReadNP(storageTest, sourceFile);
        destination = storageNewWriteP(storageTest, destinationFile, .noCreatePath = true);

        TEST_ERROR_FMT(
            storageMoveNP(storageTest, source, destination), PathMissingError,
            "unable to move '%s' to missing path '%s': [2] No such file or directory", strPtr(sourceFile),
            strPtr(strPath(destinationFile)));

        // -------------------------------------------------------------------------------------------------------------------------
        destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveNP(storageTest, source, destination), "move file to subpath");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, destinationFile), true, "check destination file exists");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, destinationFile)))), "TESTFILE",
            "check destination contents");

        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTest, sourceFile);
        destinationFile = strNewFmt("%s/sub/destination2.txt", testPath());
        destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveNP(storageTest, source, destination), "move file to same path");

        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTest, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveNP(storageTest, source, destination), "move file to parent path (no sync)");

        // Move across filesystems
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTest, sourceFile);
        destinationFile = strNewFmt("/tmp/destination.txt");
        destination = storageNewWriteNP(storageTmp, destinationFile);

        TEST_RESULT_VOID(storageMoveNP(storageTest, source, destination), "move file to another filesystem");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTmp, destinationFile), true, "check destination file exists");

        // Move across fileystems without syncing the paths
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTmp, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveNP(storageTest, source, destination), "move file to another filesystem without path sync");
        TEST_RESULT_BOOL(storageExistsNP(storageTmp, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, destinationFile), true, "check destination file exists");

        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storageTest = NULL;

        TEST_ASSIGN(storageTest, storageDriverPosixInterface(storageDriverPosixNew(strNew("/"), 0640, 0750, false, NULL)), "new storage /");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, NULL)), "/", "    root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("/"))), "/", "    same as root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("subdir"))), "/subdir", "    simple subdir");

        TEST_ERROR(
            storagePathNP(storageTest, strNew("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            storageTest, storageDriverPosixInterface(storageDriverPosixNew(strNew("/path/to"), 0640, 0750, false, storageTestPathExpression)),
            "new storage /path/to with expression");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, NULL)), "/path/to", "    root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("/path/to"))), "/path/to", "    absolute root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("is/a/subdir"))), "/path/to/is/a/subdir", "    subdir");

        TEST_ERROR(
            storagePathNP(storageTest, strNew("/bogus")), AssertError, "absolute path '/bogus' is not in base path '/path/to'");
        TEST_ERROR(
            storagePathNP(storageTest, strNew("/path/toot")), AssertError,
            "absolute path '/path/toot' is not in base path '/path/to'");

        TEST_ERROR(storagePathNP(storageTest, strNew("<TEST")), AssertError, "end > not found in path expression '<TEST'");
        TEST_ERROR(
            storagePathNP(storageTest, strNew("<TEST>" BOGUS_STR)), AssertError,
            "'/' should separate expression and path '<TEST>BOGUS'");

        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("<TEST>"))), "/path/to/test", "    expression");
        TEST_ERROR(strPtr(storagePathNP(storageTest, strNew("<TEST>/"))), AssertError, "path '<TEST>/' should not end in '/'");

        TEST_RESULT_STR(
            strPtr(storagePathNP(storageTest, strNew("<TEST>/something"))), "/path/to/test/something", "    expression with path");

        TEST_ERROR(storagePathNP(storageTest, strNew("<NULL>")), AssertError, "evaluated path '<NULL>' cannot be null");

        TEST_ERROR(storagePathNP(storageTest, strNew("<WHATEVS>")), AssertError, "invalid expression '<WHATEVS>'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNew("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoNP(storageTest, strNew("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNew("sub1")), "create sub1 again");
        TEST_ERROR_FMT(
            storagePathCreateP(storageTest, strNew("sub1"), .errorOnExists = true), PathCreateError,
            "unable to create path '%s/sub1': [17] File exists", testPath());

        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, strNew("sub2")).mode, 0777, "check sub2 dir mode");

        TEST_ERROR_FMT(
            storagePathCreateP(storageTest, strNew("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "unable to create path '%s/sub3/sub4': [2] No such file or directory", testPath());
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNew("sub3/sub4")), "create sub3/sub4");

        TEST_RESULT_INT(system(strPtr(strNewFmt("rm -rf %s/sub*", testPath()))), 0, "remove sub paths");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove1 = strNewFmt("%s/remove1", testPath());

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            "unable to remove path '%s': [2] No such file or directory", strPtr(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove2 = strNewFmt("%s/remove2", strPtr(pathRemove1));

        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo mkdir -p -m 700 %s", strPtr(pathRemove2)))), 0, "create noperm paths");

        TEST_ERROR_FMT(
            storagePathRemoveNP(storageTest, pathRemove2), PathRemoveError,
            "unable to remove path '%s': [13] Permission denied", strPtr(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            "unable to open path '%s' for read: [13] Permission denied", strPtr(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 777 %s", strPtr(pathRemove1)))), 0, "top path can be removed");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            "unable to open path '%s' for read: [13] Permission denied", strPtr(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileRemove = strNewFmt("%s/remove.txt", strPtr(pathRemove2));

        TEST_RESULT_INT(
            system(strPtr(strNewFmt(
                "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strPtr(pathRemove2), strPtr(fileRemove),
                strPtr(fileRemove)))),
            0, "add no perm file");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            "unable to remove path/file '%s': [13] Permission denied", strPtr(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 777 %s", strPtr(pathRemove2)))), 0, "bottom path can be removed");

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, pathRemove1), false, "path is removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("mkdir -p %s", strPtr(pathRemove2)))), 0, "create subpaths");

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, pathRemove1), false, "path is removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(
            storagePathSyncNP(storageTest, fileNoPerm), PathOpenError,
            "unable to open '%s' for sync: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathName = strNewFmt("%s/testpath", testPath());

        TEST_ERROR_FMT(
            storagePathSyncNP(storageTest, pathName), PathMissingError,
            "unable to open '%s' for sync: [2] No such file or directory", strPtr(pathName));

        TEST_RESULT_VOID(storagePathSyncP(storageTest, pathName, .ignoreMissing = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, pathName), "create path to sync");
        TEST_RESULT_VOID(storagePathSyncNP(storageTest, pathName), "sync path");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        StorageFileRead *file = NULL;
        String *fileName = strNewFmt("%s/readtest.txt", testPath());

        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(
            ioReadOpen(storageFileReadIo(file)), FileMissingError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileName)))), 0, "create read file");

        TEST_RESULT_BOOL(ioReadOpen(storageFileReadIo(file)), true, "    open file");
        TEST_RESULT_VOID(ioReadClose(storageFileReadIo(file)), "    close file");

        // -------------------------------------------------------------------------------------------------------------------------
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .filterGroup = filterGroup), "new read file with filters");
        TEST_RESULT_PTR(ioReadFilterGroup(storageFileReadIo(file)), filterGroup, "    check filter group is set");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        TEST_CREATE_NOPERM();
        StorageFileWrite *file = NULL;

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file (defaults)");
        TEST_ERROR_FMT(
            ioWriteOpen(storageFileWriteIo(file)), FileOpenError,
            "unable to open '%s' for write: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/sub1/testfile", testPath());

        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWriteClose(storageFileWriteIo(file)), "   close file");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0750, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0640, "    check file mode");

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/testfile", testPath());

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .modePath = 0700, .modeFile = 0600), "new write file (set mode)");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWriteClose(storageFileWriteIo(file)), "   close file");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0700, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0600, "    check file mode");

        // -------------------------------------------------------------------------------------------------------------------------
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .filterGroup = filterGroup), "new write file with filters");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWriteClose(storageFileWriteIo(file)), "   close file");
        TEST_RESULT_PTR(ioWriteFilterGroup(storageFileWriteIo(file)), filterGroup, "    check filter group is set");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storageDriverPosixInterface(storageDriverPosixNew(strNew("/"), 0640, 0750, true, NULL));

        TEST_ERROR_FMT(
            storageGetNP(storageNewReadNP(storageTest, strNew(testPath()))), FileReadError,
            "unable to read '%s': [21] Is a directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        String *emptyFile = strNewFmt("%s/test.empty", testPath());
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, emptyFile), true, "check empty file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewZ("TESTFILE\n");

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/test.txt", testPath())), buffer), "put test file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(
            storageGetNP(storageNewReadP(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR), .ignoreMissing = true)),
            NULL, "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetNP(storageNewReadNP(storageTest, strNewFmt("%s/test.empty", testPath()))), "get empty");
        TEST_RESULT_INT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetNP(storageNewReadNP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            buffer, storageGetP(storageNewReadNP(storageTest, strNewFmt("%s/test.txt", testPath())), .exactSize = 4), "get exact");
        TEST_RESULT_INT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        TEST_ERROR_FMT(
            storageGetP(storageNewReadNP(storageTest, strNewFmt("%s/test.txt", testPath())), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '%s/test.txt'", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(2);

        TEST_ASSIGN(buffer, storageGetNP(storageNewReadNP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        TEST_CREATE_NOPERM();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("missing")), "remove missing file");
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, strNew("missing"), .errorOnMissing = true), FileRemoveError,
            "unable to remove '%s/missing': [2] No such file or directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");

        TEST_RESULT_VOID(storageRemoveNP(storageTest, fileExists), "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageRemoveNP(storageTest, fileNoPerm), FileRemoveError,
            "unable to remove '%s': [13] Permission denied", strPtr(fileNoPerm));
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageFileRead"))
    {
        TEST_CREATE_NOPERM();
        StorageFileRead *file = NULL;

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_PTR(storageFileReadDriver(file), file->driver, "    check driver");
        TEST_RESULT_BOOL(storageFileReadIgnoreMissing(file), true, "    check ignore missing");
        TEST_RESULT_STR(strPtr(storageFileReadName(file)), strPtr(fileNoPerm), "    check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(
            ioReadOpen(storageFileReadIo(file)), FileOpenError,
            "unable to open '%s' for read: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.file", testPath());

        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(
            ioReadOpen(storageFileReadIo(file)), FileMissingError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(ioReadOpen(storageFileReadIo(file)), false, "   missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *outBuffer = bufNew(2);
        Buffer *expectedBuffer = bufNewZ("TESTFILE\n");
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, fileName), expectedBuffer), "write test file");

        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileName), "new read file");
        TEST_RESULT_BOOL(ioReadOpen(storageFileReadIo(file)), true, "   open file");

        // Close the file handle so operations will fail
        close(((StorageDriverPosixFileRead *)file->driver)->handle);

        TEST_ERROR_FMT(
            ioRead(storageFileReadIo(file), outBuffer), FileReadError,
            "unable to read '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            ioReadClose(storageFileReadIo(file)), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        ((StorageDriverPosixFileRead *)file->driver)->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(0);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageFileReadMove(storageNewReadNP(storageTest, fileName), MEM_CONTEXT_OLD()), "new read file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_BOOL(ioReadOpen(storageFileReadIo(file)), true, "   open file");
        TEST_RESULT_STR(strPtr(storageFileReadName(file)), strPtr(fileName), "    check file name");
        TEST_RESULT_STR(strPtr(storageFileReadType(file)), "posix", "    check file type");

        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), false, "    check file contents (not all loaded yet)");

        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageFileReadIo(file), outBuffer), "    no data to load");
        TEST_RESULT_INT(bufUsed(outBuffer), 0, "    buffer is empty");

        TEST_RESULT_VOID(
            storageDriverPosixFileRead(storageFileReadDriver(file), outBuffer), "    no data to load from driver either");
        TEST_RESULT_INT(bufUsed(outBuffer), 0, "    buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageFileReadIo(file)), true, "    eof");
        TEST_RESULT_BOOL(ioReadEof(storageFileReadIo(file)), true, "    still eof");

        TEST_RESULT_VOID(ioReadClose(storageFileReadIo(file)), "    close file");

        TEST_RESULT_VOID(storageFileReadFree(storageNewReadNP(storageTest, fileName)), "   free file");
        TEST_RESULT_VOID(storageFileReadFree(NULL), "   free null file");
        TEST_RESULT_VOID(storageDriverPosixFileReadFree(NULL), "   free null posix file");

        TEST_RESULT_VOID(storageFileReadMove(NULL, memContextTop()), "   move null file");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageFileWrite"))
    {
        TEST_CREATE_NOPERM();
        StorageFileWrite *file = NULL;

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noCreatePath = true, .noSyncFile = true,
                .noSyncPath = true, .noAtomic = true),
            "new write file");

        TEST_RESULT_BOOL(storageFileWriteAtomic(file), false, "    check atomic");
        TEST_RESULT_BOOL(storageFileWriteCreatePath(file), false, "    check create path");
        TEST_RESULT_PTR(storageFileWriteFileDriver(file), file->driver, "    check file driver");
        TEST_RESULT_INT(storageFileWriteModeFile(file), 0444, "    check mode file");
        TEST_RESULT_INT(storageFileWriteModePath(file), 0555, "    check mode path");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileNoPerm), "    check name");
        TEST_RESULT_BOOL(storageFileWriteSyncPath(file), false, "    check sync path");
        TEST_RESULT_BOOL(storageFileWriteSyncFile(file), false, "    check sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageFileWriteIo(file)), FileOpenError,
            "unable to open '%s' for write: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/sub1/test.file", testPath());

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noCreatePath = true, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageFileWriteIo(file)), FileMissingError,
            "unable to open '%s' for write: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileTmp = strNewFmt("%s.pgbackrest.tmp", strPtr(fileName));
        ioBufferSizeSet(10);
        Buffer *buffer = bufNewZ("TESTFILE\n");

        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileName), "    check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");

        // Close the file handle so operations will fail
        close(((StorageDriverPosixFileWrite *)file->driver)->handle);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        TEST_ERROR_FMT(
            storageDriverPosixFileWrite(storageFileWriteFileDriver(file), buffer), FileWriteError,
            "unable to write '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageDriverPosixFileWriteClose(storageFileWriteFileDriver(file)), FileSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));

        // Disable file sync so the close can be reached
        ((StorageDriverPosixFileWrite *)file->driver)->syncFile = false;

        TEST_ERROR_FMT(
            storageDriverPosixFileWriteClose(storageFileWriteFileDriver(file)), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        ((StorageDriverPosixFileWrite *)file->driver)->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileName), "    check file name");
        TEST_RESULT_STR(strPtr(storageFileWriteType(file)), "posix", "    check file type");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");

        // Rename the file back to original name from tmp -- this will cause the rename in close to fail
        TEST_RESULT_INT(rename(strPtr(fileTmp), strPtr(fileName)), 0, " rename tmp file");
        TEST_ERROR_FMT(
            ioWriteClose(storageFileWriteIo(file)), FileMoveError,
            "unable to move '%s' to '%s': [2] No such file or directory", strPtr(fileTmp), strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        ((StorageDriverPosixFileWrite *)file->driver)->handle = -1;

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageFileWriteMove(storageNewWriteNP(storageTest, fileName), MEM_CONTEXT_OLD()), "new write file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWrite(storageFileWriteIo(file), NULL), "   write null buffer to file");
        TEST_RESULT_VOID(ioWrite(storageFileWriteIo(file), bufNew(0)), "   write zero buffer to file");
        TEST_RESULT_VOID(ioWrite(storageFileWriteIo(file), buffer), "   write to file");
        TEST_RESULT_VOID(ioWriteClose(storageFileWriteIo(file)), "   close file");
        TEST_RESULT_VOID(storageFileWriteFree(storageNewWriteNP(storageTest, fileName)), "   free file");
        TEST_RESULT_VOID(storageFileWriteFree(NULL), "   free null file");
        TEST_RESULT_VOID(storageDriverPosixFileWriteFree(NULL), "   free null posix file");
        TEST_RESULT_VOID(storageFileWriteMove(NULL, memContextTop()), "   move null file");

        Buffer *expectedBuffer = storageGetNP(storageNewReadNP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0750, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0640, "    check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/test.file", testPath());

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageFileWriteIo(file)), "    open file");
        TEST_RESULT_VOID(ioWrite(storageFileWriteIo(file), buffer), "   write to file");
        TEST_RESULT_VOID(ioWriteClose(storageFileWriteIo(file)), "   close file");

        expectedBuffer = storageGetNP(storageNewReadNP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0700, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0600, "    check file mode");

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

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), "/", "check base path");

        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocalWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocalWrite(), storage, "get cached storage");

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), "/", "check base path");

        TEST_RESULT_VOID(storageNewWriteNP(storage, writeFile), "writes are allowed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet() and storageRepo()"))
    {
        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(storageRepoGet(strNew(STORAGE_TYPE_CIFS), false), "get cifs repo storage");
        TEST_ERROR(storageRepoGet(strNew(BOGUS_STR), false), AssertError, "invalid storage type 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        const Storage *storage = NULL;
// CSHANG Need a test regarding OK to have a NULL stanza but first need to decide if can transition from NULL to an actual stanza (i.e. require an init flag)
        TEST_RESULT_PTR(storageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageRepo, storage, "repo storage cached");
        TEST_RESULT_PTR(storageRepo(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(storagePathNP(storage, strNew("<BOGUS>/path")), AssertError, "invalid expression '<BOGUS>'");
        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), testPath(), "check base path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE))), strPtr(strNewFmt("%s/archive/db", testPath())),
            "check archive path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/simple"))),
            strPtr(strNewFmt("%s/archive/db/simple", testPath())), "check simple path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/700000007000000070000000"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/7000000070000000/700000007000000070000000", testPath())), "check segment path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/00000008.history"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/00000008.history", testPath())), "check history path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/000000010000014C0000001A.00000028.backup"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/000000010000014C/000000010000014C0000001A.00000028.backup", testPath())),
            "check backup path");

        // Change the stanza name and make sure helper fails
        // -------------------------------------------------------------------------------------------------------------------------
        storageHelper.storageRepo = NULL;

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=other");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(storageRepo(), AssertError, "stanza has changed from 'db' to 'other'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSpool() and storageSpoolWrite()"))
    {
        const Storage *storage = NULL;

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storageSpool not cached");
        TEST_ERROR(storageSpool(), AssertError, "stanza cannot be NULL for this storage object");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storageSpoolWrite not cached");
        TEST_ERROR(storageSpoolWrite(), AssertError, "stanza cannot be NULL for this storage object");

        // Load configuration to set spool-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpool, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), testPath(), "check base path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_SPOOL_ARCHIVE_OUT))), strPtr(strNewFmt("%s/archive/db/out", testPath())),
            "check spool out path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, "file.ext"))),
            strPtr(strNewFmt("%s/archive/db/out/file.ext", testPath())), "check spool out file");

        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_SPOOL_ARCHIVE_IN))), strPtr(strNewFmt("%s/archive/db/in", testPath())),
            "check spool in path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_IN, "file.ext"))),
            strPtr(strNewFmt("%s/archive/db/in/file.ext", testPath())), "check spool in file");

        TEST_ERROR(storagePathNP(storage, strNew("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");

        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpoolWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, storage, "storage cached");
        TEST_RESULT_PTR(storageSpoolWrite(), storage, "get cached storage");

        TEST_RESULT_VOID(storageNewWriteNP(storage, writeFile), "writes are allowed");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
