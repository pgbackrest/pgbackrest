/***********************************************************************************************************************************
Test Remote Storage
***********************************************************************************************************************************/
#include "command/backup/pageChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface.h"

#include "common/harnessConfig.h"
#include "common/harnessStorage.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Test storage
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // Load configuration to set repo-path and stanza
    StringList *argList = strLstNew();
    strLstAddZ(argList, "--stanza=db");
    strLstAddZ(argList, "--protocol-timeout=10");
    strLstAddZ(argList, "--buffer-size=16384");
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
    strLstAddZ(argList, "--repo1-host=localhost");
    strLstAddZ(argList, "--repo1-host-user=" TEST_USER);
    strLstAddZ(argList, "--repo1-path=" TEST_PATH "/repo");
    hrnCfgArgRawZ(argList, cfgOptRepo, "1");
    harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleLocal, argList);

    // Set type since we'll be running local and remote tests here
    cfgOptionSet(cfgOptRemoteType, cfgSourceParam, VARSTRDEF("repo"));

    // Set pg host so we can run both pg and repo remotes
    cfgOptionSet(cfgOptPgHost, cfgSourceParam, VARSTRDEF("localhost"));

    // Start a protocol server to test the remote protocol
    Buffer *serverRead = bufNew(8192);
    Buffer *serverWrite = bufNew(8192);
    IoRead *serverReadIo = ioBufferReadNew(serverRead);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioReadOpen(serverReadIo);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(STRDEF("test"), STRDEF("test"), serverReadIo, serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, false), "get remote repo storage");
        TEST_RESULT_UINT(storageInterface(storageRemote).feature, storageInterface(storageTest).feature, "    check features");
        TEST_RESULT_BOOL(storageFeature(storageRemote, storageFeaturePath), true, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storageRemote, storageFeatureCompress), true, "    check compress feature");
        TEST_RESULT_STR_Z(storagePathP(storageRemote, NULL), TEST_PATH "/repo", "    check path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly (pg)");

        cfgOptionSet(cfgOptRemoteType, cfgSourceParam, VARSTRDEF("pg"));

        TEST_RESULT_VOID(storageRemoteFeatureProtocol(NULL, server), "protocol feature");
        TEST_RESULT_STR(
            strNewBuf(serverWrite), strNewFmt(".\"" TEST_PATH "/repo\"\n.%" PRIu64 "\n{}\n", storageInterface(storageTest).feature),
            "check result");
        bufUsedSet(serverWrite, 0);

        TEST_RESULT_VOID(storageRemoteFeatureProtocol(NULL, server), "protocol feature");
        TEST_RESULT_STR(
            strNewBuf(serverWrite), strNewFmt(".\"" TEST_PATH "/repo\"\n.%" PRIu64 "\n{}\n", storageInterface(storageTest).feature),
            "check result cache");
        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly (repo)");

        storageRemoteProtocolLocal.memContext = NULL;
        cfgOptionSet(cfgOptRemoteType, cfgSourceParam, VARSTRDEF("repo"));

        TEST_RESULT_VOID(storageRemoteFeatureProtocol(NULL, server), "protocol feature");
        TEST_RESULT_STR(
            strNewBuf(serverWrite), strNewFmt(".\"" TEST_PATH "/repo\"\n.%" PRIu64 "\n{}\n", storageInterface(storageTest).feature),
            "check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing file/path");

        TEST_ERROR(
            storageInfoP(storageRemote, STRDEF(BOGUS_STR)), FileOpenError,
            "unable to get info for missing path/file '" TEST_PATH "/repo/BOGUS'");
        TEST_RESULT_BOOL(storageInfoP(storageRemote, STRDEF(BOGUS_STR), .ignoreMissing = true).exists, false, "missing file/path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path info");

        storagePathCreateP(storageTest, STRDEF("repo"));
        HRN_STORAGE_TIME(storageTest, "repo", 1555160000);

        StorageInfo info = {.exists = false};
        TEST_ASSIGN(info, storageInfoP(storageRemote, NULL), "valid path");
        TEST_RESULT_STR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0750, "    check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "    check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "    check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "    check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "    check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file info");

        storagePutP(storageNewWriteP(storageRemote, STRDEF("test"), .timeModified = 1555160001), BUFSTRDEF("TESTME"));

        TEST_ASSIGN(info, storageInfoP(storageRemote, STRDEF("test")), "valid file");
        TEST_RESULT_STR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "    check type");
        TEST_RESULT_UINT(info.size, 6, "    check size");
        TEST_RESULT_INT(info.mode, 0640, "    check mode");
        TEST_RESULT_INT(info.timeModified, 1555160001, "    check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "    check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "    check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "    check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("special info");

        TEST_SYSTEM_FMT("mkfifo -m 666 %s", strZ(storagePathP(storageTest, STRDEF("repo/fifo"))));

        TEST_ASSIGN(info, storageInfoP(storageRemote, STRDEF("fifo")), "valid fifo");
        TEST_RESULT_STR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0666, "    check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "    check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "    check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "    check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("link info");

        TEST_SYSTEM_FMT("ln -s ../repo/test %s", strZ(storagePathP(storageTest, STRDEF("repo/link"))));

        TEST_ASSIGN(info, storageInfoP(storageRemote, STRDEF("link")), "valid link");
        TEST_RESULT_STR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "    check type");
        TEST_RESULT_UINT(info.size, 0, "    check size");
        TEST_RESULT_INT(info.mode, 0777, "    check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "../repo/test", "    check link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "    check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "    check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "    check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("link info follow");

        TEST_ASSIGN(info, storageInfoP(storageRemote, STRDEF("link"), .followLink = true), "valid link follow");
        TEST_RESULT_STR(info.name, NULL, "    name is not set");
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "    check type");
        TEST_RESULT_UINT(info.size, 6, "    check size");
        TEST_RESULT_INT(info.mode, 0640, "    check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "    no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "    check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "    check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "    check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "    check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("protocol output that is not tested elsewhere (detail)");

        info = (StorageInfo){.level = storageInfoLevelDetail, .type = storageTypeLink, .linkDestination = STRDEF("../")};
        TEST_RESULT_VOID(storageRemoteInfoWrite(server, &info), "write link info");

        ioWriteFlush(serverWriteIo);
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), ".2\n.0\n.0\n.null\n.0\n.null\n.0\n.\"../\"\n", "check result");

        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly with missing path/file");

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(BOGUS_STR));
        varLstAdd(paramList, varNewUInt(storageInfoLevelBasic));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_VOID(storageRemoteInfoProtocol(paramList, server), "protocol list");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":false}\n", "check result");

        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly with a file (basic level)");

        // Do these tests against pg for coverage.  We're not really going to get a pg remote here because the remote for this host
        // id has already been created.  This will just test that the pg storage is selected to provide coverage.
        cfgOptionSet(cfgOptRemoteType, cfgSourceParam, VARSTRDEF("pg"));

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test"));
        varLstAdd(paramList, varNewUInt(storageInfoLevelBasic));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_VOID(storageRemoteInfoProtocol(paramList, server), "protocol list");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{\"out\":true}\n"
            ".0\n.1555160001\n.6\n"
            "{}\n",
            "check result");

        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly with a file (detail level)");

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test"));
        varLstAdd(paramList, varNewUInt(storageInfoLevelDetail));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_VOID(storageRemoteInfoProtocol(paramList, server), "protocol list");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{\"out\":true}\n"
            ".0\n.1555160001\n.6\n." TEST_USER_ID_Z "\n.\"" TEST_USER "\"\n." TEST_GROUP_ID_Z "\n.\"" TEST_GROUP "\"\n.416\n"
            "{}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfoList()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path not found");

        HarnessStorageInfoListCallbackData callbackData =
        {
            .content = strNew(),
        };

        TEST_RESULT_BOOL(
            storageInfoListP(
                storageRemote, STRDEF(BOGUS_STR), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            false, "info list");
        TEST_RESULT_STR_Z(
            callbackData.content, "", "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list path and file");

        storagePathCreateP(storageRemote, NULL);
        storagePutP(storageNewWriteP(storageRemote, STRDEF("test"), .timeModified = 1555160001), BUFSTRDEF("TESTME"));

        // Path timestamp must be set after file is created since file creation updates it
        HRN_STORAGE_TIME(storageRemote, NULL, 1555160000);

        TEST_RESULT_BOOL(
            storageInfoListP(storageRemote, NULL, hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            true, "info list");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path, m=0750, u=" TEST_USER ", g=" TEST_GROUP "}\n"
            "test {file, s=6, m=0640, t=1555160001, u=" TEST_USER ", g=" TEST_GROUP "}\n",
            "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check protocol function directly with a path");

        // Remove the file since ordering cannot be guaranteed in the protocol results
        storageRemoveP(storageRemote, STRDEF("test"), .errorOnMissing = true);

        // Path timestamp must be set after file is removed since file removal updates it
        HRN_STORAGE_TIME(storageRemote, NULL, 1555160000);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo"));
        varLstAdd(paramList, varNewUInt(storageInfoLevelDetail));

        TEST_RESULT_VOID(storageRemoteInfoListProtocol(paramList, server), "call protocol");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            ".\".\"\n.1\n.1555160000\n." TEST_USER_ID_Z "\n.\"" TEST_USER "\"\n." TEST_GROUP_ID_Z "\n.\"" TEST_GROUP "\"\n.488\n"
            ".\n"
            "{\"out\":true}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, false), "get remote repo storage");
        storagePathCreateP(storageTest, STRDEF("repo"));

        Buffer *contentBuf = bufNew(32768);

        for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
            bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

        bufUsedSet(contentBuf, bufSize(contentBuf));

        TEST_ERROR_FMT(
            strZ(strNewBuf(storageGetP(storageNewReadP(storageRemote, STRDEF("test.txt"))))), FileMissingError,
            "raised from remote-0 protocol on 'localhost': " STORAGE_ERROR_READ_MISSING,
            strZ(strNewFmt("%s/repo/test.txt", TEST_PATH)));

        storagePutP(storageNewWriteP(storageTest, STRDEF("repo/test.txt")), contentBuf);

        // Disable protocol compression in the storage object to test no compression
        ((StorageRemote *)storageDriver(storageRemote))->compressLevel = 0;

        StorageRead *fileRead = NULL;

        ioBufferSizeSet(8193);
        TEST_ASSIGN(fileRead, storageNewReadP(storageRemote, STRDEF("test.txt")), "new file");
        TEST_RESULT_BOOL(bufEq(storageGetP(fileRead), contentBuf), true, "get file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(fileRead), false, "check ignore missing");
        TEST_RESULT_STR_Z(storageReadName(fileRead), TEST_PATH "/repo/test.txt", "check name");
        TEST_RESULT_UINT(storageReadRemote(fileRead->driver, bufNew(32), false), 0, "nothing more to read");

        TEST_ASSIGN(fileRead, storageNewReadP(storageRemote, STRDEF("test.txt")), "get file");
        TEST_RESULT_BOOL(bufEq(storageGetP(fileRead), contentBuf), true, "    check contents");
        TEST_RESULT_UINT(((StorageReadRemote *)fileRead->driver)->protocolReadBytes, bufSize(contentBuf), "    check read size");

        TEST_ASSIGN(fileRead, storageNewReadP(storageRemote, STRDEF("test.txt"), .limit = VARUINT64(11)), "get file");
        TEST_RESULT_STR_Z(strNewBuf(storageGetP(fileRead)), "BABABABABAB", "    check contents");
        TEST_RESULT_UINT(((StorageReadRemote *)fileRead->driver)->protocolReadBytes, 11, "    check read size");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageDriver(storageRemote))->compressLevel = 3;

        TEST_ASSIGN(
            fileRead, storageNewReadP(storageRemote, STRDEF("test.txt"), .compressible = true), "get file (protocol compress)");
        TEST_RESULT_BOOL(bufEq(storageGetP(fileRead), contentBuf), true, "    check contents");
        // We don't know how much protocol compression there will be exactly, but make sure this is some
        TEST_RESULT_BOOL(
            ((StorageReadRemote *)fileRead->driver)->protocolReadBytes < bufSize(contentBuf), true,
            "    check compressed read size");

        TEST_ERROR(
            storageRemoteProtocolBlockSize(STRDEF("bogus")), ProtocolError, "'bogus' is not a valid block size message");

        // Check protocol function directly (file missing)
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(STRDEF("missing.txt")));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewVarLst(varLstNew()));

        TEST_RESULT_VOID(storageRemoteOpenReadProtocol(paramList, server), "protocol open read (missing)");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":false}\n", "check result");

        bufUsedSet(serverWrite, 0);

        // Check protocol function directly (file exists)
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(storageNewWriteP(storageTest, STRDEF("repo/test.txt")), BUFSTRDEF("TESTDATA!"));
        ioBufferSizeSet(4);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test.txt"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewUInt64(8));

        // Create filters to test filter logic
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, decompressFilter(compressTypeGz));
        varLstAdd(paramList, ioFilterGroupParamAll(filterGroup));

        TEST_RESULT_VOID(storageRemoteOpenReadProtocol(paramList, server), "protocol open read");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{\"out\":true}\n"
                "BRBLOCK4\n"
                "TESTBRBLOCK4\n"
                "DATABRBLOCK0\n"
                "{\"out\":{\"buffer\":null,\"cipherBlock\":null,\"gzCompress\":null,\"gzDecompress\":null"
                    ",\"hash\":\"bbbcf2c59433f68f22376cd2439d6cd309378df6\",\"pageChecksum\":{\"align\":false,\"valid\":false}"
                    ",\"size\":8}}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
        ioBufferSizeSet(8192);

        // Check protocol function directly (file exists but all data goes to sink)
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(storageNewWriteP(storageTest, STRDEF("repo/test.txt")), BUFSTRDEF("TESTDATA"));

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test.txt"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, NULL);

        // Create filters to test filter logic
        filterGroup = ioFilterGroupNew();
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, ioSinkNew());
        varLstAdd(paramList, ioFilterGroupParamAll(filterGroup));

        TEST_RESULT_VOID(storageRemoteOpenReadProtocol(paramList, server), "protocol open read (sink)");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{\"out\":true}\n"
                "BRBLOCK0\n"
                "{\"out\":{\"buffer\":null,\"hash\":\"bbbcf2c59433f68f22376cd2439d6cd309378df6\",\"sink\":null,\"size\":8}}\n",
            "check result");

        bufUsedSet(serverWrite, 0);

        // Check for error on a bogus filter
        // -------------------------------------------------------------------------------------------------------------------------
        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test.txt"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewVarLst(varLstAdd(varLstNew(), varNewKv(kvPut(kvNew(), varNewStrZ("bogus"), NULL)))));

        TEST_ERROR(storageRemoteOpenReadProtocol(paramList, server), AssertError, "unable to add filter 'bogus'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        storagePathCreateP(storageTest, STRDEF("repo"));

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");

        // Create buffer with plenty of data
        Buffer *contentBuf = bufNew(32768);

        for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
            bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

        bufUsedSet(contentBuf, bufSize(contentBuf));

        // Write the file
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(9999);

        // Disable protocol compression in the storage object to test no compression
        ((StorageRemote *)storageDriver(storageRemote))->compressLevel = 0;

        StorageWrite *write = NULL;
        TEST_ASSIGN(write, storageNewWriteP(storageRemote, STRDEF("test.txt")), "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
        TEST_RESULT_UINT(storageWriteModeFile(write), STORAGE_MODE_FILE_DEFAULT, "file mode is default");
        TEST_RESULT_UINT(storageWriteModePath(write), STORAGE_MODE_PATH_DEFAULT, "path mode is default");
        TEST_RESULT_STR_Z(storageWriteName(write), TEST_PATH "/repo/test.txt", "check file name");
        TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
        TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");

        TEST_RESULT_VOID(storagePutP(write, contentBuf), "write file");
        TEST_RESULT_UINT(((StorageWriteRemote *)write->driver)->protocolWriteBytes, bufSize(contentBuf), "    check write size");
        TEST_RESULT_VOID(storageWriteRemoteClose(write->driver), "close file again");
        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        // Make sure the file was written correctly
        TEST_RESULT_BOOL(
            bufEq(storageGetP(storageNewReadP(storageRemote, STRDEF("test.txt"))), contentBuf), true, "check file");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageDriver(storageRemote))->compressLevel = 3;

        // Write the file again, but this time free it before close and make sure the .tmp file is left
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(write, storageNewWriteP(storageRemote, STRDEF("test2.txt")), "new write file");

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(write)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(write), contentBuf), "write bytes");

        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        TEST_RESULT_UINT(
            storageInfoP(storageTest, STRDEF("repo/test2.txt.pgbackrest.tmp")).size, 16384, "file exists and is partial");

        // Write the file again with protocol compression
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(write, storageNewWriteP(storageRemote, STRDEF("test2.txt"), .compressible = true), "new write file (compress)");
        TEST_RESULT_VOID(storagePutP(write, contentBuf), "write file");
        TEST_RESULT_BOOL(
            ((StorageWriteRemote *)write->driver)->protocolWriteBytes < bufSize(contentBuf), true,
            "    check compressed write size");

        // Check protocol function directly (complete write)
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(10);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test3.txt"));
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

        TEST_RESULT_VOID(storageRemoteOpenWriteProtocol(paramList, server), "protocol open write");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{}\n"
            "{\"out\":{\"buffer\":null,\"size\":18}}\n",
            "check result");

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("repo/test3.txt")))), "ABC123456789012345",
            "check file");

        bufUsedSet(serverWrite, 0);

        // Check protocol function directly (free before write is closed)
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(10);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/test4.txt"));
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

        TEST_RESULT_VOID(storageRemoteOpenWriteProtocol(paramList, server), "protocol open write");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{}\n"
            "{}\n",
            "check result");

        bufUsedSet(serverWrite, 0);
        ioBufferSizeSet(8192);

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("repo/test4.txt.pgbackrest.tmp")))), "", "check file");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        const String *path = STRDEF("testpath");
        storagePathCreateP(storageTest, STRDEF("repo"));

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");

        // Create a path via the remote. Check the repo via the local test storage to ensure the remote created it.
        TEST_RESULT_VOID(storagePathCreateP(storageRemote, path), "new path");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, strNewFmt("repo/%s", strZ(path))), "  get path info");
        TEST_RESULT_BOOL(info.exists, true, "  path exists");
        TEST_RESULT_INT(info.mode, STORAGE_MODE_PATH_DEFAULT, "  mode is default");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(path))));
        varLstAdd(paramList, varNewBool(true));     // errorOnExists
        varLstAdd(paramList, varNewBool(true));     // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0));      // path mode

        TEST_ERROR_FMT(
            storageRemotePathCreateProtocol(paramList, server), PathCreateError,
            "raised from remote-0 protocol on 'localhost': unable to create path '%s/repo/testpath': [17] File exists",
            TEST_PATH);

        // Error if parent path not exist
        path = STRDEF("parent/testpath");
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(path))));
        varLstAdd(paramList, varNewBool(false));    // errorOnExists
        varLstAdd(paramList, varNewBool(true));     // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0));      // path mode

        TEST_ERROR_FMT(
            storageRemotePathCreateProtocol(paramList, server), PathCreateError,
            "raised from remote-0 protocol on 'localhost': unable to create path '%s/repo/parent/testpath': "
            "[2] No such file or directory", TEST_PATH);

        // Create parent and path with default mode
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(path))));
        varLstAdd(paramList, varNewBool(true));     // errorOnExists
        varLstAdd(paramList, varNewBool(false));    // noParentCreate (true=error if it does not have a parent, false=create parent)
        varLstAdd(paramList, varNewUInt64(0777));   // path mode

        TEST_RESULT_VOID(storageRemotePathCreateProtocol(paramList, server), "create parent and path");
        TEST_ASSIGN(info, storageInfoP(storageTest, strNewFmt("repo/%s", strZ(path))), "  get path info");
        TEST_RESULT_BOOL(info.exists, true, "  path exists");
        TEST_RESULT_INT(info.mode, 0777, "  mode is set");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        const String *path = STRDEF("testpath");
        storagePathCreateP(storageTest, STRDEF("repo"));

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");
        TEST_RESULT_VOID(storagePathCreateP(storageRemote, path), "new path");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo/%s", strZ(path))), true, "path exists");
        TEST_RESULT_VOID(storagePathRemoveP(storageRemote, path), "remote remove path");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo/%s", strZ(path))), false, "path removed");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(path))));
        varLstAdd(paramList, varNewBool(true));    // recurse

        TEST_RESULT_VOID(storageRemotePathRemoveProtocol(paramList, server), "  protocol path remove missing");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":false}\n", "  check result");

        bufUsedSet(serverWrite, 0);

        // Write the path and file to the repo and test the protocol
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRemote, strNewFmt("%s/file.txt", strZ(path))), BUFSTRDEF("TEST")),
            "new path and file");
        TEST_RESULT_VOID(storageRemotePathRemoveProtocol(paramList, server), "  protocol path recurse remove");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo/%s", strZ(path))), false, "  recurse path removed");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":true}\n", "  check result");

        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        storagePathCreateP(storageTest, STRDEF("repo"));

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");
        const String *file = STRDEF("file.txt");

        // Write the file to the repo via the remote so owner is pgbackrest
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRemote, file), BUFSTRDEF("TEST")), "new file");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNewFmt("repo/%s", strZ(file))), true, "file exists");
        TEST_RESULT_VOID(storageRemoveP(storageRemote, file), "remote remove file");
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNewFmt("repo/%s", strZ(file))), false, "file removed");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(file))));
        varLstAdd(paramList, varNewBool(true));

        TEST_ERROR_FMT(
            storageRemoteRemoveProtocol(paramList, server), FileRemoveError,
            "raised from remote-0 protocol on 'localhost': unable to remove '%s/repo/file.txt': "
            "[2] No such file or directory", TEST_PATH);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(file))));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_VOID(storageRemoteRemoveProtocol(paramList, server), "protocol file remove - no error on missing");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);

        // Write the file to the repo via the remote and test the protocol
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRemote, file), BUFSTRDEF("TEST")), "new file");
        TEST_RESULT_VOID(storageRemoteRemoveProtocol(paramList, server), "protocol file remove");
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNewFmt("repo/%s", strZ(file))), false, "  confirm file removed");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
        storagePathCreateP(storageTest, STRDEF("repo"));

        Storage *storageRemote = NULL;
        TEST_ASSIGN(storageRemote, storageRepoGet(0, true), "get remote repo storage");

        const String *path = STRDEF("testpath");
        TEST_RESULT_VOID(storagePathCreateP(storageRemote, path), "new path");
        TEST_RESULT_VOID(storagePathSyncP(storageRemote, path), "sync path");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt(TEST_PATH "/repo/%s", strZ(path))));

        TEST_RESULT_VOID(storageRemotePathSyncProtocol(paramList, server), "protocol path sync");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{}\n", "  check result");
        bufUsedSet(serverWrite, 0);

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(TEST_PATH "/repo/anewpath"));
        TEST_ERROR_FMT(
            storageRemotePathSyncProtocol(paramList, server), PathMissingError,
            "raised from remote-0 protocol on 'localhost': " STORAGE_ERROR_PATH_SYNC_MISSING,
            strZ(strNewFmt("%s/repo/anewpath", TEST_PATH)));
    }

    protocolFree();

    FUNCTION_HARNESS_RETURN_VOID();
}
