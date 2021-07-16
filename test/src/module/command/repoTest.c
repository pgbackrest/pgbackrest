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

    // *****************************************************************************************************************************
    if (testBegin("cmdRepoCreate()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdRepoCreate, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("noop on non-S3 storage");

        TEST_RESULT_VOID(cmdRepoCreate(), "repo create");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStorageList() and storageListRender()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptOutput, "text");
        hrnCfgArgRawZ(argList, cfgOptSort, "none");
        HRN_CFG_LOAD(cfgCmdRepoLs, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing directory");

        Buffer *output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "", "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}\n",
            "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("empty directory");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), NULL, .mode = 0700);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "", "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}\n",
            "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("\\."));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory with filter match (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"path\"}"
                "}\n",
            "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("2$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory with no filter match (json)");
        TEST_RESULT_STR_Z(strNewBuf(output), "{}\n", "check output");

        cfgOptionSet(cfgOptFilter, cfgSourceParam, NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add path and file");

        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("asc"));

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), "bbb");
        HRN_STORAGE_PUT_Z(storageRepoWrite(), "aaa", "TESTDATA", .timeModified = 1578671569);
        HRN_STORAGE_PUT_Z(storageRepoWrite(), "bbb/ccc", "TESTDATA2");

        HRN_SYSTEM("ln -s ../bbb " TEST_PATH "/repo/link");
        HRN_SYSTEM("mkfifo " TEST_PATH "/repo/pipe");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa\nbbb\nlink\npipe\n", "check output");

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
            "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reverse sort");

        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("desc"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb\naaa\n", "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recurse");

        cfgOptionSet(cfgOptRecurse, cfgSourceParam, VARBOOL(true));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "pipe\nlink\nbbb/ccc\nbbb\naaa\n", "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter");

        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("^aaa$"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR_Z(strNewBuf(output), "aaa\n", "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("subdirectory");

        StringList *argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "bbb");
        HRN_CFG_LOAD(cfgCmdRepoLs, argListTmp);

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "subdirectory");
        TEST_RESULT_STR_Z(strNewBuf(output), "ccc\n", "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("redirect stdout to a file");

        int stdoutSave = dup(STDOUT_FILENO);

        THROW_ON_SYS_ERROR(freopen(TEST_PATH "/stdout.txt", "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdStorageList();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_STORAGE_GET(storagePosixNewP(TEST_PATH_STR), "stdout.txt", "ccc\n", .comment = "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("too many paths");

        strLstAddZ(argListTmp, "ccc");
        HRN_CFG_LOAD(cfgCmdRepoLs, argListTmp);

        TEST_ERROR(storageListRender(ioBufferWriteNew(output)), ParamInvalidError, "only one path may be specified");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file info - json");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo/aaa");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgRawZ(argList, cfgOptOutput, "json");
        HRN_CFG_LOAD(cfgCmdRepoLs, argList);

        output = bufNew(0);
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"file\",\"size\":8,\"time\":1578671569}"
                "}\n",
            "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("\\/aaa$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(
            strNewBuf(output),
                "{"
                    "\".\":{\"type\":\"file\",\"size\":8,\"time\":1578671569}"
                "}\n",
            "check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("bbb$"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "file (json)");
        TEST_RESULT_STR_Z(strNewBuf(output), "{}\n", "check output");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStoragePut() and cmdStorageGet()"))
    {
        // Set buffer size small so copy loops get exercised
        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(8);

        // Needed for tests
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 1, TEST_CIPHER_PASS);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);

        // Test files and buffers
        const String *fileName = STRDEF("file.txt");
        const Buffer *fileBuffer = BUFSTRDEF("TESTFILE");

        const String *fileEncCustomName = STRDEF("file-enc-custom.txt");
        const Buffer *fileEncCustomBuffer = BUFSTRDEF("TESTFILE-ENC-CUSTOM");

        const String *fileRawName = STRDEF("file-raw.txt");
        const char *fileRawContent = "TESTFILE-RAW";

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
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_ERROR(storagePutProcess(ioBufferReadNew(fileBuffer)), ParamRequiredError, "destination file required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a file");

        strLstAdd(argList, fileName);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put an encrypted file with custom key");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAdd(argList, fileEncCustomName);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(fileEncCustomBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put a raw file");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawBool(argList, cfgOptRaw, true);
        strLstAdd(argList, fileRawName);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        // Get stdin from a file
        int stdinSave = dup(STDIN_FILENO);
        const String *stdinFile = storagePathP(storageRepo(), STRDEF("stdin.txt"));
        HRN_STORAGE_PUT_Z(storageRepoWrite(), strZ(stdinFile), fileRawContent);

        THROW_ON_SYS_ERROR(freopen(strZ(stdinFile), "r", stdin) == NULL, FileWriteError, "unable to reopen stdin");

        TEST_RESULT_VOID(cmdStoragePut(), "put");

        // Restore normal stdout
        dup2(stdinSave, STDIN_FILENO);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted archive.info");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted archive.info.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.info");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.info.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupInfoFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted WAL archive file");

        // Create WAL segment
        ioBufferSizeSet(oldBufferSize);
        Buffer *archiveFileBuffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(archiveFileBuffer), 0, bufSize(archiveFileBuffer));
        bufUsedSet(archiveFileBuffer, bufSize(archiveFileBuffer));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/12-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(archiveFileBuffer)), "put");

        // Reset small buffer size again for the next tests
        ioBufferSizeSet(8);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.manifest");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE);
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.manifest.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup.history manifest");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/backup.history/2020/label.manifest.gz");
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(manifestFileBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put encrypted backup_label");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom2");
        strLstAdd(argList, STRDEF(STORAGE_PATH_BACKUP "/test/latest/pg_data/backup_label"));
        HRN_CFG_LOAD(cfgCmdRepoPut, argList);

        TEST_RESULT_VOID(storagePutProcess(ioBufferReadNew(backupLabelBuffer)), "put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when missing source");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        TEST_ERROR(storageGetProcess(ioBufferWriteNew(bufNew(0))), ParamRequiredError, "source file required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a file");

        strLstAdd(argList, fileName);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        Buffer *writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a file with / repo");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/");
        strLstAdd(argList, strNewFmt(TEST_PATH "/repo/%s", strZ(fileName)));
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get an encrypted file with custom key");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList, cfgOptCipherPass, "custom");
        strLstAdd(argList, fileEncCustomName);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, fileEncCustomBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get a raw file");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawBool(argList, cfgOptRaw, true);
        strLstAdd(argList, fileRawName);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        TEST_LOG("get");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);

        THROW_ON_SYS_ERROR(freopen(TEST_PATH "/repo/stdout.txt", "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        ASSERT(cmdStorageGet() == 0);

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_STORAGE_GET(storageRepo(), "stdout.txt", fileRawContent, .comment = "get matches put");


        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing file");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawBool(argList, cfgOptIgnoreMissing, true);
        strLstAddZ(argList, BOGUS_STR);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(bufNew(0))), 1, "get");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get file outside of the repo error");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, "/somewhere/" INFO_ARCHIVE_FILE);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(
            storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            "absolute path '/somewhere/" INFO_ARCHIVE_FILE "' is not in base path '" TEST_PATH "/repo'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get file in repo root directory error");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(argList, fileEncCustomName);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(
            storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            strZ(strNewFmt("unable to determine cipher passphrase for '%s'", strZ(fileEncCustomName))));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info - stanza mismatch");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test2");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_ERROR(
            storageGetProcess(ioBufferWriteNew(writeBuffer)), OptionInvalidValueError,
            "stanza name 'test2' given in option doesn't match the given path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveInfoFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted archive.info.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_ARCHIVE "/test/" INFO_ARCHIVE_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveInfoFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.info");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupInfoFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.info.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/" INFO_BACKUP_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupInfoFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted WAL file");

        // Set higher buffer size for the WAL archive tests
        ioBufferSizeSet(oldBufferSize);

        TEST_TITLE("get encrypted WAL archive file");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(
            argList,
            strNewFmt(
                "%s/repo/" STORAGE_PATH_ARCHIVE "/test/12-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                TEST_PATH));
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, archiveFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.manifest");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE);
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.manifest.copy");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/latest/" BACKUP_MANIFEST_FILE ".copy");
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup.history manifest");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAddZ(argList, STORAGE_PATH_BACKUP "/test/backup.history/2020/label.manifest.gz");
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, manifestFileBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get encrypted backup_label");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        strLstAdd(argList, STRDEF(STORAGE_PATH_BACKUP "/test/latest/pg_data/backup_label"));
        HRN_CFG_LOAD(cfgCmdRepoGet, argList);

        writeBuffer = bufNew(0);
        TEST_RESULT_INT(storageGetProcess(ioBufferWriteNew(writeBuffer)), 0, "get");
        TEST_RESULT_BOOL(bufEq(writeBuffer, backupLabelBuffer), true, "get matches put");

        // -------------------------------------------------------------------------------------------------------------------------
        // Reset env
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 1);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        // Reset buffer size
        ioBufferSizeSet(oldBufferSize);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStorageRemove()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdRepoRm, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove missing path");

        TEST_RESULT_VOID(cmdStorageRemove(), "remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove empty path");

        strLstAddZ(argList, "path");
        HRN_CFG_LOAD(cfgCmdRepoRm, argList);

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), "path", .comment = "add path");
        TEST_RESULT_VOID(cmdStorageRemove(), "remove path");
        TEST_STORAGE_LIST_EMPTY(storageRepo(), NULL, .comment = "check path removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail when path is not empty and no recurse");

        HRN_STORAGE_PUT_Z(storageRepoWrite(), "path/aaa.txt", "TESTDATA", .comment = "add path/file");
        TEST_ERROR(cmdStorageRemove(), OptionInvalidError, "recurse option must be used to delete non-empty path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when path is not empty and no recurse");

        cfgOptionSet(cfgOptRecurse, cfgSourceParam, BOOL_TRUE_VAR);
        TEST_RESULT_VOID(cmdStorageRemove(), "remove path");
        TEST_STORAGE_LIST_EMPTY(storageRepo(), NULL, .comment = "check path removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on more than one path");

        strLstAddZ(argList, "repo2");
        HRN_CFG_LOAD(cfgCmdRepoRm, argList);

        TEST_ERROR(cmdStorageRemove(), ParamInvalidError, "only one path may be specified");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove file");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        strLstAddZ(argList, "path/aaa.txt");
        HRN_CFG_LOAD(cfgCmdRepoRm, argList);

        HRN_STORAGE_PUT_Z(storageRepoWrite(), "path/aaa.txt", "TESTDATA", .comment = "add path/file");
        TEST_RESULT_VOID(cmdStorageRemove(), "remove file");
        TEST_STORAGE_LIST(storageRepo(), NULL, "path/\n", .comment = "check path exists and file removed");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
