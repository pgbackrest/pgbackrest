/***********************************************************************************************************************************
Test Storage Manager
***********************************************************************************************************************************/

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
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        Storage *storage = NULL;

        TEST_ERROR(storageNew(NULL, 0750, 65536, NULL), AssertError, "storage base path cannot be null");

        TEST_ASSIGN(storage, storageNew(strNew("/"), 0750, 65536, NULL), "new storage");
        TEST_RESULT_INT(storage->mode, 0750, "check mode");
        TEST_RESULT_INT(storage->bufferSize, 65536, "check buffer size");
        TEST_RESULT_STR(strPtr(storage->path), "/", "check path");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storage = NULL;

        TEST_ASSIGN(storage, storageNew(strNew("/"), 0750, 65536, NULL), "new storage /");
        TEST_RESULT_STR(strPtr(storagePath(storage, NULL)), "/", "    root dir");
        TEST_RESULT_STR(strPtr(storagePath(storage, strNew("/"))), "/", "    same as root dir");
        TEST_RESULT_STR(strPtr(storagePath(storage, strNew("subdir"))), "/subdir", "    simple subdir");

        TEST_ERROR(storagePath(storage, strNew("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            storage, storageNew(strNew("/path/to"), 0750, 65536, (StoragePathExpressionCallback)storageTestPathExpression),
            "new storage /path/to with expression");
        TEST_RESULT_STR(strPtr(storagePath(storage, NULL)), "/path/to", "    root dir");
        TEST_RESULT_STR(strPtr(storagePath(storage, strNew("is/a/subdir"))), "/path/to/is/a/subdir", "    subdir");

        TEST_ERROR(storagePath(storage, strNew("/bogus")), AssertError, "absolute path '/bogus' is not in base path '/path/to'");
        TEST_ERROR(
            storagePath(storage, strNew("/path/toot")), AssertError, "absolute path '/path/toot' is not in base path '/path/to'");

        TEST_ERROR(storagePath(storage, strNew("<TEST")), AssertError, "end > not found in path expression '<TEST'");
        TEST_ERROR(
            storagePath(storage, strNew("<TEST>" BOGUS_STR)), AssertError, "'/' should separate expression and path '<TEST>BOGUS'");

        TEST_RESULT_STR(strPtr(storagePath(storage, strNew("<TEST>"))), "/path/to/test", "    expression");
        TEST_RESULT_STR(
            strPtr(storagePath(storage, strNew("<TEST>/something"))), "/path/to/test/something", "    expression with path");

        TEST_ERROR(storagePath(storage, strNew("<NULL>")), AssertError, "evaluated path '<NULL>' cannot be null");

        TEST_ERROR(storagePath(storage, strNew("<WHATEVS>")), AssertError, "invalid expression '<WHATEVS>'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        Storage *storage = NULL;

        TEST_ASSIGN(storage, storageNew(strNew(testPath()), 0750, 65536, NULL), "new storage");

        TEST_ERROR(
            storageList(storage, strNew(BOGUS_STR), NULL, false), PathOpenError,
            strPtr(strNewFmt("unable to open directory '%s/BOGUS' for read: [2] No such file or directory", testPath())));

        TEST_RESULT_PTR(storageList(storage, strNew(BOGUS_STR), NULL, true), NULL, "ignore missing dir");

        TEST_RESULT_VOID(storagePut(storage, strNew("aaa.txt"), bufNewStr(strNew("aaa"))), "write aaa.text");
        TEST_RESULT_STR(
            strPtr(strLstJoin(storageList(storage, NULL, NULL, false), ", ")), "aaa.txt, stderr.log, stdout.log", "dir list");

        TEST_RESULT_VOID(storagePut(storage, strNew("bbb.txt"), bufNewStr(strNew("bbb"))), "write bbb.text");
        TEST_RESULT_STR(strPtr(strLstJoin(storageList(storage, NULL, strNew("^bbb"), false), ", ")), "bbb.txt", "dir list");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storageNew(strNew("/"), 0750, 65536, NULL);

        TEST_ERROR(
            storagePut(storageTest, strNew(testPath()), NULL), FileOpenError,
            strPtr(strNewFmt("unable to open '%s' for write: Is a directory", testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageWriteError(3, 3, strNew("file")), "no write error");

        errno = EBUSY;
        TEST_ERROR(storageWriteError(-1, 3, strNew("file")), FileWriteError, "unable to write 'file': Device or resource busy");
        TEST_ERROR(
            storageWriteError(1, 3, strNew("file")), FileWriteError, "only wrote 1 byte(s) to 'file' but 3 byte(s) expected");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePut(storageTest, strNewFmt("%s/test.empty", testPath()), NULL), "put empty file");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));

        TEST_RESULT_VOID(storagePut(storageTest, strNewFmt("%s/test.txt", testPath()), buffer), "put text file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(storageGet(storageTest, strNewFmt("%s/%s", testPath(), BOGUS_STR), false), FileOpenError,
        strPtr(strNewFmt("unable to open '%s/%s' for read: No such file or directory", testPath(), BOGUS_STR)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageReadError(3, strNew("file")), "no read error");

        errno = ENOTBLK;
        TEST_ERROR(storageReadError(-1, strNew("file")), FileReadError, "unable to read 'file': Block device required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGet(storageTest, strNewFmt("%s/test.empty", testPath()), false), "get empty");
        TEST_RESULT_INT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGet(storageTest, strNewFmt("%s/test.txt", testPath()), false), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        const Storage *storage = storageTest;
        ((Storage *)storage)->bufferSize = 2;

        TEST_ASSIGN(buffer, storageGet(storageTest, strNewFmt("%s/test.txt", testPath()), false), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");
    }
}
