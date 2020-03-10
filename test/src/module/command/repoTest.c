/***********************************************************************************************************************************
Test Repo Commands
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("cmdRepoCreate()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdRepoCreate, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("noop on non-S3 storage");

        TEST_RESULT_VOID(cmdRepoCreate(), "repo create");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStorageList() and storageListRender()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo", testPath()));
        strLstAddZ(argList, "--output=text");
        strLstAddZ(argList, "--sort=none");
        harnessCfgLoad(cfgCmdRepoLs, argList);

        // Missing directory
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}",
            "    check output");

        // Empty directory
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageTest, strNew("repo"), .mode = 0700);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}",
            "    check output");

        // Add path and file
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("asc"));

        storagePathCreateP(storageTest, strNew("repo/bbb"));
        storagePutP(storageNewWriteP(storageTest, strNew("repo/aaa"), .timeModified = 1578671569), BUFSTRDEF("TESTDATA"));
        storagePutP(storageNewWriteP(storageTest, strNew("repo/bbb/ccc")), BUFSTRDEF("TESTDATA2"));

        ASSERT(system(strPtr(strNewFmt("ln -s ../bbb %s/repo/link", testPath()))) == 0);
        ASSERT(system(strPtr(strNewFmt("mkfifo %s/repo/pipe", testPath()))) == 0);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa\nbbb\nlink\npipe", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"},"
                    "\"aaa\":{\"type\":\"file\",\"size\":8,\"time\":1578671569},"
                    "\"bbb\":{\"type\":\"path\"},"
                    "\"link\":{\"type\":\"link\",\"destination\":\"../bbb\"},"
                    "\"pipe\":{\"type\":\"special\"}"
                "}",
            "    check output");

        // Reverse sort
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("desc"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb\naaa", "    check output");

        // Recurse
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptRecurse, true);
        cfgOptionSet(cfgOptRecurse, cfgSourceParam, VARBOOL(true));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb/ccc\nbbb\naaa", "    check output");

        // Filter
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptFilter, true);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("^aaa$"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa", "    check output");

        // Subdirectory
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "bbb");
        harnessCfgLoad(cfgCmdRepoLs, argListTmp);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "subdirectory");
        TEST_RESULT_STR_Z(strNewBuf(output), "ccc", "    check output");

        // -------------------------------------------------------------------------------------------------------------------------
        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.txt", testPath());

        THROW_ON_SYS_ERROR(freopen(strPtr(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdStorageList();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(storageTest, stdoutFile))), "ccc\n", "    check text");

        // Too many paths
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argListTmp, "ccc");
        harnessCfgLoad(cfgCmdRepoLs, argListTmp);

        output = bufNew(0);
        TEST_ERROR(storageListRender(ioBufferWriteNew(output)), ParamInvalidError, "only one path may be specified");

        // File
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo/aaa", testPath()));
        strLstAddZ(argList, "--output=json");
        harnessCfgLoad(cfgCmdRepoLs, argList);

        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"file\",\"size\":8,\"time\":1578671569}"
                "}",
            "    check output");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStoragePut() and cmdStorageGet()"))
    {
        // Set buffer size small so copy loops get exercised
        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(8);

        // Needed for tests
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "xxx", true);

        // Test files and buffers
        const String *fileName = STRDEF("file.txt");
        const Buffer *fileBuffer = BUFSTRDEF("TESTFILE");

        const String *fileEncName = STRDEF("file-enc.txt");
        const Buffer *fileEncBuffer = BUFSTRDEF("TESTFILE-ENC");

        const String *fileEncCustomName = STRDEF("file-enc-custom.txt");
        const Buffer *fileEncCustomBuffer = BUFSTRDEF("TESTFILE-ENC-CUSTOM");

        const String *fileRawName = STRDEF("file-raw.txt");
        const Buffer *fileRawBuffer = BUFSTRDEF("TESTFILE-RAW");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when missing destination");

        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_ERROR(storagePutProcess(ioBufferReadNew(fileBuffer)), ParamRequiredError, "destination file required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a file");

        strLstAdd(argList, fileName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put an encrypted file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAdd(argList, fileEncName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileEncBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put an encrypted file with custom key");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAdd(argList, fileEncCustomName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileEncCustomBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a raw file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAddZ(argList, "--raw");
        strLstAdd(argList, fileRawName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        // Get stdin from a file
        int stdinSave = dup(STDIN_FILENO);
        const String *stdinFile = storagePathP(storageRepo(), STRDEF("stdin.txt"));
        storagePutP(storageNewWriteP(storageRepoWrite(), stdinFile), fileRawBuffer);

        THROW_ON_SYS_ERROR(freopen(strPtr(stdinFile), "r", stdin) == NULL, FileWriteError, "unable to reopen stdin");

        TEST_RESULT_VOID(cmdStoragePut(), "put");

        // Restore normal stdout
        dup2(stdinSave, STDIN_FILENO);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when missing source");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdRepoGet, argList);

        TEST_ERROR(storageGetProcess(ioBufferWriteNew(bufNew(0))), ParamRequiredError, "source file required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a file");

        strLstAdd(argList, fileName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        Buffer *writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get an encrypted file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAdd(argList, fileEncName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileEncBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get an encrypted file with custom key");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAdd(argList, fileEncCustomName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileEncCustomBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a raw file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REPO1_CIPHER_TYPE "=" CIPHER_TYPE_AES_256_CBC);
        strLstAddZ(argList, "--raw");
        strLstAdd(argList, fileRawName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        TEST_LOG("get");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/repo/stdout.txt", testPath());

        THROW_ON_SYS_ERROR(freopen(strPtr(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        ASSERT(cmdStorageGet() == 0);

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_RESULT_STR(
            strNewBuf(storageGetP(storageNewReadP(storageRepo(), stdoutFile))), strNewBuf(fileRawBuffer), "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s/repo", testPath()));
        strLstAddZ(argList, "--" CFGOPT_IGNORE_MISSING);
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(bufNew(0))), 1, "get");

        // -------------------------------------------------------------------------------------------------------------------------
        // Reset env
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        // Reset buffer size
        ioBufferSizeSet(oldBufferSize);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStorageRemove()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdRepoRm, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove missing path");

        TEST_RESULT_VOID(cmdStorageRemove(), "remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove empty path");

        strLstAddZ(argList, "path");
        harnessCfgLoad(cfgCmdRepoRm, argList);

        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite(), STRDEF("path")), "add path");
        TEST_RESULT_VOID(cmdStorageRemove(), "remove path");
        TEST_RESULT_BOOL(storagePathExistsP(storageRepo(), STRDEF("path")), false, "    check path removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail when path is not empty and no recurse");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(), strNew("path/aaa.txt")), BUFSTRDEF("TESTDATA")), "add path/file");
        TEST_ERROR(cmdStorageRemove(), OptionInvalidError, "recurse option must be used to delete non-empty path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when path is not empty and no recurse");

        cfgOptionSet(cfgOptRecurse, cfgSourceParam, BOOL_TRUE_VAR);
        TEST_RESULT_VOID(cmdStorageRemove(), "remove path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on more than one path");

        strLstAddZ(argList, "repo2");
        harnessCfgLoad(cfgCmdRepoRm, argList);

        TEST_ERROR(cmdStorageRemove(), ParamInvalidError, "only one path may be specified");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove file");

        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo", testPath()));
        strLstAddZ(argList, "path/aaa.txt");
        harnessCfgLoad(cfgCmdRepoRm, argList);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(), strNew("path/aaa.txt")), BUFSTRDEF("TESTDATA")), "add path/file");
        TEST_RESULT_VOID(cmdStorageRemove(), "remove file");
        TEST_RESULT_BOOL(storagePathExistsP(storageRepo(), STRDEF("path/aaa.txt")), false, "    check file removed");
        TEST_RESULT_BOOL(storagePathExistsP(storageRepo(), STRDEF("path")), true, "    check path exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
