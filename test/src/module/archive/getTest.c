/***********************************************************************************************************************************
Test Archive Get Command
***********************************************************************************************************************************/
#include <sys/wait.h>

#include "common/harnessConfig.h"
#include "postgres/type.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    Storage *storageTest = storageNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("queueNeed()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        size_t queueSize = WAL_SEGMENT_DEFAULT_SIZE;
        size_t walSegmentSize = WAL_SEGMENT_DEFAULT_SIZE;

        TEST_ERROR_FMT(
            queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            PathOpenError, "unable to open path '%s/spool/archive/db/in' for read: [2] No such file or directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN));

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000100000001|000000010000000100000002", "queue size smaller than min");

        // -------------------------------------------------------------------------------------------------------------------------
        queueSize = WAL_SEGMENT_DEFAULT_SIZE * 3;

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000100000001|000000010000000100000002|000000010000000100000003", "empty queue");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(
                storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE")), bufNew(walSegmentSize));
        storagePutNP(
            storageNewWriteNP(
                storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF")), bufNew(walSegmentSize));

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("0000000100000001000000FE"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000200000000|000000010000000200000001", "queue has wal < 9.3");

        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), "|")),
            "0000000100000001000000FE", "check queue");

        // -------------------------------------------------------------------------------------------------------------------------
        walSegmentSize = 1024 * 1024;
        queueSize = walSegmentSize * 5;

        storagePutNP(storageNewWriteNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/junk")), bufNew(16));
        storagePutNP(
            storageNewWriteNP(
                storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFE")), bufNew(walSegmentSize));
        storagePutNP(
            storageNewWriteNP(
                storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF")), bufNew(walSegmentSize));

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000A00000FFD"), true, queueSize, walSegmentSize, PG_VERSION_11), "|")),
            "000000010000000B00000000|000000010000000B00000001|000000010000000B00000002", "queue has wal >= 9.3");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), sortOrderAsc), "|")),
            "000000010000000A00000FFE|000000010000000A00000FFF", "check queue");

        storagePathRemoveP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN), .recurse = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGet()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--log-level-file=debug"));
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "WAL segment to get required");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTemp = strLstDup(argList);
        String *walSegment = strNew("000000010000000100000001");
        strLstAdd(argListTemp, walSegment);
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "Path to copy WAL segment required");

        // -------------------------------------------------------------------------------------------------------------------------
        String *controlFile = strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);
        PgControlFile control = {.systemId = 0xFACEFACE, .controlVersion = 1002, .catalogVersion = 201707211};
        storagePutNP(storageNewWriteNP(storageTest, controlFile), bufNewC(sizeof(PgControlFile), &control));

        storagePathCreateNP(storageTest, strNewFmt("%s/db/pg_wal", testPath()));

        String *walFile = strNewFmt("%s/db/pg_wal/RECOVERYXLOG", testPath());
        strLstAdd(argListTemp, walFile);
        strLstAdd(argListTemp, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        int processId = fork();

        // Test this in a fork so we can use different Perl options in later tests
        if (processId == 0)
        {
            TEST_ERROR(cmdArchiveGet(), FileMissingError, "!!!EMBEDDEDPERLERROR!!!");
            exit(0);
        }
        else
        {
            int processStatus;

            if (waitpid(processId, &processStatus, 0) != processId)                         // {uncoverable - fork() does not fail}
                THROW_SYS_ERROR(AssertError, "unable to find child process");               // {uncoverable+}

            if (WEXITSTATUS(processStatus) != 0)
                THROW_FMT(AssertError, "perl exited with error %d", WEXITSTATUS(processStatus));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAdd(argListTemp, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAddZ(argListTemp, "00000001.history");
        strLstAdd(argListTemp, walFile);
        strLstAddZ(argListTemp, "--archive-async");
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));
        processId = fork();

        // Test this in a fork so we can use different Perl options in later tests
        if (processId == 0)
        {
            TEST_ERROR(cmdArchiveGet(), FileMissingError, "!!!EMBEDDEDPERLERROR!!!");
            exit(0);
        }
        else
        {
            int processStatus;

            if (waitpid(processId, &processStatus, 0) != processId)                         // {uncoverable - fork() does not fail}
                THROW_SYS_ERROR(AssertError, "unable to find child process");               // {uncoverable+}

            if (WEXITSTATUS(processStatus) != 0)
                THROW_FMT(AssertError, "perl exited with error %d", WEXITSTATUS(processStatus));
        }

        // Make sure the process times out when there is nothing to get
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, walSegment);
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout getting WAL segment");

        // Write out a bogus .error file to make sure it is ignored on the first loop
        // -------------------------------------------------------------------------------------------------------------------------
        // String *errorFile = storagePathNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.error"));
        // storagePutNP(storageNewWriteNP(storageSpool(), errorFile), bufNewStr(strNew("25\n" BOGUS_STR)));
        //
        // TEST_ERROR(cmdArchiveGet(), AssertError, BOGUS_STR);
        // unlink(strPtr(errorFile));
        //
        // // Wait for the lock to be released
        // lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true);
        // lockRelease(true);

        // Check for missing WAL
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strPtr(walSegment))), NULL);

        TEST_RESULT_VOID(cmdArchiveGet(), "successful get of missing WAL");
        testLogResult("P00   INFO: unable to find WAL segment 000000010000000100000001");

        TEST_RESULT_BOOL(
            storageExistsNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strPtr(walSegment))), false,
            "check OK file was removed");

        // Wait for the lock to be released
        lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true);
        lockRelease(true);

        // Write out a WAL segment for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment))),
            bufNewStr(strNew("SHOULD-BE-A-REAL-WAL-FILE")));

        TEST_RESULT_VOID(cmdArchiveGet(), "successful get");
        testLogResult("P00   INFO: got WAL segment 000000010000000100000001 asynchronously");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, walFile), true, "check WAL segment was moved");
        storageRemoveP(storageTest, walFile, .errorOnMissing = true);

        // Wait for the lock to be released
        lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true);
        lockRelease(true);

        // Write more WAL segments (in this case queue should be full)
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--archive-get-queue-max=48");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        String *walSegment2 = strNew("000000010000000100000002");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment))),
            bufNewStr(strNew("SHOULD-BE-A-REAL-WAL-FILE")));
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment2))),
            bufNewStr(strNew("SHOULD-BE-A-REAL-WAL-FILE")));

        TEST_RESULT_VOID(cmdArchiveGet(), "successful get");
        testLogResult("P00   INFO: got WAL segment 000000010000000100000001 asynchronously");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, walFile), true, "check WAL segment was moved");

        // Wait for the lock to be released
        lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true);
        lockRelease(true);

        // Make sure the process times out when it can't get a lock
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true), "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout waiting for lock");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "extra parameters found");
    }
}
