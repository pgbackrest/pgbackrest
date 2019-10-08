/***********************************************************************************************************************************
Test Remote Storage
***********************************************************************************************************************************/
#include "command/backup/pageChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Test storage
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // Load configuration to set repo-path and stanza
    StringList *argList = strLstNew();
    strLstAddZ(argList, "--stanza=db");
    strLstAddZ(argList, "--protocol-timeout=10");
    strLstAddZ(argList, "--buffer-size=16384");
    strLstAddZ(argList, "--repo1-host=localhost");
    strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
    harnessCfgLoad(cfgCmdInfo, argList);

    // Set type since we'll be running local and remote tests here
    cfgOptionSet(cfgOptType, cfgSourceParam, VARSTRDEF("backup"));
    cfgOptionValidSet(cfgOptType, true);

    // Set pg settings so we can run both db and backup remotes
    cfgOptionSet(cfgOptPgHost, cfgSourceParam, VARSTRDEF("localhost"));
    cfgOptionValidSet(cfgOptPgHost, true);
    cfgOptionSet(cfgOptPgPath, cfgSourceParam, VARSTR(strNewFmt("%s/pg", testPath())));
    cfgOptionValidSet(cfgOptPgPath, true);

    // Start a protocol server to test the remote protocol
    Buffer *serverRead = bufNew(8192);
    Buffer *serverWrite = bufNew(8192);
    IoRead *serverReadIo = ioBufferReadNew(serverRead);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioReadOpen(serverReadIo);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(strNew("test"), strNew("test"), serverReadIo, serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        TEST_RESULT_UINT(storageInterface(storageRemote).feature, storageInterface(storageTest).feature, "    check features");
        TEST_RESULT_BOOL(storageFeature(storageRemote, storageFeaturePath), true, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storageRemote, storageFeatureCompress), true, "    check compress feature");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_FEATURE_STR, varLstNew(), server), true, "protocol feature");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)), strPtr(strNewFmt("{\"out\":%" PRIu64 "}\n", storageInterface(storageTest).feature)),
            "check result");

        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(storageRemoteProtocol(strNew(BOGUS_STR), varLstNew(), server), false, "invalid function");
    }

    // Do these tests against a db remote for coverage
    // *****************************************************************************************************************************
    if (testBegin("storageExists()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storagePgGet(1, false), "get remote pg storage");
        storagePathCreateNP(storageTest, strNew("pg"));

        TEST_RESULT_BOOL(storageExistsNP(storageRemote, strNew("test.txt")), false, "file does not exist");

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), BUFSTRDEF("TEST"));
        TEST_RESULT_BOOL(storageExistsNP(storageRemote, strNew("test.txt")), true, "file exists");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptType, cfgSourceParam, VARSTRDEF("db"));
        cfgOptionValidSet(cfgOptType, true);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_EXISTS_STR, paramList, server), true, "protocol exists");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "check result");

        bufUsedSet(serverWrite, 0);

        cfgOptionSet(cfgOptType, cfgSourceParam, VARSTRDEF("db"));
        cfgOptionValidSet(cfgOptType, true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        storagePathCreateNP(storageTest, strNew("repo"));

        TEST_RESULT_PTR(storageListP(storageRemote, strNew(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListNP(storageRemote, NULL)), 0, "empty list for missing dir");

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
        varLstAdd(paramList, varNewStr(strNew("^testy$")));

        TEST_RESULT_BOOL(storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_LIST_STR, paramList, server), true, "protocol list");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":[\"testy\"]}\n", "check result");

        bufUsedSet(serverWrite, 0);
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

        TEST_ERROR_FMT(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageRemote, strNew("test.txt"))))), FileMissingError,
            "raised from remote-0 protocol on 'localhost': " STORAGE_ERROR_READ_MISSING,
            strPtr(strNewFmt("%s/repo/test.txt", testPath())));

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), contentBuf);

        // Disable protocol compression in the storage object to test no compression
        ((StorageRemote *)storageRemote->driver)->compressLevel = 0;

        StorageRead *fileRead = NULL;

        ioBufferSizeSet(8193);
        TEST_ASSIGN(fileRead, storageNewReadNP(storageRemote, strNew("test.txt")), "new file");
        TEST_RESULT_BOOL(bufEq(storageGetNP(fileRead), contentBuf), true, "get file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(fileRead), false, "check ignore missing");
        TEST_RESULT_STR(strPtr(storageReadName(fileRead)), "test.txt", "check name");
        TEST_RESULT_SIZE(
            storageReadRemote(storageRead(fileRead), bufNew(32), false), 0,
            "nothing more to read");

        TEST_ASSIGN(fileRead, storageNewReadNP(storageRemote, strNew("test.txt")), "get file");
        TEST_RESULT_BOOL(bufEq(storageGetNP(fileRead), contentBuf), true, "    check contents");
        TEST_RESULT_UINT(((StorageReadRemote *)fileRead->driver)->protocolReadBytes, bufSize(contentBuf), "    check read size");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageRemote->driver)->compressLevel = 3;

        TEST_ASSIGN(
            fileRead, storageNewReadP(storageRemote, strNew("test.txt"), .compressible = true), "get file (protocol compress)");
        TEST_RESULT_BOOL(bufEq(storageGetNP(fileRead), contentBuf), true, "    check contents");
        // We don't know how much protocol compression there will be exactly, but make sure this is some
        TEST_RESULT_BOOL(
            ((StorageReadRemote *)fileRead->driver)->protocolReadBytes < bufSize(contentBuf), true,
            "    check compressed read size");

        TEST_ERROR(
            storageRemoteProtocolBlockSize(strNew("bogus")), ProtocolError, "'bogus' is not a valid block size message");

        // Check protocol function directly (file missing)
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("missing.txt")));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewVarLst(varLstNew()));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), true,
            "protocol open read (missing)");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "check result");

        bufUsedSet(serverWrite, 0);

        // Check protocol function directly (file exists)
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/test.txt")), BUFSTRDEF("TESTDATA"));
        ioBufferSizeSet(4);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));
        varLstAdd(paramList, varNewBool(false));

        // Create filters to test filter logic
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, PG_PAGE_SIZE_DEFAULT, 0));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, gzipCompressNew(3, false));
        ioFilterGroupAdd(filterGroup, gzipDecompressNew(false));
        varLstAdd(paramList, ioFilterGroupParamAll(filterGroup));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), true, "protocol open read");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{\"out\":true}\n"
                "BRBLOCK4\n"
                "TESTBRBLOCK4\n"
                "DATABRBLOCK0\n"
                "{\"out\":{\"buffer\":null,\"cipherBlock\":null,\"gzipCompress\":null,\"gzipDecompress\":null"
                    ",\"hash\":\"bbbcf2c59433f68f22376cd2439d6cd309378df6\",\"pageChecksum\":{\"align\":false,\"valid\":false}"
                    ",\"size\":8}}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
        ioBufferSizeSet(8192);

        // Check protocol function directly (file exists but all data goes to sink)
        // -------------------------------------------------------------------------------------------------------------------------
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));
        varLstAdd(paramList, varNewBool(false));

        // Create filters to test filter logic
        filterGroup = ioFilterGroupNew();
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, ioSinkNew());
        varLstAdd(paramList, ioFilterGroupParamAll(filterGroup));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), true, "protocol open read (sink)");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{\"out\":true}\n"
                "BRBLOCK0\n"
                "{\"out\":{\"buffer\":null,\"hash\":\"bbbcf2c59433f68f22376cd2439d6cd309378df6\",\"sink\":null,\"size\":8}}\n",
            "check result");

        bufUsedSet(serverWrite, 0);

        // Check for error on a bogus filter
        // -------------------------------------------------------------------------------------------------------------------------
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test.txt")));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewVarLst(varLstAdd(varLstNew(), varNewKv(kvAdd(kvNew(), varNewStrZ("bogus"), NULL)))));

        TEST_ERROR(
            storageRemoteProtocol(
                PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR, paramList, server), AssertError, "unable to add filter 'bogus'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown pgbackrest %s/repo", testPath()))), 0, "update repo owner");

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");

        // Create buffer with plenty of data
        Buffer *contentBuf = bufNew(32768);

        for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
            bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

        bufUsedSet(contentBuf, bufSize(contentBuf));

        // Write the file
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(9999);

        // Disable protocol compression in the storage object to test no compression
        ((StorageRemote *)storageRemote->driver)->compressLevel = 0;

        StorageWrite *write = NULL;
        TEST_ASSIGN(write, storageNewWriteNP(storageRemote, strNew("test.txt")), "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
        TEST_RESULT_UINT(storageWriteModeFile(write), STORAGE_MODE_FILE_DEFAULT, "file mode is default");
        TEST_RESULT_UINT(storageWriteModePath(write), STORAGE_MODE_PATH_DEFAULT, "path mode is default");
        TEST_RESULT_STR(strPtr(storageWriteName(write)), "test.txt", "check file name");
        TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
        TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");

        TEST_RESULT_VOID(storagePutNP(write, contentBuf), "write file");
        TEST_RESULT_UINT(((StorageWriteRemote *)write->driver)->protocolWriteBytes, bufSize(contentBuf), "    check write size");
        TEST_RESULT_VOID(storageWriteRemoteClose((StorageWriteRemote *)storageWriteDriver(write)), "close file again");
        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        // Make sure the file was written correctly
        TEST_RESULT_BOOL(
            bufEq(storageGetNP(storageNewReadNP(storageRemote, strNew("test.txt"))), contentBuf), true, "check file");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageRemote->driver)->compressLevel = 3;

        // Write the file again, but this time free it before close and make sure the .tmp file is left
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(write, storageNewWriteNP(storageRemote, strNew("test2.txt")), "new write file");

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(write)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(write), contentBuf), "write bytes");

        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        TEST_RESULT_UINT(
            storageInfoNP(storageTest, strNew("repo/test2.txt.pgbackrest.tmp")).size, 16384, "file exists and is partial");

        // Write the file again with protocol compression
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(write, storageNewWriteP(storageRemote, strNew("test2.txt"), .compressible = true), "new write file (compress)");
        TEST_RESULT_VOID(storagePutNP(write, contentBuf), "write file");
        TEST_RESULT_BOOL(
            ((StorageWriteRemote *)write->driver)->protocolWriteBytes < bufSize(contentBuf), true,
            "    check compressed write size");

        // Check protocol function directly (complete write)
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(10);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test3.txt")));
        varLstAdd(paramList, varNewUInt64(0640));
        varLstAdd(paramList, varNewUInt64(0750));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewInt(0));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, ioFilterGroupParamAll(ioFilterGroupAdd(ioFilterGroupNew(), ioSizeNew())));

        // Generate input (includes the input for the test below -- need a way to reset this for better testing)
        bufCat(
            serverRead,
            BUFSTRDEF(
                "BRBLOCK3\n"
                "ABCBRBLOCK15\n"
                "123456789012345BRBLOCK0\n"
                "BRBLOCK3\n"
                "ABCBRBLOCK-1\n"));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR, paramList, server), true, "protocol open write");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{}\n"
                "{\"out\":{\"buffer\":null,\"size\":18}}\n",
            "check result");

        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, strNew("repo/test3.txt"))))), "ABC123456789012345",
            "check file");

        bufUsedSet(serverWrite, 0);

        // Check protocol function directly (free before write is closed)
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(10);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test4.txt")));
        varLstAdd(paramList, varNewUInt64(0640));
        varLstAdd(paramList, varNewUInt64(0750));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewInt(0));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, varNewVarLst(varLstNew()));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR, paramList, server), true, "protocol open write");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{}\n"
                "{}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
        ioBufferSizeSet(8192);

        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, strNew("repo/test4.txt.pgbackrest.tmp"))))), "",
            "check file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathExists()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), false), "get remote repo storage");
        storagePathCreateNP(storageTest, strNew("repo"));

        TEST_RESULT_BOOL(storagePathExistsNP(storageRemote, strNew("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsNP(storageRemote, NULL), true, "path exists");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("test")));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_EXISTS_STR, paramList, server), true, "protocol path exists");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        String *path = strNew("testpath");
        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown pgbackrest %s/repo", testPath()))), 0, "update repo owner");

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");

        // Create a path via the remote. Check the repo via the local test storage to ensure the remote created it.
        TEST_RESULT_VOID(storagePathCreateNP(storageRemote, path), "new path");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("repo/%s", strPtr(path))), "  get path info");
        TEST_RESULT_BOOL(info.exists, true, "  path exists");
        TEST_RESULT_INT(info.mode, STORAGE_MODE_PATH_DEFAULT, "  mode is default");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(path));
        varLstAdd(paramList, varNewBool(true));     // errorOnExists
        varLstAdd(paramList, varNewBool(true));     // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0));      // path mode

        TEST_ERROR_FMT(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR, paramList, server), PathCreateError,
            "raised from remote-0 protocol on 'localhost': unable to create path '%s/repo/testpath': [17] File exists",
            testPath());

        // Error if parent path not exist
        path = strNew("parent/testpath");
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(path));
        varLstAdd(paramList, varNewBool(false));    // errorOnExists
        varLstAdd(paramList, varNewBool(true));     // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0));      // path mode

        TEST_ERROR_FMT(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR, paramList, server), PathCreateError,
            "raised from remote-0 protocol on 'localhost': unable to create path '%s/repo/parent/testpath': "
            "[2] No such file or directory", testPath());

        // Create parent and path with default mode
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(path));
        varLstAdd(paramList, varNewBool(true));     // errorOnExists
        varLstAdd(paramList, varNewBool(false));    // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0777));   // path mode

        TEST_RESULT_VOID(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR, paramList, server), "create parent and path");
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("repo/%s", strPtr(path))), "  get path info");
        TEST_RESULT_BOOL(info.exists, true, "  path exists");
        TEST_RESULT_INT(info.mode, 0777, "  mode is set");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        String *path = strNew("testpath");
        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown pgbackrest %s/repo", testPath()))), 0, "update repo owner");

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");
        TEST_RESULT_VOID(storagePathCreateNP(storageRemote, path), "new path");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storagePathExistsNP(storageTest, strNewFmt("repo/%s", strPtr(path))), true, "path exists");
        TEST_RESULT_VOID(storagePathRemoveNP(storageRemote, path), "remote remove path");
        TEST_RESULT_BOOL(storagePathExistsNP(storageTest, strNewFmt("repo/%s", strPtr(path))), false, "path removed");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(path));      // path
        varLstAdd(paramList, varNewBool(true));    // recurse

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR, paramList, server), true,
            "  protocol path remove missing");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "  check result");

        bufUsedSet(serverWrite, 0);

        // Write the path and file to the repo and test the protocol
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRemote, strNewFmt("%s/file.txt", strPtr(path))), BUFSTRDEF("TEST")),
            "new path and file");
        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR, paramList, server), true,
            "  protocol path recurse remove");
        TEST_RESULT_BOOL(storagePathExistsNP(storageTest, strNewFmt("repo/%s", strPtr(path))), false, "  recurse path removed");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "  check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown pgbackrest %s/repo", testPath()))), 0, "update repo owner");

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");
        String *file = strNew("file.txt");

        // Write the file to the repo via the remote so owner is pgbackrest
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageRemote, file), BUFSTRDEF("TEST")), "new file");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("repo/%s", strPtr(file))), true, "file exists");
        TEST_RESULT_VOID(storageRemoveNP(storageRemote, file), "remote remove file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("repo/%s", strPtr(file))), false, "file removed");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(file));
        varLstAdd(paramList, varNewBool(true));

        TEST_ERROR_FMT(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_REMOVE_STR, paramList, server), FileRemoveError,
            "raised from remote-0 protocol on 'localhost': unable to remove '%s/repo/file.txt': "
            "[2] No such file or directory", testPath());

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(file));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_REMOVE_STR, paramList, server), true,
            "protocol file remove - no error on missing");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);

        // Write the file to the repo via the remote and test the protocol
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageRemote, file), BUFSTRDEF("TEST")), "new file");
        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_REMOVE_STR, paramList, server), true,
            "protocol file remove");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("repo/%s", strPtr(file))), false, "  confirm file removed");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chown pgbackrest %s/repo", testPath()))), 0, "update repo owner");

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");

        String *path = strNew("testpath");
        TEST_RESULT_VOID(storagePathCreateNP(storageRemote, path), "new path");
        TEST_RESULT_VOID(storagePathSyncNP(storageRemote, path), "sync path");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(path));

        TEST_RESULT_BOOL(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR, paramList, server), true,
            "protocol path sync");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNew("anewpath")));
        TEST_ERROR_FMT(
            storageRemoteProtocol(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR, paramList, server), PathMissingError,
            "raised from remote-0 protocol on 'localhost': " STORAGE_ERROR_PATH_SYNC_MISSING,
            strPtr(strNewFmt("%s/repo/anewpath", testPath())));
    }

    // *****************************************************************************************************************************
    if (testBegin("UNIMPLEMENTED"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(strNew(STORAGE_TYPE_POSIX), true), "get remote repo storage");

        TEST_ERROR(storageInfoNP(storageRemote, strNew("file.txt")), AssertError, "NOT YET IMPLEMENTED");
    }

    protocolFree();

    FUNCTION_HARNESS_RESULT_VOID();
}
