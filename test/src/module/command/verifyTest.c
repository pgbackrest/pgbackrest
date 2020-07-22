/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include <stdio.h> // CSHANG remove

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "common/io/bufferRead.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *stanza = strNew("db");
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strPtr(stanza));
    String *backupInfoFileName = strNewFmt("%s/" INFO_BACKUP_FILE, strPtr(backupStanzaPath));
    String *backupInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strPtr(stanza));
    String *archiveInfoFileName = strNewFmt("%s/" INFO_ARCHIVE_FILE, strPtr(archiveStanzaPath));
    String *archiveInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName));

    StringList *argListBase = strLstNew();
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    String *backupInfoContent = strNewFmt(
        "[backup:current]\n"
        "20181119-152138F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.28dev\","
        "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "\n"
        "[db]\n"
        "db-catalog-version=201409291\n"
        "db-control-version=942\n"
        "db-id=1\n"
        "db-system-id=6625592122879095702\n"
        "db-version=\"9.4\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
            "\"db-version\":\"9.4\"}");

    const Buffer *backupInfoBase = harnessInfoChecksumZ(strPtr(backupInfoContent));

    String *backupInfoMultiHistoryContent = strNewFmt(
        "[backup:current]\n"
        "20181119-152138F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152900F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "\n"
        "[db]\n"
        "db-catalog-version=201707211\n"
        "db-control-version=1100\n"
        "db-id=2\n"
        "db-system-id=6626363367545678089\n"
        "db-version=\"11\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
            "\"db-version\":\"9.4\"}\n"
        "2={\"db-catalog-version\":201707211,\"db-control-version\":1100,\"db-system-id\":6626363367545678089,"
            "\"db-version\":\"11\"}");

    const Buffer *backupInfoMultiHistoryBase = harnessInfoChecksumZ(strPtr(backupInfoMultiHistoryContent));

    String *archiveInfoContent = strNewFmt(
        "[db]\n"
        "db-id=1\n"
        "db-system-id=6625592122879095702\n"
        "db-version=\"9.4\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}");

    const Buffer *archiveInfoBase = harnessInfoChecksumZ(strPtr(archiveInfoContent));

    String *archiveInfoMultiHistoryContent = strNewFmt(
        "[db]\n"
        "db-id=2\n"
        "db-system-id=6626363367545678089\n"
        "db-version=\"11\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
        "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}");

    const Buffer *archiveInfoMultiHistoryBase = harnessInfoChecksumZ(strPtr(archiveInfoMultiHistoryContent));

    // String *archiveInfoMultiHistoryContent = strNewFmt(
    //     "[db]\n"
    //     "db-id=2\n"
    //     "db-system-id=6626363367545678089\n"
    //     "db-version=\"10\"\n"
    //     "\n"
    //     "[db:history]\n"
    //     "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
    //     "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

// CSHANG Tests - parens for logging, e.g. (ERROR) means LOG_ERROR and continue:
//
// 1) Backup.info:
//     - only backup.info exists (WARN) and is OK
//     - only backup.info exists (WARN) and is NOT OK (ERROR):
//         - checksum in file (corrupt it) does not match generated checksum
//     - only backup.info.copy exists (WARN) and is OK
//     - both info & copy exist and are valid but don't match each other (in this case are we always reading the main file? If so, then we must check the main file in the code against the archive.info file)
// 2) Local and remote tests
// 3) Should probably have 1 test that in with encryption? Like a run through with one failure and then all success? Can't set encryption password on command line so can't just pass encryptions type and password as options...


// typedef struct ArchiveRange
// {
//     List *archiveRangePgList;
// } ArchiveRange;
//
// typedef struct ArchiveRangePg
// {
//     unsigned int pgId;
//     List *walRangeList;
// } ArchiveRangePg;
//
// typedef struct WalRange
// {
//     String *stop;
//     String *start;
// } WalRange;

