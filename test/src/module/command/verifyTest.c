/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
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
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strZ(stanza));
    String *backupInfoFileName = strNewFmt("%s/" INFO_BACKUP_FILE, strZ(backupStanzaPath));
    String *backupInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strZ(stanza));
    String *archiveInfoFileName = strNewFmt("%s/" INFO_ARCHIVE_FILE, strZ(archiveStanzaPath));
    String *archiveInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName));

    StringList *argListBase = strLstNew();
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strZ(stanza)));
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

    const Buffer *backupInfoBase = harnessInfoChecksumZ(strZ(backupInfoContent));

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

    const Buffer *backupInfoMultiHistoryBase = harnessInfoChecksumZ(strZ(backupInfoMultiHistoryContent));

    String *archiveInfoContent = strNewFmt(
        "[db]\n"
        "db-id=1\n"
        "db-system-id=6625592122879095702\n"
        "db-version=\"9.4\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}");

    const Buffer *archiveInfoBase = harnessInfoChecksumZ(strZ(archiveInfoContent));

    String *archiveInfoMultiHistoryContent = strNewFmt(
        "[db]\n"
        "db-id=2\n"
        "db-system-id=6626363367545678089\n"
        "db-version=\"11\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
        "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}");

    const Buffer *archiveInfoMultiHistoryBase = harnessInfoChecksumZ(strZ(archiveInfoMultiHistoryContent));

    // *****************************************************************************************************************************
    if (testBegin("verifyCreateArchiveIdRange()"))
    {
        VerifyWalRange *walRangeResult = NULL;
        unsigned int errTotal = 0;
        StringList *walFileList = strLstNew();

        VerifyArchiveResult archiveResult =
        {
            .archiveId = strNew("9.4-1"),
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator =  lstComparatorStr),
        };
        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator =  archiveIdComparator);
        lstAdd(archiveIdResultList, &archiveResult);
        VerifyArchiveResult *archiveIdResult = lstGetLast(archiveIdResultList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Single WAL");

        archiveIdResult->pgWalInfo.size = PG_WAL_SEGMENT_SIZE_DEFAULT;
        archiveIdResult->pgWalInfo.version = PG_VERSION_94;

        strLstAddZ(walFileList, "000000020000000200000000-daa497dba64008db824607940609ba1cd7c6c501.gz");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Duplicate WAL only - no range, all removed from list");

        lstClear(archiveIdResult->walRangeList);

        // Add a duplicate
        strLstAddZ(walFileList, "000000020000000200000000");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 1, "duplicate WAL error");
        TEST_RESULT_UINT(strLstSize(walFileList), 0, "all WAL removed from WAL file list");
        TEST_RESULT_UINT(lstSize(archiveIdResult->walRangeList), 0, "no range");
        harnessLogResult("P00  ERROR: [028]: duplicate WAL '000000020000000200000000' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal not skipped > 9.2, duplicates at beginning and end of list are removed");

        errTotal = 0;
        strLstAddZ(walFileList, "000000020000000100000000-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "000000020000000100000000");
        strLstAddZ(walFileList, "000000020000000100000000-aaaaaadba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz");
        strLstAddZ(walFileList, "0000000200000001000000FF-daa497dba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "000000020000000200000000");
        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000001");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 2, "triplicate WAL error at beginning, duplicate WAL at end");
        TEST_RESULT_UINT(strLstSize(walFileList), 4, "only duplicate WAL removed from WAL list");
        TEST_RESULT_UINT(lstSize(archiveIdResultList), 1, "single archiveId result");
        TEST_RESULT_UINT(lstSize(archiveIdResult->walRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        harnessLogResult(
            "P00  ERROR: [028]: duplicate WAL '000000020000000100000000' for '9.4-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal skipped <= 9.2, duplicates in middle of list removed");

        // Clear the range lists and rerun the test with PG_VERSION_92 to ensure FF is reported as an error
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;
        archiveIdResult->archiveId = strNew("9.2-1");
        archiveIdResult->pgWalInfo.version = PG_VERSION_92;

        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000002");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 2, "error reported");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        harnessLogResult(
            "P00  ERROR: [028]: invalid WAL '0000000200000001000000FF' for '9.2-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.2-1' exists, skipping");

        TEST_RESULT_STRLST_Z(
            walFileList,
            "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz\n"
            "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz\n"
            "000000020000000200000000\n000000020000000200000002\n",
            "skipped files removed");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Rerun <= 9.2, missing FF not a gap");

        // Clear the range lists, rerun the PG_VERSION_92 test to ensure the missing FF is not considered a gap
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "error reported");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version > 9.2, missing FF is a gap");

        // Clear the range lists and update the version > 9.2 so missing FF is considered a gap in the WAL ranges
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;
        archiveIdResult->archiveId = strNew("9.6-1");
        archiveIdResult->pgWalInfo.version = PG_VERSION_96;

        strLstAddZ(walFileList, "000000020000000200000003-123456");
        strLstAddZ(walFileList, "000000020000000200000004-123456");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 3, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get first range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "0000000200000001000000FE", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 2),
            "get third range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000004", "stop range");
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
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info missing history");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095777,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history system id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.5\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history version mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");


        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "3={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifySetBackupCheckArchive(), verifyLogInvalidResult(), verifyRender()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifySetBackupCheckArchive()");

        InfoBackup *backupInfo = NULL;
        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(backupInfo, infoBackupNewLoad(ioBufferReadNew(backupInfoMultiHistoryBase)), "backup.info");
        TEST_ASSIGN(archiveInfo, infoArchiveNewLoad(ioBufferReadNew(archiveInfoMultiHistoryBase)), "archive.info");
        InfoPg *pgHistory = infoArchivePg(archiveInfo);

        StringList *backupList= strLstNew();
        strLstAddZ(backupList, "20181119-152138F");
        strLstAddZ(backupList, "20181119-152900F");
        StringList *archiveIdList = strLstComparatorSet(strLstNew(), archiveIdComparator);
        strLstAddZ(archiveIdList, "9.4-1");
        strLstAddZ(archiveIdList, "11-2");

        unsigned int errTotal = 0;

        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(
                strLstNew(), backupInfo, strLstNew(), pgHistory, &errTotal), NULL, "no archives or backups");
        TEST_RESULT_UINT(errTotal, 0, "no error");
        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(
                backupList, backupInfo, archiveIdList, pgHistory, &errTotal), NULL, "no current backup, no missing archive");
        TEST_RESULT_UINT(errTotal, 0, "no error");

        // Add backup to end of list
        strLstAddZ(backupList, "20181119-153000F");
        strLstAddZ(archiveIdList, "12-3");

        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "current backup, missing archive");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        harnessLogResult("P00  ERROR: [044]: archiveIds '12-3' are not in the archive.info history list");

        errTotal = 0;
        strLstAddZ(archiveIdList, "13-4");
        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "test multiple archiveIds on disk not in archive.info");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        harnessLogResult("P00  ERROR: [044]: archiveIds '12-3, 13-4' are not in the archive.info history list");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyLogInvalidResult() - missing file");

        TEST_RESULT_UINT(verifyLogInvalidResult(verifyFileMissing, 0, strNew("missingfilename")), 0, "file missing message");
        harnessLogResult("P00   WARN: file missing 'missingfilename'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyRender() - missing file, empty invalidList");

        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator =  archiveIdComparator);
        VerifyArchiveResult archiveIdResult =
        {
            .archiveId = strNew("9.6-1"),
            .totalWalFile = 1,
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator =  lstComparatorStr),
        };
        VerifyWalRange walRange =
        {
            .start = strNew("0"),
            .stop = strNew("2"),
            .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator =  lstComparatorStr),
        };

        lstAdd(archiveIdResult.walRangeList, &walRange);
        lstAdd(archiveIdResultList, &archiveIdResult);
        TEST_RESULT_STR_Z(
            verifyRender(archiveIdResultList),
            "Results:\n"
            "  archiveId: 9.6-1, total WAL checked: 1, total valid WAL: 0\n"
            "    missing: 0, checksum invalid: 0, size invalid: 0, other: 0", "no invalid file list");

        VerifyInvalidFile invalidFile =
        {
            .fileName = strNew("file"),
            .reason = verifyFileMissing,
        };

        lstAdd(walRange.invalidFileList, &invalidFile);
        TEST_RESULT_STR_Z(
            verifyRender(archiveIdResultList),
            "Results:\n"
            "  archiveId: 9.6-1, total WAL checked: 1, total valid WAL: 0\n"
            "    missing: 1, checksum invalid: 0, size invalid: 0, other: 0", "file missing");

        // Coverage test
        TEST_RESULT_VOID(
            verifyAddInvalidWalFile(archiveIdResult.walRangeList, verifyFileMissing, strNew("test"), strNew("3")), "coverage test");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify() - info files"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("neither backup nor archive info files exist");

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(backupStanzaPath), testPath(), strZ(archiveStanzaPath), testPath(), strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, neither backup copy nor archive infos exist");

        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.28\"\n");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, backupInfoFileName), contentLoad), "write invalid backup.info");
        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(archiveStanzaPath), testPath(), strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, backup.info.copy valid, archive.info not exist, archive copy checksum invalid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), contentLoad), "write invalid archive.info.copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoBase), "write valid backup.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:ARCHIVE>/archive.info.copy\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), strZ(archiveStanzaPath))));


        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid but checksum mismatch, archive.info checksum invalid, archive.info copy valid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoMultiHistoryBase), "write valid backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), contentLoad), "write invalid archive.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoBase), "write valid archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            "P00   WARN: backup.info.copy does not match backup.info\n"
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
            "P00   WARN: archive.info.copy does not match archive.info\n"
            "P00   WARN: no archives or backups exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info valid, copy invalid, archive.info valid, copy invalid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileNameCopy), "remove backup.info.copy");
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileNameCopy), "remove archive.info.copy");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info.copy' for read\n"
            "P00   WARN: no archives or backups exist in the repo", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy missing, archive.info and copy valid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName), "remove backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoMultiHistoryBase),
            "write valid and matching archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(backupStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info.copy valid, archive.info and copy valid, present but empty backup");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoMultiHistoryBase),
            "write valid backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoMultiHistoryBase),
            "write valid backup.info.copy");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/20200810-171426F", strZ(backupStanzaPath))),
            "create empty backup label path");
        TEST_RESULT_VOID(cmdVerify(), "no archives, empty backup label path");
        harnessLogResult("P00   WARN: no archives exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info.copy valid, archive.info and copy valid, present but empty backup, empty archive");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/9.4-1", strZ(archiveStanzaPath))),
            "create empty path for archiveId");

        TEST_RESULT_VOID(cmdVerify(), "no jobs - empty archive id and backup label paths");
        harnessLogResult(
            "P00   WARN: archive path '9.4-1' is empty\n"
            "P00   INFO: Results:\n"
            "              archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyFile(), verifyProtocol()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyFile()");

        String *filePathName =  strNewFmt(STORAGE_REPO_ARCHIVE "/testfile");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRDEF("")), "put zero-sized file");
        TEST_RESULT_UINT(verifyFile(filePathName, STRDEF(HASH_TYPE_SHA1_ZERO), 0, NULL), verifyOk, "file ok");

        const char *fileContents = "acefile";
        uint64_t fileSize = 7;
        const String *checksum = STRDEF("d1cd8a7d11daa26814b93eb604e1d49ab4b43770");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRZ(fileContents)), "put file");

        TEST_RESULT_UINT(verifyFile(filePathName, checksum, fileSize, NULL), verifyOk, "file size ok");
        TEST_RESULT_UINT(verifyFile(filePathName, checksum, 0, NULL), verifySizeInvalid, "file size invalid");
        TEST_RESULT_UINT(
            verifyFile(filePathName, strNew("badchecksum"), fileSize, NULL), verifyChecksumMismatch, "file checksum mismatch");
        TEST_RESULT_UINT(
            verifyFile(
                strNewFmt(STORAGE_REPO_ARCHIVE "/missingFile"), checksum, 0, NULL), verifyFileMissing, "file missing");

        // Create a compressed encrypted repo file
        filePathName = strNew(STORAGE_REPO_BACKUP "/testfile.gz");
        StorageWrite *write = storageNewWriteP(storageRepoWrite(), filePathName);
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(write));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("pass"), NULL));
        TEST_RESULT_VOID(storagePutP(write, BUFSTRZ(fileContents)), "write encrypted, compressed file");

        TEST_RESULT_UINT(
            verifyFile(filePathName, checksum, fileSize, strNew("pass")), verifyOk, "file encrypted compressed ok");
        TEST_RESULT_UINT(
            verifyFile(
                filePathName, strNew("badchecksum"), fileSize, strNew("pass")), verifyChecksumMismatch,
                "file encrypted compressed checksum mismatch");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyProtocol()");

        // Start a protocol server to test the protocol directly
        Buffer *serverWrite = bufNew(8192);
        IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
        ioWriteOpen(serverWriteIo);
        ProtocolServer *server = protocolServerNew(strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);
        bufUsedSet(serverWrite, 0);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(filePathName));
        varLstAdd(paramList, varNewStr(checksum));
        varLstAdd(paramList, varNewUInt64(fileSize));
        varLstAdd(paramList, varNewStrZ("pass"));

        TEST_RESULT_BOOL(verifyProtocol(PROTOCOL_COMMAND_VERIFY_FILE_STR, paramList, server), true, "protocol verify file");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":0}\n", "check result");
        bufUsedSet(serverWrite, 0);

        TEST_RESULT_BOOL(verifyProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid protocol function");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify(), verifyProcess()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        // Store valid archive/backup info files
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), archiveInfoMultiHistoryBase),
            "write valid archive.info");
        storageCopy(storageNewReadP(storageTest, archiveInfoFileName), storageNewWriteP(storageTest, archiveInfoFileNameCopy));

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
        storageCopy(storageNewReadP(storageTest, backupInfoFileName), storageNewWriteP(storageTest, backupInfoFileNameCopy));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, WAL files present, no backups");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        pgWalTestToBuffer(
            (PgWal){.version = PG_VERSION_11, .systemId = 6626363367545678089, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write valid WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
                    strZ(archiveStanzaPath))),
                walBuffer),
            "write duplicate WAL");

        // Set log detail level to capture ranges (there should be none)
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: no backups exist in the repo\n"
                "P00  ERROR: [028]: duplicate WAL '000000020000000700000FFE' for '11-2' exists, skipping\n"
                "P00   WARN: path '11-2/0000000200000007' does not contain any valid WAL to be processed\n"
                "P00   INFO: Results:\n"
                "              archiveId: 11-2, total WAL checked: 2, total valid WAL: 0\n"
                "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0")));

        harnessLogLevelReset();

        TEST_RESULT_VOID(
            storageRemoveP(
                storageTest, strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
                strZ(archiveStanzaPath))),
            "remove duplicate WAL");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/9.4-1", strZ(archiveStanzaPath))),
            "create empty path for old archiveId");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/11-2/0000000100000000", strZ(archiveStanzaPath))),
            "create empty timeline path");

        StorageWrite *write = storageNewWriteP(
            storageTest,
            strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz",
            strZ(archiveStanzaPath)));
        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), compressFilter(compressTypeGz, 3));
        TEST_RESULT_VOID(storagePutP(write, walBuffer), "write first WAL compressed - but checksum failure");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFF-%s", strZ(archiveStanzaPath),
                    strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, BUFSTRDEF("invalidsize")))))),
                BUFSTRDEF("invalidsize")),
            "write WAL - invalid size");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000000-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - continue range");

        // Set log detail level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        // Test verifyProcess directly
        unsigned int errorTotal = 0;
        TEST_RESULT_STR_Z(
            verifyProcess(&errorTotal),
            "Results:\n"
            "  archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0\n"
            "  archiveId: 11-2, total WAL checked: 4, total valid WAL: 2\n"
            "    missing: 0, checksum invalid: 1, size invalid: 1, other: 0",
            "verifyProcess() results");
        TEST_RESULT_UINT(errorTotal, 2, "errors");
        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: no backups exist in the repo\n"
                "P00   WARN: archive path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000")));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, start next timeline");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000002-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next range");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000300000000/000000030000000000000000-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next timeline");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000300000000/000000030000000000000001-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - end next timeline");

        // Set log level to errors only
        harnessLogLevelSet(logLevelError);

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'")));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, unreadable WAL file");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9",
                        strZ(archiveStanzaPath)),
                .modeFile = 0200),
                walBuffer),
            "write WAL - file not readable");

        // Set log level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "3 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: no backups exist in the repo\n"
                "P00   WARN: archive path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
                "P01  ERROR: [039]: invalid verify "
                    "11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9: [41] raised from "
                    "local-1 protocol: unable to open file "
                    "'%s/%s/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9' for read: "
                    "[13] Permission denied\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000800000002, wal stop: 000000020000000800000003\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000030000000000000000, wal stop: 000000030000000000000001\n"
                "P00   INFO: Results:\n"
                "              archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0\n"
                "              archiveId: 11-2, total WAL checked: 8, total valid WAL: 5\n"
                "                missing: 0, checksum invalid: 1, size invalid: 1, other: 1",
                 testPath(), strZ(archiveStanzaPath))));

        harnessLogLevelReset();

        TEST_RESULT_VOID(
            storageRemoveP(
                storageTest,
                strNewFmt("%s/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9",
                    strZ(archiveStanzaPath))),
            "remove unreadable WAL");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
