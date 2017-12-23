/***********************************************************************************************************************************
Test Storage Manager
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("storageLocal()"))
    {
        const Storage *storage = NULL;

        TEST_ASSIGN(storage, storageLocal(), "get local storage");
        TEST_RESULT_INT(storage->mode, 0750, "check mode");
        TEST_RESULT_INT(storage->bufferSize, 65536, "check buffer size");
        TEST_RESULT_STR(strPtr(storage->path), "/", "check path");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("storagePut() and storageGet()"))
    {
        TEST_ERROR(
            storagePut(storageLocal(), strNew(testPath()), NULL), FileOpenError,
            strPtr(strNewFmt("unable to open '%s' for write: Is a directory", testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageWriteError(3, 3, strNew("file")), "no write error");

        errno = EBUSY;
        TEST_ERROR(storageWriteError(-1, 3, strNew("file")), FileWriteError, "unable to write 'file': Device or resource busy");
        TEST_ERROR(
            storageWriteError(1, 3, strNew("file")), FileWriteError, "only wrote 1 byte(s) to 'file' but 3 byte(s) expected");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePut(storageLocal(), strNewFmt("%s/test.empty", testPath()), NULL), "put empty file");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));

        TEST_RESULT_VOID(storagePut(storageLocal(), strNewFmt("%s/test.txt", testPath()), buffer), "put text file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(storageGet(storageLocal(), strNewFmt("%s/%s", testPath(), BOGUS_STR), false), FileOpenError,
        strPtr(strNewFmt("unable to open '%s/%s' for read: No such file or directory", testPath(), BOGUS_STR)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageReadError(3, strNew("file")), "no read error");

        errno = ENOTBLK;
        TEST_ERROR(storageReadError(-1, strNew("file")), FileReadError, "unable to read 'file': Block device required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGet(storageLocal(), strNewFmt("%s/test.empty", testPath()), false), "get empty");
        TEST_RESULT_INT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(buffer, storageGet(storageLocal(), strNewFmt("%s/test.txt", testPath()), false), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        const Storage *storage = storageLocal();
        ((Storage *)storage)->bufferSize = 2;

        TEST_ASSIGN(buffer, storageGet(storageLocal(), strNewFmt("%s/test.txt", testPath()), false), "get text");
        TEST_RESULT_INT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");
    }
}