// void
// walGen(unsigned int pgId, ArchiveRange *testArchiveRange)
// {
//     ArchiveRangePg pgArchiveRange =
//     {
//         .pgId = pgId,
//         .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
//     };
//
//     if (pgId == 1)
//     {
//         WalRange walRange1 =
//         {
//             .stop = strNew("00000001000000000000000E-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("00000001000000000000000B-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange1);
//
//         WalRange walRange2 =
//         {
//             .stop = strNew("000000010000000000000009-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("000000010000000000000001-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange2);
//     }
//     else
//     {
//         WalRange walRange3 =
//         {
//             .stop = strNew("000000020000000000000010-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("00000002000000000000000E-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange3);
//
//         WalRange walRange4 =
//         {
//             .stop = strNew("00000002000000000000000A-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("000000020000000000000001-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange4);
//     }
//
//     lstSort(pgArchiveRange.walRangeList, sortOrderAsc);
//
//     lstAdd(testArchiveRange->archiveRangePgList, &pgArchiveRange);
// }

/*


// So can test that the next file is the distance of one WAL segment size away
        TEST_RESULT_UINT((pgLsnFromWalSegment(STRDEF("0000000100000000000000FF"), 0x1000000) - pgLsnFromWalSegment(STRDEF("0000000100000000000000FE"), 0x1000000)), 0x1000000, "test distance");

// 0000000100000000000000FF
        TEST_RESULT_STR_Z(pgLsnToWalSegment(1, (uint64_t)0x00000000FF000000  + (16 * 1024 * 1024), 0x1000000), "000000010000000100000000", "test");
        TEST_RESULT_STR_Z(pgLsnToWalSegment(1, pgLsnFromWalSegment(STRDEF("0000000100000000000000FF"), 0x1000000) + (16 * 1024 * 1024), 0x1000000), "000000010000000100000000", "test");


        // THROW_FMT(AssertError, "%016" PRIX64, pgLsnFromWalSegment(STRDEF("0000000100000000000000FF"), 0x1000000));
*/

//
// ArchiveRange myTest =
// {
//     .archiveRangePgList = lstNew(sizeof(ArchiveRangePg)),
// };
//     ArchiveRangePg pgArchiveRange =
//     {
//         .pgId = 1,
//         .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
//     };
//
//
//         WalRange walRange1 =
//         {
//             .stop = strNew("00000001000000000000000E-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("00000001000000000000000B-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange1);
//
//         WalRange walRange2 =
//         {
//             .stop = strNew("000000010000000000000009-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("000000010000000000000001-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange.walRangeList, &walRange2);
//         lstSort(pgArchiveRange.walRangeList, sortOrderAsc);
//     lstAdd(myTest.archiveRangePgList, &pgArchiveRange);
//
//     ArchiveRangePg pgArchiveRange2 =
//     {
//         .pgId = 2,
//         .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
//     };
//
//         WalRange walRange3 =
//         {
//             .stop = strNew("000000020000000000000010-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("00000002000000000000000E-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange2.walRangeList, &walRange3);
//
//         WalRange walRange4 =
//         {
//             .stop = strNew("00000002000000000000000A-9baedd24b61aa15305732ac678c4e2c102435a09"),
//             .start = strNew("000000020000000000000001-99999924b61aa15305732ac678c4e2c102435a09"),
//         };
//         lstAdd(pgArchiveRange2.walRangeList, &walRange4);
//         lstSort(pgArchiveRange2.walRangeList, sortOrderAsc);
//     lstAdd(myTest.archiveRangePgList, &pgArchiveRange2);


