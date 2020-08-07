/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include <stdio.h> // CSHANG remove

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

    // *****************************************************************************************************************************
    if (testBegin("createArchiveIdRange()"))
    {
        WalRange *walRangeResult = NULL;
        unsigned int errTotal = 0;
        StringList *walFileList = strLstNew();

        ArchiveIdRange archiveIdRange =
        {
            .archiveId = strNew("9.4-1"),
            .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
        };
        List *archiveIdRangeList = lstNewP(sizeof(ArchiveIdRange), .comparator =  archiveIdComparator);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Single WAL");

        archiveIdRange.pgWalInfo.size = PG_WAL_SEGMENT_SIZE_DEFAULT;
        archiveIdRange.pgWalInfo.version = PG_VERSION_94;

        strLstAddZ(walFileList, "000000020000000200000000-daa497dba64008db824607940609ba1cd7c6c501.gz");

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Duplicate WAL only - no range, all removed from list");

        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);

        // Add a duplicate
        strLstAddZ(walFileList, "000000020000000200000000");

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 1, "duplicate WAL error");
        TEST_RESULT_UINT(strLstSize(walFileList), 0, "all WAL removed from WAL list");
        TEST_RESULT_UINT(lstSize(archiveIdRangeList), 0, "no range");
        harnessLogResult("P00  ERROR: [028]: duplicate WAL '000000020000000200000000' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal not skipped > 9.2, duplicates at beginning of list are removed");

        errTotal = 0;
        strLstAddZ(walFileList, "000000020000000100000000-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "000000020000000100000000");
        strLstAddZ(walFileList, "000000020000000100000000-aaaaaadba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz");
        strLstAddZ(walFileList, "0000000200000001000000FF-daa497dba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "000000020000000200000000");

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 1, "triplicate WAL error");
        TEST_RESULT_UINT(strLstSize(walFileList), 4, "only duplicate WAL removed from WAL list");
        TEST_RESULT_UINT(lstSize(archiveIdRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        harnessLogResult("P00  ERROR: [028]: duplicate WAL '000000020000000100000000' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal skipped <= 9.2, duplicates in middle of list removed");

        // Clear the range lists and rerun the test with PG_VERSION_92 to ensure FF is reported as an error
        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);
        errTotal = 0;

        archiveIdRange.archiveId = strNew("9.2-1");
        archiveIdRange.pgWalInfo.version = PG_VERSION_92;

        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000002");

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 2, "error reported");
        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        harnessLogResult(
            "P00  ERROR: [028]: invalid WAL '0000000200000001000000FF' for '9.2-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.2-1' exists, skipping");

        TEST_RESULT_STR_Z(
            strLstJoin(walFileList, ", "),
            "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz, "
            "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz, "
            "000000020000000200000000, 000000020000000200000002",
            "skipped files removed");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Rerun <= 9.2, missing FF not a gap");

        // Clear the range lists, rerun the PG_VERSION_92 test to ensure the missing FF is not considered a gap
        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);
        errTotal = 0;

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "error reported");
        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 0), "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version > 9.2, missing FF is a gap");

        // Clear the range lists and update the version > 9.2 so missing FF is considered a gap in the WAL ranges
        lstClear(archiveIdRangeList);
        lstClear(archiveIdRange.walRangeList);
        errTotal = 0;

        strLstAddZ(walFileList, "000000020000000200000003-123456");
        strLstAddZ(walFileList, "000000020000000200000004-123456");

        archiveIdRange.archiveId = strNew("9.6-1");
        archiveIdRange.pgWalInfo.version = PG_VERSION_96;

        TEST_RESULT_VOID(
            createArchiveIdRange(&archiveIdRange, walFileList, archiveIdRangeList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList), 3, "multiple ranges");
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
        TEST_ASSIGN(
            walRangeResult, (WalRange *)lstGet(((ArchiveIdRange *)lstGet(archiveIdRangeList, 0))->walRangeList, 2),
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
    if (testBegin("setBackupCheckArchive()"))
    {
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
            setBackupCheckArchive(strLstNew(), backupInfo, strLstNew(), pgHistory, &errTotal), NULL, "no archives or backups");
        TEST_RESULT_UINT(errTotal, 0, "no error");
        TEST_RESULT_STR_Z(
            setBackupCheckArchive(
                backupList, backupInfo, archiveIdList, pgHistory, &errTotal), NULL, "no current backup, no missing archive");
        TEST_RESULT_UINT(errTotal, 0, "no error");

        // Add backup to end of list
        strLstAddZ(backupList, "20181119-153000F");
        strLstAddZ(archiveIdList, "12-3");

        TEST_RESULT_STR_Z(
            setBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "current backup, missing archive");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        harnessLogResult("P00  ERROR: [044]: archiveIds '12-3' are not in the archive.info history list");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()- info files"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("neither backup nor archive info files exist");

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
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
        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
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
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
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
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/archive/db/archive.info.copy' for read\n"
            "P00   WARN: no archives or backups exist in the repo", testPath(), testPath())));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy missing, archive.info and copy valid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName), "remove backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoMultiHistoryBase),
            "write valid and matching archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file", testPath(), testPath())));
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyFile(), verifyProtocol()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyFile()");

        String *checksum = strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770");
        String *filePathName = strNewFmt(STORAGE_REPO_ARCHIVE "/testfile");
        const char *fileContents = "acefile";
        unsigned int fileSize = 7;

        VerifyFileResult result = {0};

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRDEF(fileContents)), "put file");
        TEST_ASSIGN(result, verifyFile(filePathName, checksum, false, 0, NULL), "get results valid file");
        TEST_RESULT_UINT(result.fileResult, verifyOk, "file ok");
        TEST_RESULT_STR(result.filePathName, filePathName, "file path name correct");
        TEST_ASSIGN(result, verifyFile(filePathName, checksum, true, fileSize, NULL), "get size results WAL");
        TEST_RESULT_UINT(result.fileResult, verifyOk, "file ok");
        TEST_ASSIGN(result, verifyFile(filePathName, checksum, true, 0, NULL), "get size results WAL");
        TEST_RESULT_UINT(result.fileResult, verifySizeInvalid, "file size invalid");

        TEST_ASSIGN(result, verifyFile(filePathName, strNew("badchecksum"), false, 0, NULL), "get results invalid file");
        TEST_RESULT_UINT(result.fileResult, verifyChecksumMismatch, "file checksum mismatch");
        TEST_RESULT_STR(result.filePathName, filePathName, "file path name correct");

        filePathName = strNewFmt(STORAGE_REPO_ARCHIVE "/missingFile");
        TEST_ASSIGN(result, verifyFile(filePathName, checksum, false, 0, NULL), "get results missing WAL");
        TEST_RESULT_UINT(result.fileResult, verifyFileMissing, "file missing");

        // Create a compressed encrypted repo file
        filePathName = strNew(STORAGE_REPO_BACKUP "/testfile.gz");
        StorageWrite *write = storageNewWriteP(storageRepoWrite(), filePathName);
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(write));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("pass"), NULL));
        storagePutP(write, BUFSTRDEF("acefile"));

        TEST_ASSIGN(result, verifyFile(filePathName, checksum, false, 0, strNew("pass")), "get results encrypted file");
        TEST_RESULT_UINT(result.fileResult, verifyOk, "file ok");
        TEST_RESULT_STR(result.filePathName, filePathName, "file path name correct");

        TEST_ASSIGN(
            result, verifyFile(filePathName, strNew("badchecksum"), false, 0, strNew("pass")), "get results encrypted file");
        TEST_RESULT_UINT(result.fileResult, verifyChecksumMismatch, "file checksum mismatch");

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
        varLstAdd(paramList, varNewStr(strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770")));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewUInt64(0));
        varLstAdd(paramList, varNewStrZ("pass"));

        TEST_RESULT_BOOL(verifyProtocol(PROTOCOL_COMMAND_VERIFY_FILE_STR, paramList, server), true, "protocol verify file");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":[0,\"<REPO:BACKUP>/testfile.gz\"]}\n", "check result");
        bufUsedSet(serverWrite, 0);

        TEST_RESULT_BOOL(verifyProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid protocol function");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()"))
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
            storagePathCreateP(storageTest, strNewFmt("%s/9.4-1", strZ(archiveStanzaPath))),
            "create empty path for old archiveId");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/11-2/0000000100000000", strZ(archiveStanzaPath))),
            "create empty timeline path");

        StorageWrite *write = storageNewWriteP(
            storageTest,
            strNewFmt("%s/11-2/0000000200000000/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz",
            strZ(archiveStanzaPath)));
        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), compressFilter(compressTypeGz, 3));
        TEST_RESULT_VOID(storagePutP(write, walBuffer), "write first WAL compressed - but checksum failure");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000700000FFE-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000700000FFF-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000800000000-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000800000002-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next range");

        // Set log detail level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_STR_Z(
            verifyProcess(),
            "Results:\n"
            "  archiveId: 9.4-1,  total WAL files: 0\n"
            "  archiveId: 11-2,  total WAL files: 5\n"
            "    missing: 0, checksum invalid: 1, size invalid: 0\n",
            "process results");

        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: no backups exist in the repo\n"
                "P00   WARN: path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' is empty\n"
                "P01   WARN: invalid checksum: "
                "<REPO:ARCHIVE>/11-2/0000000200000000/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000800000002, wal stop: 000000020000000800000002")));

        harnessLogLevelReset();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
