/***********************************************************************************************************************************
Test Storage Manager
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/file.h"

/***********************************************************************************************************************************
Get the mode of a file on local storage
***********************************************************************************************************************************/
mode_t
storageStatMode(const String *path)
{
    // Attempt to stat the file
    struct stat statFile;

    if (stat(strPtr(path), &statFile) == -1)                                                // {uncovered - error should not happen}
        THROW_SYS_ERROR(FileOpenError, "unable to stat '%s'", strPtr(path));                // {uncovered+}

    return statFile.st_mode & 0777;
}

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
        THROW(AssertError, "invalid expression '%s'", strPtr(expression));

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // Create default storage object for testing
    Storage *storageTest = storageNewP(strNew(testPath()), .write = true);

    // Create a directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    TEST_RESULT_INT(
        system(
            strPtr(strNewFmt("sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strPtr(pathNoPerm), strPtr(fileNoPerm),
            strPtr(fileNoPerm)))),
        0, "create no perm file");

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
        TEST_ERROR(
            storageExistsNP(storageTest, fileNoPerm), FileOpenError,
            strPtr(strNewFmt("unable to stat '%s': [13] Permission denied", strPtr(fileNoPerm))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        if (fork() == 0)
        {
            sleepMSec(250);
            TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");
            exit(0);
        }

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 1), true, "file exists after wait");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo rm %s", strPtr(fileExists)))), 0, "remove exists file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            storageListP(storageTest, strNew(BOGUS_STR), .errorOnMissing = true), PathOpenError,
            strPtr(strNewFmt("unable to open directory '%s/BOGUS' for read: [2] No such file or directory", testPath())));

        TEST_RESULT_PTR(storageListNP(storageTest, strNew(BOGUS_STR)), NULL, "ignore missing dir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            storageListNP(storageTest, pathNoPerm), PathOpenError,
            strPtr(strNewFmt("unable to open directory '%s' for read: [13] Permission denied", strPtr(pathNoPerm))));

        // Should still error even when ignore missing
        TEST_ERROR(
            storageListNP(storageTest, pathNoPerm), PathOpenError,
            strPtr(strNewFmt("unable to open directory '%s' for read: [13] Permission denied", strPtr(pathNoPerm))));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageOpenWriteNP(storageTest, strNew("aaa.txt")), bufNewStr(strNew("aaa"))), "write aaa.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListNP(storageTest, NULL), ", ")), "aaa.txt, stderr.log, stdout.log, noperm",
            "dir list");

        TEST_RESULT_VOID(
            storagePutNP(storageOpenWriteNP(storageTest, strNew("bbb.txt")), bufNewStr(strNew("bbb"))), "write bbb.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListP(storageTest, NULL, .expression = strNew("^bbb")), ", ")), "bbb.txt", "dir list");
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
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, strNew("sub1"))), 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNew("sub1")), "create sub1 again");
        TEST_ERROR(
            storagePathCreateP(storageTest, strNew("sub1"), .errorOnExists = true), PathCreateError,
            strPtr(strNewFmt("unable to create path '%s/sub1': [17] File exists", testPath())));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, strNew("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, strNew("sub2"))), 0777, "check sub2 dir mode");

        TEST_ERROR(
            storagePathCreateP(storageTest, strNew("sub3/sub4"), .noParentCreate = true), PathCreateError,
            strPtr(strNewFmt("unable to create path '%s/sub3/sub4': [2] No such file or directory", testPath())));
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNew("sub3/sub4")), "create sub3/sub4");

        TEST_RESULT_INT(system(strPtr(strNewFmt("rm -rf %s/sub*", testPath()))), 0, "remove sub paths");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageOpenRead()"))
    {
        TEST_ERROR(
            storageOpenReadNP(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR)),
            FileOpenError,
            strPtr(strNewFmt("unable to open '%s/%s' for read: [2] No such file or directory", testPath(), BOGUS_STR)));

        TEST_RESULT_PTR(
            storageOpenReadP(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR), .ignoreMissing = true),
            NULL, "open missing file without error");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            storageGetNP(storageOpenReadP(storageTest, fileNoPerm, .ignoreMissing = true)), FileOpenError,
            strPtr(strNewFmt("unable to open '%s' for read: [13] Permission denied", strPtr(fileNoPerm))));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageOpenWrite()"))
    {
        TEST_ERROR(
            storageOpenWriteNP(storageTest, strNew(testPath())), FileOpenError,
            strPtr(strNewFmt("unable to open '%s' for write: [21] Is a directory", testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        StorageFile *file = NULL;
        String *fileName = strNewFmt("%s/testfile", testPath());

        TEST_ASSIGN(file, storageOpenWriteNP(storageTest, fileName), "open file for write (defaults)");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, fileName)), 0640, "check file mode");
        close(STORAGE_DATA(file)->handle);

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageOpenWriteP(storageTest, fileName, .mode = 0777), "open file for write (custom)");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, fileName)), 0777, "check file mode");
        close(STORAGE_DATA(file)->handle);

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storageNewP(strNew("/"), .write = true);

        TEST_ERROR(
            storageGetNP(storageOpenReadNP(storageTest, strNew(testPath()))), FileReadError,
            strPtr(strNewFmt("unable to read '%s': [21] Is a directory", testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageOpenWriteNP(storageTest, strNewFmt("%s/test.empty", testPath())), NULL), "put empty file");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));
        StorageFile *file = NULL;

        StorageFileDataPosix fileData = {.handle = 999999};
        MEM_CONTEXT_NEW_BEGIN("FileTest")
        {
            file = storageFileNew(storageTest, strNew("badfile"), storageFileTypeWrite, &fileData);
        }
        MEM_CONTEXT_NEW_END();

        TEST_ERROR(storagePutNP(file, buffer), FileWriteError, "unable to write 'badfile': [9] Bad file descriptor");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageOpenWriteNP(storageTest, strNewFmt("%s/test.txt", testPath())), buffer), "put text file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(
            storageGetNP(storageOpenReadP(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR), .ignoreMissing = true)),
            NULL, "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetNP(storageOpenReadNP(storageTest, strNewFmt("%s/test.empty", testPath()))), "get empty");
        TEST_RESULT_INT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGetNP(storageOpenReadNP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        const Storage *storage = storageTest;
        ((Storage *)storage)->bufferSize = 2;

        TEST_ASSIGN(buffer, storageGetNP(storageOpenReadNP(storageTest, strNewFmt("%s/test.txt", testPath()))), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("missing")), "remove missing file");
        TEST_ERROR(
            storageRemoveP(storageTest, strNew("missing"), .errorOnMissing = true), FileRemoveError,
            strPtr(strNewFmt("unable to remove '%s/missing': [2] No such file or directory", testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileExists = strNewFmt("%s/exists", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileExists)))), 0, "create exists file");

        TEST_RESULT_VOID(storageRemoveNP(storageTest, fileExists), "remove exists file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            storageRemoveNP(storageTest, fileNoPerm), FileRemoveError,
            strPtr(strNewFmt("unable to remove '%s': [13] Permission denied", strPtr(fileNoPerm))));
    }
}
