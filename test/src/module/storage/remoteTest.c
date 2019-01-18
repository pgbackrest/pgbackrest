/***********************************************************************************************************************************
Test Remote Storage Driver
***********************************************************************************************************************************/
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
    strLstAddZ(argList, "--db-timeout=9");
    strLstAddZ(argList, "--protocol-timeout=10");
    strLstAddZ(argList, "--buffer-size=16384");
    strLstAddZ(argList, "--repo1-host=localhost");
    strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argList, "archive-push");
    harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

    // *****************************************************************************************************************************
    if (testBegin("storageList(), storageNewRead()"))
    {
        Storage *storageRemote = NULL;

        // Create repo path since the remote will not start without it
        storagePathCreateNP(storageTest, strNew("repo"));

        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        TEST_RESULT_STR(strPtr(strLstJoin(storageListNP(storageRemote, NULL), ",")), "" , "list empty path");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageTest, strNew("repo/testy"));
        TEST_RESULT_STR(strPtr(strLstJoin(storageListNP(storageRemote, NULL), ",")), "testy" , "list path");

        storagePathCreateNP(storageTest, strNew("repo/testy2"));
        TEST_RESULT_STR(strPtr(strLstJoin(storageListNP(storageRemote, NULL), ",")), "testy,testy2" , "list 2 paths");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *contentBuf = bufNew(32768);

        for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
            bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

        bufUsedSet(contentBuf, bufSize(contentBuf));

        TEST_ERROR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageRemote, strNew("test.txt"))))), FileMissingError,
            strPtr(
                strNewFmt(
                    "raised from remote-1 protocol on 'localhost': unable to open '%s/repo/test.txt': No such file or directory",
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

        // -------------------------------------------------------------------------------------------------------------------------
        storageRemote->write = true;
        TEST_ERROR(storageExistsNP(storageRemote, strNew("file.txt")), AssertError, "NOT YET IMPLEMENTED");
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