// walGen(1, &myTest);
// walGen(2, &myTest);
// lstSort(myTest.archiveRangePgList, sortOrderAsc);
//
// String *testFile = strNew("00000001000000000000000D-9baedd24b61aa15305732ac678c4e2c102435a09");
//
// for (unsigned int i = 0; i < lstSize(myTest.archiveRangePgList); i++)
// {
//     ArchiveRangePg *myArchive = lstGet(myTest.archiveRangePgList, i);
//     printf("i: %u, pgId=%u\n", i, myArchive->pgId); fflush(stdout);
//
//     for (unsigned int j = 0; j < lstSize(myArchive->walRangeList); j++)
//     {
//         WalRange *myWal = lstGet(myArchive->walRangeList, j);
//         printf("    stop: %s\n", strPtr(myWal->stop)); fflush(stdout);
//         printf("    start: %s\n", strPtr(myWal->start)); fflush(stdout);
//         // if the file
//         if (myArchive->pgId == 1 && strCmp(testFile, myWal->stop) <= 0)
//         {
//             printf("testfile <= stop %s\n", strPtr(myWal->stop)); fflush(stdout);
//             /*
//             wait - this may not help since the files can come in any order so all we know is that the range is no longer valid
//             maybe we just mark the range as invalid and add the file to the "bad" list?
//             What is we have a list of all the bad files and a list of all the good files and try to do an intersection? No that won't work because it is a range and not a comprehensive list so can't get an intersection.
//             maybe have another list in the WalRange structure so stop, start and list of invalid? Then when looking at the manifest, we see if the start and stop fall within any of the ranges and if the range is marked invalid, we look and see if it is in the list of invalid files?
// Range: 01 -> 09
// invalid lstSize > 0 and has 03 and 06
// backup start/stop 01, 04
// since invalid has something then if invalid >= backup start or <= backup stop then the backup is invalid but if not, then how to determine if PITR is not valid?
// e.g. if the invalid list only contained 06 then the backup would be consistent but then what makes us sure PITR is not?
//
//             */
//         }
//
//     }
// }
/*
16 * 1024 * 1024 = PG_WAL_SEGMENT_SIZE_DEFAULT
1024 * 1024 * 1024 = 1GB wal segment

        TEST_RESULT_STR_Z(
            strLstJoin(
                pgLsnRangeToWalSegmentList(
                    PG_VERSION_92, 2, pgLsnFromStr(STRDEF("1/FD000000")), pgLsnFromStr(STRDEF("2/1000000")), 16 * 1024 * 1024),
                ", "),
            "0000000200000001000000FD, 0000000200000001000000FE, 000000020000000200000000, 000000020000000200000001",
            "get range <= 9.2");
        TEST_RESULT_STR_Z(
            strLstJoin(
                pgLsnRangeToWalSegmentList(
                    PG_VERSION_93, 2, pgLsnFromStr(STRDEF("1/FD000000")), pgLsnFromStr(STRDEF("2/60")), 16 * 1024 * 1024),
                ", "),
            "0000000200000001000000FD, 0000000200000001000000FE, 0000000200000001000000FF, 000000020000000200000000",
            "get range > 9.2");
        TEST_RESULT_STR_Z(
            strLstJoin(
                pgLsnRangeToWalSegmentList(
                    PG_VERSION_11, 2, pgLsnFromStr(STRDEF("A/800")), pgLsnFromStr(STRDEF("B/C0000000")), 1024 * 1024 * 1024),
                ", "),
            "000000020000000A00000000, 000000020000000A00000001, 000000020000000A00000002, 000000020000000A00000003"
                ", 000000020000000B00000000, 000000020000000B00000001, 000000020000000B00000002, 000000020000000B00000003",
            "get range >= 11/1GB");
*/
    // *****************************************************************************************************************************
    if (testBegin("updateArchiveIdRange()"))
    {
        ArchiveIdRange archiveIdRange =
        {
            .archiveId = strNew("10-1"),
            .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
        };
        List *archiveIdRangeList = lstNewP(sizeof(ArchiveIdRange), .comparator =  archiveIdComparator);

        archiveIdRange.pgWalInfo.size = PG_WAL_SEGMENT_SIZE_DEFAULT;
        archiveIdRange.pgWalInfo.version = PG_VERSION_94;

        StringList *walFileList = strLstNew();
        strLstAddZ(walFileList, "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz");
        strLstAddZ(walFileList, "0000000200000001000000FF-daa497dba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "000000020000000200000000");

        TEST_RESULT_VOID(
            updateArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList),
            "update archiveId WAL range");

        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 1, "single range");

        WalRange *walRangeResult = NULL;

        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        // Clear the range lists and create a gap in the WAL list
        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);
        strLstRemoveIdx(walFileList, 2);

        TEST_RESULT_STR_Z(
            strLstJoin(walFileList, ", "),
            "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz, "
            "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz, 000000020000000200000000",
            "gap in WAL list");

        TEST_RESULT_VOID(
            updateArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList),
            "update archiveId WAL range");

        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 2, "multiple range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0),
            "get first range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "0000000200000001000000FE", "stop range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        // Clear the range lists and rerun the test with PG_VERSION_92 to ensure the missing FF is not considered a gap
        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);

        archiveIdRange.pgWalInfo.version = PG_VERSION_92;

        TEST_RESULT_STR_Z(
            strLstJoin(walFileList, ", "),
            "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz, "
            "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz, 000000020000000200000000",
            "WAL missing FF file");

        TEST_RESULT_VOID(
            updateArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList),
            "update archiveId WAL range for version 9.2");

        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 1, "singe range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
    }


    // *****************************************************************************************************************************
    if (testBegin("verifyPgHistory()"))
    {
        // Create backup.info
        InfoBackup *backupInfo = NULL;
        TEST_ASSIGN(backupInfo, infoBackupNewLoad(ioBufferReadNew(backupInfoMultiHistoryBase)), "backup.info multi-history");

        // Create archive.info - history mismatch
        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"))), "archive.info missing history");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095777,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"))), "archive.info history system id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.5\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"))), "archive.info history version mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");


        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "3={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"))), "archive.info history id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("neither backup nor archive info files exist");

        TEST_ERROR(cmdVerify(), RuntimeError, "fatal errors encountered, see log for details");
        harnessLogResult(
            strPtr(strNewFmt(
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), testPath(), testPath(), testPath())));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, neither backup copy nor archive infos exist");

        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.28\"\n");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, backupInfoFileName), contentLoad), "write invalid backup.info");
        TEST_ERROR(cmdVerify(), RuntimeError, "fatal errors encountered, see log for details");
        harnessLogResult(
            strPtr(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
            "<REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), testPath(), testPath())));


        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, backup.info.copy valid, archive.info not exist, archive copy checksum invalid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), contentLoad), "write invalid archive.info.copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoBase), "write valid backup.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "fatal errors encountered, see log for details");
        harnessLogResult(
            strPtr(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
            " <REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
            " <REPO:ARCHIVE>/archive.info.copy\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath())));


        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid but checksum mismatch, archive.info checksum invalid, archive.info copy valid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoMultiHistoryBase), "write valid backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), contentLoad), "write invalid archive.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoBase), "write valid archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "fatal errors encountered, see log for details");
        harnessLogResult(
            "P00   WARN: backup.info.copy doesn't match backup.info\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
            " <REPO:ARCHIVE>/archive.info\n"
            "P00  ERROR: [029]: backup info file and archive info file do not match\n"
            "            archive: id = 1, version = 9.4, system-id = 6625592122879095702\n"
            "            backup : id = 2, version = 11, system-id = 6626363367545678089\n"
            "            HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid and checksums match, archive.info and copy valid, but checksum mismatch");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoMultiHistoryBase),
            "write valid backup.info.copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), archiveInfoMultiHistoryBase),
            "write valid archive.info");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        harnessLogResult(
            "P00   WARN: archive.info.copy doesn't match archive.info\n"
            "P00   WARN: no archives or backups exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info valid, copy invalid, archive.info valid, copy invalid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileNameCopy), "remove backup.info.copy");
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileNameCopy), "remove archive.info.copy");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        harnessLogResult(
            strPtr(strNewFmt(
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info.copy' for read\n"
            "P00   WARN: no archives or backups exist in the repo", testPath(), testPath())));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy missing, archive.info and copy valid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName), "remove backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoMultiHistoryBase),
            "write valid and matching archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "fatal errors encountered, see log for details");
        harnessLogResult(
            strPtr(strNewFmt(
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file", testPath(), testPath())));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, WAL files present, no backups");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
                harnessInfoChecksumZ(
                    "[db]\n"
                    "db-catalog-version=201707211\n"
                    "db-control-version=1100\n"
                    "db-id=2\n"
                    "db-system-id=6626363367545678089\n"
                    "db-version=\"11\"\n"
                    "\n"
                    "[db:history]\n"
                    "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                        "\"db-version\":\"9.4\"}\n"
                    "2={\"db-catalog-version\":201707211,\"db-control-version\":1100,\"db-system-id\":6626363367545678089,"
                        "\"db-version\":\"11\"}")),
            "put backup.info files");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        pgWalTestToBuffer(
            (PgWal){.version = PG_VERSION_11, .systemId = 6626363367545678089, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strPtr(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/11-2/0000000100000000", strPtr(archiveStanzaPath))),
            "create empty timeline path");
// CSHANG Need WAL that fails and a gap
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt(
                        "%s/11-2/0000000200000000/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4",
                        strPtr(archiveStanzaPath))),
                walBuffer),
            "write WAL for checksum failure");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000700000FFE-%s", strPtr(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000700000FFF-%s", strPtr(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000800000000-%s", strPtr(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000800000002-%s", strPtr(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next range");

       TEST_RESULT_VOID(cmdVerify(), "TEST");

    }

    FUNCTION_HARNESS_RESULT_VOID();
}
