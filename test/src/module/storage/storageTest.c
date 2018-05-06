/***********************************************************************************************************************************
Test Storage Manager
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/fileRead.h"
#include "storage/fileWrite.h"

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
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // Create default storage object for testing
    Storage *storageTest = storageNewP(strNew(testPath()), .write = true, .bufferSize = 3);
    Storage *storageTmp = storageNewP(strNew("/tmp"), .write = true);

    // Create a directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    TEST_RESULT_INT(
        system(
            strPtr(strNewFmt("sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strPtr(pathNoPerm), strPtr(fileNoPerm),
            strPtr(fileNoPerm)))),
        0, "create no perm path/file");

    // *****************************************************************************************************************************
    if (testBegin("storageNew() and storageFree()"))
    {
        Storage *storageTest = NULL;

        TEST_ERROR(storageNewNP(NULL), AssertError, "storage base path cannot be null");

        TEST_ASSIGN(storageTest, storageNewNP(strNew("/")), "new storage (defaults)");
        TEST_RESULT_STR(strPtr(storageTest->path), "/", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "     check path mode");
        TEST_RESULT_INT(storageTest->bufferSize, 65536, "    check buffer size");
        TEST_RESULT_BOOL(storageTest->write, false, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "    check expression function is not set");

        TEST_ASSIGN(
            storageTest,
            storageNewP(
                strNew("/path/to"), .modeFile = 0600, .modePath = 0700, .bufferSize = 8192, .write = true,
                .pathExpressionFunction = storageTestPathExpression),
            "new storage (non-default)");
        TEST_RESULT_STR(strPtr(storageTest->path), "/path/to", "    check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "    check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "     check path mode");
        TEST_RESULT_INT(storageTest->bufferSize, 8192, "    check buffer size");
        TEST_RESULT_BOOL(storageTest->write, true, "    check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction != NULL, true, "    check expression function is set");

        TEST_RESULT_VOID(storageFree(storageTest), "free storage");
        TEST_RESULT_VOID(storageFree(NULL), "free null storage");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists()"))
    {
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
        Buffer *buffer = bufNewStr(strNew("TESTFILE"));
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
            storagePutNP(storageNewWriteNP(storageTest, strNew("aaa.txt")), bufNewStr(strNew("aaa"))), "write aaa.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListNP(storageTest, NULL), ", ")), "aaa.txt, stderr.log, stdout.log, noperm",
            "dir list");

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNew("bbb.txt")), bufNewStr(strNew("bbb"))), "write bbb.text");
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
            storageCopyNP(source, destination), FileOpenError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadP(storageTest, sourceFile, .ignoreMissing = true);

        TEST_RESULT_BOOL(storageCopyNP(source, destination), false, "copy and ignore missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *expectedBuffer = bufNewStr(strNew("TESTFILE\n"));
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
        String *sourceFile = strNewFmt("%s/source.txt", testPath());
        String *destinationFile = strNewFmt("%s/sub/destination.txt", testPath());

        StorageFileRead *source = storageNewReadNP(storageTest, sourceFile);
        StorageFileWrite *destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageMoveNP(source, destination), FileMissingError,
            "unable to move missing file '%s': [2] No such file or directory", strPtr(sourceFile));

        // -------------------------------------------------------------------------------------------------------------------------
        source = storageNewReadNP(storageTest, fileNoPerm);

        TEST_ERROR_FMT(
            storageMoveNP(source, destination), FileMoveError,
            "unable to move '%s' to '%s': [13] Permission denied", strPtr(fileNoPerm), strPtr(destinationFile));

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewStr(strNew("TESTFILE"));
        storagePutNP(storageNewWriteNP(storageTest, sourceFile), buffer);

        source = storageNewReadNP(storageTest, sourceFile);
        destination = storageNewWriteP(storageTest, destinationFile, .noCreatePath = true);

        TEST_ERROR_FMT(
            storageMoveNP(source, destination), PathMissingError,
            "unable to move '%s' to missing path '%s': [2] No such file or directory", strPtr(sourceFile),
            strPtr(strPath(destinationFile)));

        // -------------------------------------------------------------------------------------------------------------------------
        destination = storageNewWriteNP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveNP(source, destination), "move file to subpath");
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

        TEST_RESULT_VOID(storageMoveNP(source, destination), "move file to same path");

        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTest, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveNP(source, destination), "move file to parent path (no sync)");

        // Move across filesystems
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTest, sourceFile);
        destinationFile = strNewFmt("/tmp/destination.txt");
        destination = storageNewWriteNP(storageTmp, destinationFile);

        TEST_RESULT_VOID(storageMoveNP(source, destination), "move file to another filesystem");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTmp, destinationFile), true, "check destination file exists");

        // Move across fileystems without syncing the paths
        // -------------------------------------------------------------------------------------------------------------------------
        sourceFile = destinationFile;
        source = storageNewReadNP(storageTmp, sourceFile);
        destinationFile = strNewFmt("%s/source.txt", testPath());
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveNP(source, destination), "move file to another filesystem without path sync");
        TEST_RESULT_BOOL(storageExistsNP(storageTmp, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, destinationFile), true, "check destination file exists");

        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storageTest = NULL;

        TEST_ASSIGN(storageTest, storageNewNP(strNew("/")), "new storage /");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, NULL)), "/", "    root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("/"))), "/", "    same as root dir");
        TEST_RESULT_STR(strPtr(storagePathNP(storageTest, strNew("subdir"))), "/subdir", "    simple subdir");

        TEST_ERROR(
            storagePathNP(storageTest, strNew("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            storageTest,
            storageNewP(strNew("/path/to"), .pathExpressionFunction = storageTestPathExpression),
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
        TEST_ERROR_FMT(
            storagePathSyncNP(storageTest, fileNoPerm), PathOpenError,
            "unable to open '%s' for sync: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathName = strNewFmt("%s/testpath", testPath());

        TEST_ERROR_FMT(
            storagePathSyncNP(storageTest, pathName), PathOpenError,
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
            storageFileReadOpen(file), FileOpenError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileName)))), 0, "create read file");

        TEST_RESULT_BOOL(storageFileReadOpen(file), true, "    open file");
        TEST_RESULT_VOID(storageFileReadClose(file), "    close file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        StorageFileWrite *file = NULL;

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file (defaults)");
        TEST_ERROR_FMT(
            storageFileWriteOpen(file), FileOpenError,
            "unable to open '%s' for write: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/sub1/testfile", testPath());

        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");
        TEST_RESULT_VOID(storageFileWriteClose(file), "   close file");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0750, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0640, "    check file mode");

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/testfile", testPath());

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .modePath = 0700, .modeFile = 0600), "new write file (set mode)");
        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");
        TEST_RESULT_VOID(storageFileWriteClose(file), "   close file");
        TEST_RESULT_INT(storageInfoNP(storageTest, strPath(fileName)).mode, 0700, "    check path mode");
        TEST_RESULT_INT(storageInfoNP(storageTest, fileName).mode, 0600, "    check file mode");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storageNewP(strNew("/"), .write = true);

        TEST_ERROR_FMT(
            storageGetNP(storageNewReadNP(storageTest, strNew(testPath()))), FileReadError,
            "unable to read '%s': [21] Is a directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        String *emptyFile = strNewFmt("%s/test.empty", testPath());
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, emptyFile), true, "check empty file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));

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
        const Storage *storage = storageTest;
        ((Storage *)storage)->bufferSize = 2;

        TEST_ASSIGN(buffer, storageGetNP(storageNewReadNP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
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
}
