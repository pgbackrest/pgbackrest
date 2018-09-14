/***********************************************************************************************************************************
Test Storage File
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "storage/storage.h"

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
    ioBufferSizeSet(2);

    // Create a directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    TEST_RESULT_INT(
        system(
            strPtr(strNewFmt("sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strPtr(pathNoPerm), strPtr(fileNoPerm),
            strPtr(fileNoPerm)))),
        0, "create no perm path/file");

    // *****************************************************************************************************************************
    if (testBegin("storageDriverPosixFile*()"))
    {
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
    if (testBegin("StorageFileRead"))
    {
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
        Buffer *expectedBuffer = bufNewStr(strNew("TESTFILE\n"));
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
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));

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

    FUNCTION_HARNESS_RESULT_VOID();
}
