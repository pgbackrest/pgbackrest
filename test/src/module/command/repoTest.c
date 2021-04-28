/***********************************************************************************************************************************
Test Repo Commands
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

#include "info/infoArchive.h"
#include "info/infoBackup.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

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
                "}\n",
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
                "}\n",
            "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("\\."));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory with filter match (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}\n",
            "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("2$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory with no filter match (json)");
        TEST_RESULT_STR_Z(strNewBuf(output), "{}\n", "    check output");

        cfgOptionSet(cfgOptFilter, cfgSourceParam, NULL);

        // Add path and file
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("asc"));

        storagePathCreateP(storageTest, strNew("repo/bbb"));
        storagePutP(storageNewWriteP(storageTest, strNew("repo/aaa"), .timeModified = 1578671569), BUFSTRDEF("TESTDATA"));
        storagePutP(storageNewWriteP(storageTest, strNew("repo/bbb/ccc")), BUFSTRDEF("TESTDATA2"));

        ASSERT(system(strZ(strNewFmt("ln -s ../bbb %s/repo/link", testPath()))) == 0);
        ASSERT(system(strZ(strNewFmt("mkfifo %s/repo/pipe", testPath()))) == 0);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa\nbbb\nlink\npipe\n", "    check output");

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
                "}\n",
            "    check output");

        // Reverse sort
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("desc"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb\naaa\n", "    check output");

        // Recurse
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptRecurse, cfgSourceParam, VARBOOL(true));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb/ccc\nbbb\naaa\n", "    check output");

        // Filter
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("^aaa$"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa\n", "    check output");

        // Subdirectory
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "bbb");
        harnessCfgLoad(cfgCmdRepoLs, argListTmp);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "subdirectory");
        TEST_RESULT_STR_Z(strNewBuf(output), "ccc\n", "    check output");

        // -------------------------------------------------------------------------------------------------------------------------
        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.txt", testPath());

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdStorageList();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(storageTest, stdoutFile))), "ccc\n", "    check text");

        // Too many paths
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argListTmp, "ccc");
        harnessCfgLoad(cfgCmdRepoLs, argListTmp);

        TEST_ERROR(storageListRender(ioBufferWriteNew(output)), ParamInvalidError, "only one path may be specified");

        // File
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH_REPO "/aaa");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        strLstAddZ(argList, "--output=json");
        harnessCfgLoad(cfgCmdRepoLs, argList);

        output = bufNew(0);
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"file\",\"size\":8,\"time\":1578671569}"
                "}\n",
            "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("\\/aaa$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"file\",\"size\":8,\"time\":1578671569}"
                "}\n",
            "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("bbb$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(strNewBuf(output), "{}\n", "    check output");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStoragePut() and cmdStorageGet()"))
    {
        // Set buffer size small so copy loops get exercised
        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(8);

        // Needed for tests
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "xxx", true);
        setenv("PGBACKREST_REPO2_CIPHER_PASS", "xxx", true);

        // Test files and buffers
        const String *fileName = STRDEF("file.txt");
        const Buffer *fileBuffer = BUFSTRDEF("TESTFILE");

        const String *fileEncCustomName = STRDEF("file-enc-custom.txt");
        const Buffer *fileEncCustomBuffer = BUFSTRDEF("TESTFILE-ENC-CUSTOM");

        const String *fileRawName = STRDEF("file-raw.txt");
        const Buffer *fileRawBuffer = BUFSTRDEF("TESTFILE-RAW");

        const Buffer *archiveInfoFileBuffer = BUFSTRDEF(
            "[cipher]\n"
            "cipher-pass=\"custom\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6846378200844646865\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6846378200844646865,\"db-version\":\"12\"}\n"
            "\n"
            "[backrest]\n"
            "backrest-checksum=\"85c2460341ddc2af8bdc0e07437965e3f1f64ea2\"\n");

        const Buffer *backupInfoFileBuffer = BUFSTRDEF(
            "[cipher]\n"
            "cipher-pass=\"custom\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201909212\n"
            "db-control-version=1201\n"
            "db-id=1\n"
            "db-system-id=6846378200844646865\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201909212,\"db-control-version\":1201,\"db-system-id\":6846378200844646865,\"db-version\":\"12\"}\n"
            "\n"
            "[backrest]\n"
            "backrest-checksum=\"089f3f0c862691ff083cd5db66d0cea3f5830b37\"\n");

        const Buffer *manifestFileBuffer = BUFSTRDEF(
            "[cipher]\n"
            "cipher-pass=\"custom2\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201909212\n"
            "db-control-version=1201\n"
            "db-id=1\n"
            "db-system-id=6846378200844646865\n"
            "db-version=\"12\"\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/var/lib/pgsql/12/data\",\"type\":\"path\"}\n"
            "\n"
            "[backrest]\n"
            "backrest-checksum=\"31706010d1aa7e850191b4de9e76dc1ed13fb855\"\n");

        const Buffer *backupLabelBuffer = BUFSTRDEF("BACKUP-LABEL");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when missing destination");

        StringList *argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_ERROR(storagePutProcess(ioBufferReadNew(fileBuffer)), ParamRequiredError, "destination file required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a file");

        strLstAdd(argList, fileName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put an encrypted file with custom key");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAdd(argList, fileEncCustomName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileEncCustomBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a raw file");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--raw");
        strLstAdd(argList, fileRawName);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        // Get stdin from a file
        int stdinSave = dup(STDIN_FILENO);
        const String *stdinFile = storagePathP(storageRepo(), STRDEF("stdin.txt"));
        storagePutP(storageNewWriteP(storageRepoWrite(), stdinFile), fileRawBuffer);

        THROW_ON_SYS_ERROR(freopen(strZ(stdinFile), "r", stdin) == NULL, FileWriteError, "unable to reopen stdin");

        TEST_RESULT_VOID(cmdStoragePut(), "put");

        // Restore normal stdout
        dup2(stdinSave, STDIN_FILENO);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted archive.info");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted archive.info.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.info");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.info.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted WAL archive file");

        // Create WAL segment
        ioBufferSizeSet(oldBufferSize);
        Buffer *archiveFileBuffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(archiveFileBuffer), 0, bufSize(archiveFileBuffer));
        bufUsedSet(archiveFileBuffer, bufSize(archiveFileBuffer));

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAdd(argList, strNew(STORAGE_PATH_ARCHIVE "/test/12-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveFileBuffer)), "put");

        // Reset small buffer size again for the next tests
        ioBufferSizeSet(8);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.manifest");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE);
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.manifest.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.history manifest");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH_REPO);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/backup.history/2020/label.manifest.gz");
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup_label");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom2");
        strLstAdd(argList, strNew(STORAGE_PATH_BACKUP "/test/latest/pg_data/backup_label"));
        harnessCfgLoad(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupLabelBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when missing source");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
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
        TEST_TITLE("get a file with / repo");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/");
        strLstAdd(argList, strNewFmt("%s/repo/%s", testPath(), strZ(fileName)));
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get an encrypted file with custom key");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--" CFGOPT_CIPHER_PASS "=custom");
        strLstAdd(argList, fileEncCustomName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileEncCustomBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a raw file");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "--raw");
        strLstAdd(argList, fileRawName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        TEST_LOG("get");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/repo/stdout.txt", testPath());

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        ASSERT(cmdStorageGet() == 0);

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_RESULT_STR(
            strNewBuf(storageGetP(storageNewReadP(storageRepo(), stdoutFile))), strNewBuf(fileRawBuffer), "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing file");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        strLstAddZ(argList, "--" CFGOPT_IGNORE_MISSING);
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(bufNew(0))), 1, "get");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get file outside of the repo error");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH_REPO);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "/somewhere/" INFO_ARCHIVE_FILE);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(
            storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            strZ(strNewFmt("absolute path '/somewhere/%s' is not in base path '%s/repo'", INFO_ARCHIVE_FILE, testPath())));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get file in repo root directory error");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(argList, fileEncCustomName);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(
            storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            strZ(strNewFmt("unable to determine cipher passphrase for '%s'", strZ(fileEncCustomName))));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info - stanza mismatch");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            "stanza name 'test2' given in option doesn't match the given path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveInfoFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveInfoFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.info");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupInfoFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.info.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupInfoFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        // Set higher buffer size for the WAL archive tests
        ioBufferSizeSet(oldBufferSize);

        TEST_TITLE("get encrypted WAL archive file");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(argList, strNewFmt(
            "%s/repo/" STORAGE_PATH_ARCHIVE "/test/12-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",testPath()));
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.manifest");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE);
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.manifest.copy");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE ".copy");
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.history manifest");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/backup.history/2020/label.manifest.gz");
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "    get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup_label");

        argList = strLstNew();
        hrnCfgArgRawFmt(argList, cfgOptRepoPath, "%s/repo", testPath());
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(argList, strNew(STORAGE_PATH_BACKUP "/test/latest/pg_data/backup_label"));
        harnessCfgLoad(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupLabelBuffer), true, "    get matches put");

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
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH_REPO);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        strLstAddZ(argList, "path/aaa.txt");
        harnessCfgLoad(cfgCmdRepoRm, argList);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(), strNew("path/aaa.txt")), BUFSTRDEF("TESTDATA")), "add path/file");
        TEST_RESULT_VOID(cmdStorageRemove(), "remove file");
        TEST_RESULT_BOOL(storagePathExistsP(storageRepo(), STRDEF("path/aaa.txt")), false, "    check file removed");
        TEST_RESULT_BOOL(storagePathExistsP(storageRepo(), STRDEF("path")), true, "    check path exists");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
