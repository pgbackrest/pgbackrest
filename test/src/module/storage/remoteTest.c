/***********************************************************************************************************************************
Test Remote Storage
***********************************************************************************************************************************/
#include "command/backup/pageChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "config/protocol.h"
#include "postgres/interface.h"

#include "common/harnessConfig.h"
#include "common/harnessPack.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install remote command handler shim
    static const ProtocolServerHandler testRemoteHandlerList[] =
    {
        PROTOCOL_SERVER_HANDLER_OPTION_LIST
        PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST
    };

    hrnProtocolRemoteShimInstall(testRemoteHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testRemoteHandlerList));

    // Test storage
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // Load configuration and get pg remote storage. This must be done before starting the repo storage below to set the max remotes
    // allowed to 2. The protocol helper expects all remotes to have the same type so we are cheating here a bit, but without this
    // ordering the second remote will never be sent an explicit exit and may not save coverage data.
    StringList *argList = strLstNew();
    hrnCfgArgRawZ(argList, cfgOptStanza, "db");
    hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "20");
    hrnCfgArgRawZ(argList, cfgOptBufferSize, "16384");
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
    hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 256, "localhost");
    hrnCfgArgKeyRawZ(argList, cfgOptPgHostUser, 256, TEST_USER);
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 256, TEST_PATH "/pg256");
    hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
    HRN_CFG_LOAD(cfgCmdBackup, argList);

    const Storage *const storagePgWrite = storagePgGet(1, true);

    // Load configuration and get repo remote storage
    argList = strLstNew();
    hrnCfgArgRawZ(argList, cfgOptStanza, "db");
    hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "20");
    hrnCfgArgRawFmt(argList, cfgOptBufferSize, "%zu", ioBufferSize());
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
    hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 128, "localhost");
    hrnCfgArgKeyRawZ(argList, cfgOptRepoHostUser, 128, TEST_USER);
    hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 128, TEST_PATH "/repo128");
    hrnCfgArgRawZ(argList, cfgOptRepo, "128");
    HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleLocal);

    const Storage *const storageRepoWrite = storageRepoGet(0, true);
    const Storage *const storageRepo = storageRepoGet(0, false);

    // Create a file larger than the remote buffer size
    Buffer *contentBuf = bufNew(ioBufferSize() * 2);

    for (unsigned int contentIdx = 0; contentIdx < bufSize(contentBuf); contentIdx++)
        bufPtr(contentBuf)[contentIdx] = contentIdx % 2 ? 'A' : 'B';

    bufUsedSet(contentBuf, bufSize(contentBuf));

    // *****************************************************************************************************************************
    if (testBegin("storageInterface(), storageFeature, and storagePathP()"))
    {
        TEST_RESULT_UINT(storageInterface(storageRepoWrite).feature, storageInterface(storageTest).feature, "check features");
        TEST_RESULT_BOOL(storageFeature(storageRepoWrite, storageFeaturePath), true, "check path feature");
        TEST_RESULT_BOOL(storageFeature(storageRepoWrite, storageFeatureCompress), true, "check compress feature");
        TEST_RESULT_STR_Z(storagePathP(storageRepo, NULL), TEST_PATH "/repo128", "check repo path");
        TEST_RESULT_STR_Z(storagePathP(storageRepoWrite, NULL), TEST_PATH "/repo128", "check repo write path");
        TEST_RESULT_STR_Z(storagePathP(storagePgWrite, NULL), TEST_PATH "/pg256", "check pg write path");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing file/path");

        TEST_ERROR(
            storageInfoP(storageRepo, STRDEF(BOGUS_STR)), FileOpenError,
            "unable to get info for missing path/file '" TEST_PATH "/repo128/BOGUS'");
        TEST_RESULT_BOOL(storageInfoP(storageRepo, STRDEF(BOGUS_STR), .ignoreMissing = true).exists, false, "missing file/path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path info");

        storagePathCreateP(storageTest, STRDEF("repo128"));
        HRN_STORAGE_TIME(storageTest, "repo128", 1555160000);

        StorageInfo info = {.exists = false};
        TEST_ASSIGN(info, storageInfoP(storageRepo, NULL), "valid path");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0750, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file info");

        storagePutP(storageNewWriteP(storageRepoWrite, STRDEF("test"), .timeModified = 1555160001), BUFSTRDEF("TESTME"));

        TEST_ASSIGN(info, storageInfoP(storageRepo, STRDEF("test")), "valid file");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 6, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160001, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file info (basic level)");

        TEST_ASSIGN(info, storageInfoP(storageRepo, STRDEF("test"), .level = storageInfoLevelBasic), "file basic info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "type");
        TEST_RESULT_UINT(info.size, 6, "size");
        TEST_RESULT_INT(info.timeModified, 1555160001, "mod time");
        TEST_RESULT_STR(info.user, NULL, "user not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("special info");

        HRN_SYSTEM("mkfifo -m 666 " TEST_PATH "/repo128/fifo");

        TEST_ASSIGN(info, storageInfoP(storageRepo, STRDEF("fifo")), "valid fifo");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("link info");

        HRN_SYSTEM("ln -s ../repo128/test " TEST_PATH "/repo128/link");

        TEST_ASSIGN(info, storageInfoP(storageRepo, STRDEF("link")), "valid link");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "../repo128/test", "check link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("link info follow");

        TEST_ASSIGN(info, storageInfoP(storageRepo, STRDEF("link"), .followLink = true), "valid link follow");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 6, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfoList()"))
    {
        TEST_TITLE("path not found");

        HarnessStorageInfoListCallbackData callbackData =
        {
            .content = strNew(),
        };

        TEST_RESULT_BOOL(
            storageInfoListP(storageRepo, STRDEF(BOGUS_STR), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            false, "info list");
        TEST_RESULT_STR_Z(callbackData.content, "", "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list path and file");

        storagePutP(storageNewWriteP(storagePgWrite, STRDEF("test"), .timeModified = 1555160001), BUFSTRDEF("TESTME"));

#ifdef TEST_CONTAINER_REQUIRED
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF("noname"), .timeModified = 1555160002), BUFSTRDEF("NONAME"));
        HRN_SYSTEM_FMT("sudo chown 99999:99999 %s", strZ(storagePathP(storagePgWrite, STRDEF("noname"))));
#endif // TEST_CONTAINER_REQUIRED

        // Path timestamp must be set after file is created since file creation updates it
        HRN_STORAGE_TIME(storagePgWrite, NULL, 1555160000);

        TEST_RESULT_BOOL(
            storageInfoListP(storagePgWrite, NULL, hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc), true,
            "info list");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path, m=0750, u=" TEST_USER ", g=" TEST_GROUP "}\n"
#ifdef TEST_CONTAINER_REQUIRED
            "noname {file, s=6, m=0640, t=1555160002, u=99999, g=99999}\n"
#endif // TEST_CONTAINER_REQUIRED
            "test {file, s=6, m=0640, t=1555160001, u=" TEST_USER ", g=" TEST_GROUP "}\n",
            "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        TEST_ERROR_FMT(
            strZ(strNewBuf(storageGetP(storageNewReadP(storagePgWrite, STRDEF("test.txt"))))), FileMissingError,
            "raised from remote-0 shim protocol: " STORAGE_ERROR_READ_MISSING, TEST_PATH "/pg256/test.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read file without compression");

        HRN_STORAGE_PUT(storageTest, TEST_PATH "/repo128/test.txt", contentBuf);

        // Disable protocol compression in the storage object
        ((StorageRemote *)storageDriver(storageRepo))->compressLevel = 0;

        StorageRead *fileRead = NULL;
        TEST_ASSIGN(fileRead, storageNewReadP(storageRepo, STRDEF("test.txt")), "new file");
        TEST_RESULT_BOOL(bufEq(storageGetP(fileRead), contentBuf), true, "get file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(fileRead), false, "check ignore missing");
        TEST_RESULT_STR_Z(storageReadName(fileRead), TEST_PATH "/repo128/test.txt", "check name");
        TEST_RESULT_UINT(storageReadRemote(fileRead->driver, bufNew(32), false), 0, "nothing more to read");
        TEST_RESULT_UINT(((StorageReadRemote *)fileRead->driver)->protocolReadBytes, bufSize(contentBuf), "check read size");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageDriver(storageRepo))->compressLevel = 3;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read file with limit");

        TEST_ASSIGN(fileRead, storageNewReadP(storageRepo, STRDEF("test.txt"), .limit = VARUINT64(11)), "get file");
        TEST_RESULT_STR_Z(strNewBuf(storageGetP(fileRead)), "BABABABABAB", "check contents");
        TEST_RESULT_UINT(((StorageReadRemote *)fileRead->driver)->protocolReadBytes, 11, "check read size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read file with compression");

        TEST_ASSIGN(
            fileRead, storageNewReadP(storageRepo, STRDEF("test.txt"), .compressible = true), "get file (protocol compress)");
        TEST_RESULT_BOOL(bufEq(storageGetP(fileRead), contentBuf), true, "check contents");
        // We don't know how much protocol compression there will be exactly, but make sure this is some
        TEST_RESULT_BOOL(
            ((StorageReadRemote *)fileRead->driver)->protocolReadBytes < bufSize(contentBuf), true, "check compressed read size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        TEST_RESULT_PTR(storageGetP(storageNewReadP(storageRepo, STRDEF("missing.txt"), .ignoreMissing = true)), NULL, "get");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read with filters");

        HRN_STORAGE_PUT_Z(storageTest, TEST_PATH "/repo128/test.txt", "TESTDATA!");

        TEST_ASSIGN(
            fileRead, storageNewReadP(storageRepo, STRDEF(TEST_PATH "/repo128/test.txt"), .limit = VARUINT64(8)), "new read");

        IoFilterGroup *filterGroup = ioReadFilterGroup(storageReadIo(fileRead));
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTRZ("x"), NULL));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, decompressFilter(compressTypeGz));

        TEST_RESULT_STR_Z(strNewBuf(storageGetP(fileRead)), "TESTDATA", "check contents");

        TEST_RESULT_STR_Z(
            hrnPackToStr(ioFilterGroupResultAll(filterGroup)),
            "1:strid:size, 2:pack:<1:u64:8>, 3:strid:hash, 4:pack:<1:str:bbbcf2c59433f68f22376cd2439d6cd309378df6>,"
                " 5:strid:pg-chksum, 6:pack:<2:bool:false, 3:bool:false>, 7:strid:cipher-blk, 9:strid:cipher-blk, 11:strid:gz-cmp,"
                " 13:strid:gz-dcmp, 15:strid:buffer",
            "filter results");

        // Check protocol function directly (file exists but all data goes to sink)
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read into sink (no data returned)");

        HRN_STORAGE_PUT_Z(storageTest, TEST_PATH "/repo128/test.txt", "TESTDATA");

        TEST_ASSIGN(
            fileRead, storageNewReadP(storageRepo, STRDEF(TEST_PATH "/repo128/test.txt"), .limit = VARUINT64(8)), "new read");

        filterGroup = ioReadFilterGroup(storageReadIo(fileRead));
        ioFilterGroupAdd(filterGroup, ioSizeNew());
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
        ioFilterGroupAdd(filterGroup, ioSinkNew());

        TEST_RESULT_STR_Z(strNewBuf(storageGetP(fileRead)), "", "no content");

        TEST_RESULT_STR_Z(
            hrnPackToStr(ioFilterGroupResultAll(filterGroup)),
            "1:strid:size, 2:pack:<1:u64:8>, 3:strid:hash, 4:pack:<1:str:bbbcf2c59433f68f22376cd2439d6cd309378df6>, 5:strid:sink,"
                " 7:strid:buffer",
            "filter results");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid filter");

        PackWrite *filterWrite = pckWriteNewP();
        pckWriteStrIdP(filterWrite, STRID5("bogus", 0x13a9de20));
        pckWriteEndP(filterWrite);

        TEST_ERROR(
            storageRemoteFilterGroup(ioFilterGroupNew(), pckWriteResult(filterWrite)), AssertError, "unable to add filter 'bogus'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write with smaller buffer size than remote");

        ioBufferSizeSet(9999);

        // Disable protocol compression in the storage object to test no compression
        ((StorageRemote *)storageDriver(storageRepoWrite))->compressLevel = 0;

        StorageWrite *write = NULL;
        TEST_ASSIGN(write, storageNewWriteP(storageRepoWrite, STRDEF("test.txt")), "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
        TEST_RESULT_UINT(storageWriteModeFile(write), STORAGE_MODE_FILE_DEFAULT, "file mode is default");
        TEST_RESULT_UINT(storageWriteModePath(write), STORAGE_MODE_PATH_DEFAULT, "path mode is default");
        TEST_RESULT_STR_Z(storageWriteName(write), TEST_PATH "/repo128/test.txt", "check file name");
        TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
        TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");

        TEST_RESULT_VOID(storagePutP(write, contentBuf), "write file");
        TEST_RESULT_UINT(((StorageWriteRemote *)write->driver)->protocolWriteBytes, bufSize(contentBuf), "check write size");
        TEST_RESULT_VOID(storageWriteRemoteClose(write->driver), "close file again");
        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        // Make sure the file was written correctly
        TEST_RESULT_BOOL(bufEq(storageGetP(storageNewReadP(storageRepo, STRDEF("test.txt"))), contentBuf), true, "check file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write with larger buffer than remote");

        ioBufferSizeSet(ioBufferSize() * 2);

        TEST_ASSIGN(write, storageNewWriteP(storageRepoWrite, STRDEF("test.txt")), "new write file");
        TEST_RESULT_VOID(storagePutP(write, contentBuf), "write file");
        TEST_RESULT_BOOL(bufEq(storageGetP(storageNewReadP(storageRepo, STRDEF("test.txt"))), contentBuf), true, "check file");

        // Enable protocol compression in the storage object
        ((StorageRemote *)storageDriver(storageRepoWrite))->compressLevel = 3;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file, free before close, make sure the .tmp file remains");

        TEST_ASSIGN(write, storageNewWriteP(storageRepoWrite, STRDEF("test2.txt")), "new write file");

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(write)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(write), contentBuf), "write bytes");

        TEST_RESULT_VOID(storageWriteFree(write), "free file");

        TEST_RESULT_UINT(
            storageInfoP(storageTest, STRDEF("repo128/test2.txt.pgbackrest.tmp")).size, 16384, "file exists and is partial");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write the file again with protocol compression");

        TEST_ASSIGN(
            write, storageNewWriteP(storageRepoWrite, STRDEF("test2.txt"), .compressible = true), "new write file (compress)");
        TEST_RESULT_VOID(storagePutP(write, contentBuf), "write file");
        TEST_RESULT_BOOL(
            ((StorageWriteRemote *)write->driver)->protocolWriteBytes < bufSize(contentBuf), true, "check compressed write size");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        const String *path = STRDEF("testpath");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create path via the remote");

        // Check the repo via the local test storage to ensure the remote created it.
        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite, path), "new path");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, strNewFmt("repo128/%s", strZ(path))), "get path info");
        TEST_RESULT_BOOL(info.exists, true, "path exists");
        TEST_RESULT_INT(info.mode, STORAGE_MODE_PATH_DEFAULT, "mode is default");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on existing path");

        TEST_ERROR(
            storagePathCreateP(storageRepoWrite, STRDEF("testpath"), .errorOnExists = true), PathCreateError,
            "raised from remote-0 shim protocol: unable to create path '" TEST_PATH "/repo128/testpath': [17] File exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing parent path");

        TEST_ERROR(
            storagePathCreateP(storageRepoWrite, STRDEF("parent/testpath"), .noParentCreate = true), PathCreateError,
            "raised from remote-0 shim protocol: unable to create path '" TEST_PATH "/repo128/parent/testpath': [2] No such"
                " file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create parent/path with non-default mode");

        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite, STRDEF("parent/testpath"), .mode = 0777), "path create");

        HarnessStorageInfoListCallbackData callbackData = {.content = strNew(), .userOmit = true, .groupOmit = true};

        TEST_RESULT_BOOL(
            storageInfoListP(
                storageRepo, STRDEF(TEST_PATH "/repo128/parent"), hrnStorageInfoListCallback, &callbackData,
                .sortOrder = sortOrderAsc),
            true, "info list");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path, m=0777}\n"
            "testpath {path, m=0777}\n",
            "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        const String *path = STRDEF("testpath");
        storagePathCreateP(storageTest, STRDEF("repo128"));
        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite, path), "new path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote remove path");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo128/%s", strZ(path))), true, "path exists");
        TEST_RESULT_VOID(storagePathRemoveP(storageRepoWrite, path), "remote remove path");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo128/%s", strZ(path))), false, "path removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove recursive");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite, strNewFmt("%s/file.txt", strZ(path))), BUFSTRDEF("TEST")),
            "new path and file");

        TEST_RESULT_VOID(storagePathRemoveP(storageRepoWrite, STRDEF("testpath"), .recurse = true), "remove missing path");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewFmt("repo128/%s", strZ(path))), false, "recurse path removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        storagePathCreateP(storageTest, STRDEF("repo128"));

        const String *file = STRDEF("file.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote remove path");

        // Write the file to the repo via the remote so owner is pgbackrest
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite, file), BUFSTRDEF("TEST")), "new file");

        // Check the repo via the local test storage to ensure the remote wrote it, then remove via the remote and confirm removed
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNewFmt("repo128/%s", strZ(file))), true, "file exists");
        TEST_RESULT_VOID(storageRemoveP(storageRepoWrite, file), "remote remove file");
        TEST_RESULT_BOOL(storageExistsP(storageTest, strNewFmt("repo128/%s", strZ(file))), false, "file removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing file");

        TEST_ERROR(
            storageRemoveP(storageRepoWrite, file, .errorOnMissing = true), FileRemoveError,
            "raised from remote-0 shim protocol: unable to remove '" TEST_PATH "/repo128/file.txt': [2] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing file");

        TEST_RESULT_VOID(storageRemoveP(storageRepoWrite, file), "remove missing");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathSync()"))
    {
        storagePathCreateP(storageTest, STRDEF("repo128"));

        const String *path = STRDEF("testpath");
        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite, path), "new path");
        TEST_RESULT_VOID(storagePathSyncP(storageRepoWrite, path), "sync path");
    }

    // When clients are freed by protocolClientFree() they do not wait for a response. We need to wait for a response here to be
    // sure coverage data has been written by the remote. We also need to make sure that the mem context callback is cleared so that
    // protocolClientFreeResource() will not be called and send another exit. protocolFree() is still required to free the client
    // objects.
    memContextCallbackClear(objMemContext(protocolRemoteGet(protocolStorageTypeRepo, 0)));
    protocolClientExecute(protocolRemoteGet(protocolStorageTypeRepo, 0), protocolCommandNew(PROTOCOL_COMMAND_EXIT), false);
    memContextCallbackClear(objMemContext(protocolRemoteGet(protocolStorageTypePg, 1)));
    protocolClientExecute(protocolRemoteGet(protocolStorageTypePg, 1), protocolCommandNew(PROTOCOL_COMMAND_EXIT), false);
    protocolFree();

    FUNCTION_HARNESS_RETURN_VOID();
}
