/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/time.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // Start a protocol server to test the protocol directly
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(
        strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("archivePushReadyList(), archivePushProcessList(), and archivePushDrop()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argList);

        storagePathCreateP(storagePgWrite(), strNew("pg_wal/archive_status"));
        storagePathCreateP(storageTest, strNew("spool/archive/db/out"));

        // Create ok files to indicate WAL that has already been archived
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok")), NULL);
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000003.ok")), NULL);
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000004.ok")), NULL);
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000005.error")), NULL);
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000006.error")), NULL);
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/global.error")), NULL);

        // Create ready files for wal that still needs to be archived
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000002.ready")), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000003.ready")), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000005.ready")), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000006.ready")), NULL);

        TEST_RESULT_STR_Z(
            strLstJoin(archivePushProcessList(strNewFmt("%s/db/pg_wal", testPath())), "|"),
            "000000010000000100000002|000000010000000100000005|000000010000000100000006", "ready list");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT)), sortOrderAsc), "|"),
            "000000010000000100000003.ok", "remaining status list");

        // Test drop
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListDrop = strLstDup(argList);
        strLstAdd(argListDrop, strNewFmt("--archive-push-queue-max=%zu", (size_t)1024 * 1024 * 1024));
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argListDrop);

        // Write the files that we claim are in pg_wal
        Buffer *walBuffer = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}, walBuffer);

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000002")), walBuffer);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000003")), walBuffer);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000005")), walBuffer);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000006")), walBuffer);

        // Queue max is high enough that no WAL will be dropped
        TEST_RESULT_BOOL(
            archivePushDrop(strNew("pg_wal"), archivePushProcessList(strNewFmt("%s/db/pg_wal", testPath()))), false,
            "wal is not dropped");

        // Now set queue max low enough that WAL will be dropped
        argListDrop = strLstDup(argList);
        strLstAdd(argListDrop, strNewFmt("--archive-push-queue-max=%zu", (size_t)16 * 1024 * 1024 * 2));
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argListDrop);

        TEST_RESULT_BOOL(
            archivePushDrop(strNew("pg_wal"), archivePushProcessList(strNewFmt("%s/db/pg_wal", testPath()))), true,
            "wal is dropped");
    }

    // *****************************************************************************************************************************
    if (testBegin("archivePushCheck()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdArchivePush, argList);

        // Check mismatched pg_control and archive.info
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageTest, strNew("pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 0xFACEFACEFACEFACE}));

        // Create incorrect archive info
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"));

        TEST_ERROR(
            archivePushCheck(true, cipherTypeNone, NULL), ArchiveMismatchError,
            "PostgreSQL version 9.6, system-id 18072658121562454734 do not match stanza version 9.4, system-id 5555555555555555555"
                "\nHINT: are you archiving to the correct stanza?");

        // Fix the version
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.6\"}\n"));

        TEST_ERROR(
            archivePushCheck(true, cipherTypeNone, NULL), ArchiveMismatchError,
            "PostgreSQL version 9.6, system-id 18072658121562454734 do not match stanza version 9.6, system-id 5555555555555555555"
                "\nHINT: are you archiving to the correct stanza?");

        // Fix archive info
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n"));

        ArchivePushCheckResult result = {0};
        TEST_ASSIGN(result, archivePushCheck(true, cipherTypeNone, NULL), "get archive check result");

        TEST_RESULT_UINT(result.pgVersion, PG_VERSION_96, "check pg version");
        TEST_RESULT_UINT(result.pgSystemId, 0xFACEFACEFACEFACE, "check pg system id");
        TEST_RESULT_STR_Z(result.archiveId, "9.6-1", "check archive id");
        TEST_RESULT_STR_Z(result.archiveCipherPass, NULL, "check archive cipher pass (not set in this test)");
    }

    // *****************************************************************************************************************************
    if (testBegin("Synchronous cmdArchivePush(), archivePushFile() and archivePushProtocol()"))
    {
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleDefault, argList);

        TEST_ERROR(cmdArchivePush(), HostInvalidError, "archive-push command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test");
        harnessCfgLoad(cfgCmdArchivePush, argList);

        TEST_ERROR(cmdArchivePush(), ParamRequiredError, "WAL segment to push required");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        TEST_ERROR(
            cmdArchivePush(), OptionRequiredError,
            "option 'pg1-path' must be specified when relative wal paths are used"
            "\nHINT: is %f passed to archive-push instead of %p?"
            "\nHINT: PostgreSQL may pass relative paths even with %p depending on the environment.");

        // Create pg_control and archive.info
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        storagePutP(
            storageNewWriteP(storageTest, strNew("pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}));

        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}\n"));

        // Generate WAL with incorrect headers and try to push them
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *walBuffer1 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer1, bufSize(walBuffer1));
        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}, walBuffer1);

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000001")), walBuffer1);

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_ERROR(
            cmdArchivePush(), ArchiveMismatchError,
            strZ(
                strNewFmt(
                    "WAL file '%s/pg/pg_wal/000000010000000100000001' version 10, system-id 18072658121562454734 do not match"
                        " stanza version 11, system-id 18072658121562454734",
                    testPath())));

        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xECAFECAFECAFECAF}, walBuffer1);

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000001")), walBuffer1);

        TEST_ERROR(
            cmdArchivePush(), ArchiveMismatchError,
            strZ(
                strNewFmt(
                    "WAL file '%s/pg/pg_wal/000000010000000100000001' version 11, system-id 17055110554209741999 do not match"
                        " stanza version 11, system-id 18072658121562454734",
                    testPath())));

        // Generate valid WAL and push them
        // -------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}, walBuffer1);

        // Check sha1 checksum against fixed values once to make sure they are not getting munged. After this we'll calculate them
        // directly from the buffers to reduce the cost of maintaining checksums.
        const char *walBuffer1Sha1 = TEST_64BIT() ?
            (TEST_BIG_ENDIAN() ? "1c5f963d720bb199d7935dbd315447ea2ec3feb2" : "aae7591a1dbc58f21d0d004886075094f622e6dd") :
            "28a13fd8cf6fcd9f9a8108aed4c8bcc58040863a";

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000001")), walBuffer1);

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        harnessLogResult("P00   INFO: pushed WAL file '000000010000000100000001' to the archive");

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000001-%s.gz", walBuffer1Sha1)),
            true, "check repo for WAL file");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment again");
        harnessLogResult(
            "P00   WARN: WAL file '000000010000000100000001' already exists in the archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000001' to the archive");

        // Now create a new WAL buffer with a different checksum to test checksum errors
        Buffer *walBuffer2 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer2, bufSize(walBuffer2));
        memset(bufPtr(walBuffer2), 0xFF, bufSize(walBuffer2));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}, walBuffer2);
        const char *walBuffer2Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer2)));

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/000000010000000100000001")), walBuffer2);

        TEST_ERROR(cmdArchivePush(), ArchiveDuplicateError, "WAL file '000000010000000100000001' already exists in the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL with absolute path and no pg1-path");

        argListTemp = strLstNew();
        strLstAddZ(argListTemp, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawFmt(argListTemp, cfgOptRepoPath, "%s/repo", testPath());
        strLstAdd(argListTemp, strNewFmt("%s/pg/pg_wal/000000010000000100000002", testPath()));
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNew("pg/pg_wal/000000010000000100000002")), walBuffer2), "write WAL");

        // Create tmp file to make it look like a prior push failed partway through to ensure that retries work
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s.gz.pgbackrest.tmp",
                    walBuffer2Sha1)),
                BUFSTRDEF("PARTIAL")),
            "write WAL tmp file");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        harnessLogResult("P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s.gz", walBuffer2Sha1)),
            true, "check repo for WAL file");
        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest,
                strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s.gz.pgbackrest.tmp", walBuffer2Sha1)),
            false, "check WAL tmp file is gone");

        // Push a history file
        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/00000001.history");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/00000001.history")), BUFSTRDEF("FAKEHISTORY"));

        TEST_RESULT_VOID(cmdArchivePush(), "push a history file");
        harnessLogResult("P00   INFO: pushed WAL file '00000001.history' to the archive");

        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNew("repo/archive/test/11-1/00000001.history")), true, "check repo for history file");

        // Check drop functionality
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000001.ready")), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_wal/archive_status/000000010000000100000002.ready")), NULL);

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--archive-push-queue-max=16m");
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(cmdArchivePush(), "drop WAL file");
        harnessLogResult("P00   WARN: dropped WAL file '000000010000000100000002' because archive queue exceeded 16MB");

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--archive-push-queue-max=1GB");
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(cmdArchivePush(), "push WAL file again");
        harnessLogResult(
            "P00   WARN: WAL file '000000010000000100000002' already exists in the archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(strNewFmt("%s/pg/pg_wal/000000010000000100000002", testPath())));
        varLstAdd(paramList, varNewStrZ("11-1"));
        varLstAdd(paramList, varNewUInt64(PG_VERSION_11));
        varLstAdd(paramList, varNewUInt64(0xFACEFACEFACEFACE));
        varLstAdd(paramList, varNewStrZ("000000010000000100000002"));
        varLstAdd(paramList, varNewUInt64(cipherTypeNone));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewInt(6));

        TEST_RESULT_BOOL(
            archivePushProtocol(PROTOCOL_COMMAND_ARCHIVE_PUSH_STR, paramList, server), true, "protocol archive put");
        TEST_RESULT_STR_Z(
            strNewBuf(serverWrite),
            "{\"out\":\"WAL file '000000010000000100000002' already exists in the archive with the same checksum"
                "\\nHINT: this is valid in some recovery scenarios but may also indicate a problem.\"}\n",
            "check result");

        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(archivePushProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");

        // Create a new encrypted repo to test encryption
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathRemoveP(storageTest, strNew("repo"), .errorOnMissing = true, .recurse = true);

        StorageWrite *infoWrite = storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info"));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(infoWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("badpassphrase"), NULL));

        storagePutP(
            infoWrite,
            harnessInfoChecksumZ(
                "[cipher]\n"
                "cipher-pass=\"badsubpassphrase\"\n"
                "\n"
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}"));

        // Push encrypted WAL segment
        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        strLstAddZ(argListTemp, "--repo1-cipher-type=aes-256-cbc");
        strLstAddZ(argListTemp, "--no-compress");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "badpassphrase", true);
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        harnessLogResult("P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            true, "check repo for WAL file");
    }

    // *****************************************************************************************************************************
    if (testBegin("Asynchronous cmdArchivePush() and cmdArchivePushAsync()"))
    {
        harnessLogLevelSet(logLevelDetail);

        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        strLstAddZ(argList, "--" CFGOPT_SPOOL_PATH "=/spool");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchivePush(), HostInvalidError, "archive-push command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1-path must be set when async");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_SPOOL_PATH "=/spool");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "/000000010000000100000001");
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchivePush(), OptionRequiredError, "'archive-push' command in async mode requires option 'pg1-path'");

        // Call with a bogus exe name so the async process will error out and we can make sure timeouts work
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest-bogus");
        strLstAddZ(argList, "--stanza=test");
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAdd(argList, strNewFmt("--lock-path=%s/lock", testPath()));
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "pg_wal/bogus");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        storagePathCreateP(storageTest, cfgOptionStr(cfgOptPgPath));
        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL file 'bogus' to the archive asynchronously after 1 second(s)");

        // Create pg_control and archive.info
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test");
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--no-compress");
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--log-subprocess");

        storagePutP(
            storageNewWriteP(storageTest, strNew("pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}));

        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":12297848147757817309,\"db-version\":\"9.4\"}\n"));

        // Write out an error file that will be ignored on the first pass, then the async process will write a new one
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTemp = strLstDup(argList);
        strLstAdd(argListTemp, strNewFmt("%s/pg/pg_xlog/000000010000000100000001", testPath()));
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        storagePathCreateP(storagePgWrite(), strNew("pg_xlog/archive_status"));

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error")),
            BUFSTRDEF("25\nbogus error"));

        TEST_ERROR(cmdArchivePush(), AssertError, "no WAL files to process");

        storageRemoveP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/global.error"), .errorOnMissing = true);

        // Acquire a lock so the async process will not be able to run -- this will result in a timeout
        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAdd(argListTemp, strNewFmt("%s/pg/pg_xlog/000000010000000100000001", testPath()));
        strLstAddZ(argListTemp, "--archive-timeout=1");
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(strNew("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("child write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30000, true);

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew(""));
                ioWriteFlush(write);
                ioReadLine(read);

                lockRelease(true);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(strNew("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_ERROR(
                    cmdArchivePush(), ArchiveTimeoutError,
                    "unable to push WAL file '000000010000000100000001' to the archive asynchronously after 1 second(s)");

                // Notify the child to release the lock
                ioWriteLine(write, bufNew(0));
                ioWriteFlush(write);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // Actually push a WAL file
        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAdd(argListTemp, strNewFmt("%s/pg/pg_xlog/000000010000000100000001", testPath()));
        harnessCfgLoad(cfgCmdArchivePush, argListTemp);

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_xlog/archive_status/000000010000000100000001.ready")), NULL);

        Buffer *walBuffer1 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer1, bufSize(walBuffer1));
        memset(bufPtr(walBuffer1), 0xFF, bufSize(walBuffer1));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}, walBuffer1);
        const char *walBuffer1Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer1)));

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_xlog/000000010000000100000001")), walBuffer1);

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        harnessLogResult("P00   INFO: pushed WAL file '000000010000000100000001' to the archive asynchronously");

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            true, "check repo for WAL file");

        // Direct tests of the async function
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test");
        strLstAddZ(argList, "--no-compress");
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--log-subprocess");
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchivePushAsync(), ParamRequiredError, "WAL path to push required");

        // Check that global.error is created
        // -------------------------------------------------------------------------------------------------------------------------
        // Remove data from prior tests
        storagePathRemoveP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR, .recurse = true);
        storagePathCreateP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR);

        storagePathRemoveP(storageRepoWrite(), strNew(STORAGE_REPO_ARCHIVE "/9.4-1"), .recurse = true);
        storagePathCreateP(storageRepoWrite(), strNew(STORAGE_REPO_ARCHIVE "/9.4-1"));

        storagePathRemoveP(storagePgWrite(), strNew("pg_xlog/archive_status"), .recurse = true);
        storagePathCreateP(storagePgWrite(), strNew("pg_xlog/archive_status"));

        strLstAdd(argList, strNewFmt("%s/pg/pg_xlog", testPath()));
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchivePushAsync(), AssertError, "no WAL files to process");

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/global.error")))),
            "25\nno WAL files to process", "check global.error");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT)), sortOrderAsc), "|"),
            "global.error", "check status files");

        // Push WAL
        // -------------------------------------------------------------------------------------------------------------------------
        // Recreate ready file for WAL 1
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_xlog/archive_status/000000010000000100000001.ready")), NULL);

        // Create a ready file for WAL 2 but don't create the segment yet -- this will test the file error
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_xlog/archive_status/000000010000000100000002.ready")), NULL);

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        harnessLogResult(
            strZ(
                strNewFmt(
                    "P00   INFO: push 2 WAL file(s) to archive: 000000010000000100000001...000000010000000100000002\n"
                    "P01 DETAIL: pushed WAL file '000000010000000100000001' to the archive\n"
                    "P01   WARN: could not push WAL file '000000010000000100000002' to the archive (will be retried): "
                        "[55] raised from local-1 protocol: " STORAGE_ERROR_READ_MISSING,
                    strZ(strNewFmt("%s/pg/pg_xlog/000000010000000100000002", testPath())))));

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            true, "check repo for WAL 1 file");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT)), sortOrderAsc), "|"),
            "000000010000000100000001.ok|000000010000000100000002.error", "check status files");

        // Create WAL 2 segment
        Buffer *walBuffer2 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer2, bufSize(walBuffer2));
        memset(bufPtr(walBuffer2), 0x0C, bufSize(walBuffer2));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}, walBuffer2);
        const char *walBuffer2Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer2)));

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("pg_xlog/000000010000000100000002")), walBuffer2);

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--archive-push-queue-max=1gb");
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argListTemp);

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        harnessLogResult(
            "P00   INFO: push 1 WAL file(s) to archive: 000000010000000100000002\n"
            "P01 DETAIL: pushed WAL file '000000010000000100000002' to the archive");

        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest, strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            true, "check repo for WAL 2 file");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT)), sortOrderAsc), "|"),
            "000000010000000100000001.ok|000000010000000100000002.ok", "check status files");

        // Check that drop functionality works
        // -------------------------------------------------------------------------------------------------------------------------
        // Remove status files
        storagePathRemoveP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR, .recurse = true);
        storagePathCreateP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR);

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--archive-push-queue-max=16m");
        harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleAsync, argListTemp);

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        harnessLogResult(
            "P00   INFO: push 2 WAL file(s) to archive: 000000010000000100000001...000000010000000100000002\n"
            "P00   WARN: dropped WAL file '000000010000000100000001' because archive queue exceeded 16MB\n"
            "P00   WARN: dropped WAL file '000000010000000100000002' because archive queue exceeded 16MB");

        TEST_RESULT_STR_Z(
            strNewBuf(
                storageGetP(storageNewReadP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok")))),
            "0\ndropped WAL file '000000010000000100000001' because archive queue exceeded 16MB", "check WAL 1 warning");

        TEST_RESULT_STR_Z(
            strNewBuf(
                storageGetP(storageNewReadP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000002.ok")))),
            "0\ndropped WAL file '000000010000000100000002' because archive queue exceeded 16MB", "check WAL 2 warning");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT)), sortOrderAsc), "|"),
            "000000010000000100000001.ok|000000010000000100000002.ok", "check status files");
        }

    FUNCTION_HARNESS_RESULT_VOID();
}
