/***********************************************************************************************************************************
Test Remote Storage Driver
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Test storage
    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

    // Load configuration to set repo-path and stanza
    StringList *argList = strLstNew();
    strLstAddZ(argList, "/usr/bin/pgbackrest");
    strLstAddZ(argList, "--stanza=db");
    strLstAddZ(argList, "--protocol-timeout=10");
    strLstAddZ(argList, "--buffer-size=16384");
    strLstAddZ(argList, "--repo1-host=localhost");
    strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argList, "info");
    harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

    // Start a protocol server to test the remote protocol
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteIo(ioBufferWriteNew(serverWrite));
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(
        strNew("test"), strNew("test"), ioBufferReadIo(ioBufferReadNew(bufNew(0))), serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("storageExists()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        storagePathCreateNP(storageTest, strNew("repo"));

        TEST_RESULT_BOOL(storageExistsNP(storageRemote, strNew("test.txt")), false, "file does not exist");

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), bufNewStr(strNew("TEST")));
        TEST_RESULT_BOOL(storageExistsNP(storageRemote, strNew("test.txt")), true, "file exists");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));

        TEST_RESULT_BOOL(
            storageDriverRemoteProtocol(PROTOCOL_COMMAND_STORAGE_EXISTS_STR, paramList, server), true, "protocol exists");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        storagePathCreateNP(storageTest, strNew("repo"));

        TEST_RESULT_STR(strPtr(strLstJoin(storageListNP(storageRemote, NULL), ",")), "" , "list empty path");
        TEST_RESULT_PTR(storageListNP(storageRemote, strNew(BOGUS_STR)), NULL , "missing directory ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageTest, strNew("repo/testy"));
        TEST_RESULT_STR(strPtr(strLstJoin(storageListNP(storageRemote, NULL), ",")), "testy" , "list path");

        storagePathCreateNP(storageTest, strNew("repo/testy2\""));
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(storageRemote, strNewFmt("%s/repo", testPath())), sortOrderAsc), ",")),
            "testy,testy2\"" , "list 2 paths");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(strNew("^testy$")));

        TEST_RESULT_BOOL(storageDriverRemoteProtocol(PROTOCOL_COMMAND_STORAGE_LIST_STR, paramList, server), true, "protocol list");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":[\"testy\"]}\n", "check result");

        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(storageDriverRemoteProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        storagePathCreateNP(storageTest, strNew("repo"));

        Buffer *contentBuf = bufNew(32768);

        for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
            bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

        bufUsedSet(contentBuf, bufSize(contentBuf));

        TEST_ERROR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageRemote, strNew("test.txt"))))), FileMissingError,
            strPtr(
                strNewFmt(
                    "raised from remote-0 protocol on 'localhost': unable to open '%s/repo/test.txt' for read:"
                        " [2] No such file or directory",
                    testPath())));

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), contentBuf);

        StorageFileRead *fileRead = NULL;

        ioBufferSizeSet(8193);
        TEST_ASSIGN(fileRead, storageNewReadNP(storageRemote, strNew("test.txt")), "new file");
        TEST_RESULT_BOOL(bufEq(storageGetNP(fileRead), contentBuf), true, "get file");
        TEST_RESULT_BOOL(storageFileReadIgnoreMissing(fileRead), false, "check ignore missing");
        TEST_RESULT_STR(strPtr(storageFileReadName(fileRead)), "test.txt", "check name");
        TEST_RESULT_SIZE(
            storageDriverRemoteFileRead((StorageDriverRemoteFileRead *)storageFileReadDriver(fileRead), bufNew(32), false), 0,
            "nothing more to read");

        TEST_RESULT_BOOL(
            bufEq(storageGetNP(storageNewReadNP(storageRemote, strNew("test.txt"))), contentBuf), true, "get file again");

        TEST_ERROR(
            storageDriverRemoteFileReadBlockSize(strNew("bogus")), ProtocolError, "'bogus' is not a valid block size message");

        // Check protocol function directly (file missing)
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("missing.txt")));
        varLstAdd(paramList, varNewBool(true));

        TEST_RESULT_BOOL(
            storageDriverRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), true,
            "protocol open read (missing)");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "check result");

        bufUsedSet(serverWrite, 0);

        // Check protocol function directly (file exists)
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), bufNewStr(strNew("TESTDATA")));
        ioBufferSizeSet(4);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_BOOL(
            storageDriverRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), true, "protocol open read");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{\"out\":true}\n"
                "BRBLOCK4\n"
                "TESTBRBLOCK4\n"
                "DATABRBLOCK0\n",
            "check result");

        bufUsedSet(serverWrite, 0);
        ioBufferSizeSet(8192);
    }

    // *****************************************************************************************************************************
    if (testBegin("UNIMPLEMENTED"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");

        TEST_ERROR(storageInfoNP(storageRemote, strNew("file.txt")), AssertError, "NOT YET IMPLEMENTED");
        TEST_ERROR(storageNewWriteNP(storageRemote, strNew("file.txt")), AssertError, "NOT YET IMPLEMENTED");
        TEST_ERROR(storagePathCreateNP(storageRemote, strNew("path")), AssertError, "NOT YET IMPLEMENTED");
        TEST_ERROR(storagePathRemoveNP(storageRemote, strNew("path")), AssertError, "NOT YET IMPLEMENTED");
        TEST_ERROR(storagePathSyncNP(storageRemote, strNew("path")), AssertError, "NOT YET IMPLEMENTED");
        TEST_ERROR(storageRemoveNP(storageRemote, strNew("file.txt")), AssertError, "NOT YET IMPLEMENTED");
    }

    protocolFree();

    FUNCTION_HARNESS_RESULT_VOID();
}
