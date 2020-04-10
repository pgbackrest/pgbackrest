/***********************************************************************************************************************************
Test Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#include "common/io/bufferWrite.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "postgres/interface.h"
#include "postgres/interface/static.auto.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("backupRegExp()"))
    {
        String *full = strNew("20181119-152138F");
        String *incr = strNew("20181119-152138F_20181119-152152I");
        String *diff = strNew("20181119-152138F_20181119-152152D");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            backupRegExpP(0),
            AssertError, "assertion 'param.full || param.differential || param.incremental' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        String *filter = backupRegExpP(.full = true);
        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F$", "full backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "    does not exactly match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "    does not exactly match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    exactly matches full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true, .incremental = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}I){0,1}$", "full and optional incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "    match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "    does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("12341234-123123F_12341234-123123IG")), false, "    does not match with trailing character");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("A12341234-123123F_12341234-123123I")), false, "    does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true, .differential = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}D){0,1}$", "full and optional diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "    does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "    match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true,  .incremental = true, .differential = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}(D|I)){0,1}$",
            "full, optional diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "    match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "    match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.incremental = true, .differential = true, .noAnchorEnd = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}(D|I)", "diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "   match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "   match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("A12341234-123123F_12341234-123123I")), false, "   does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.incremental = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}I$", "incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "   match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "   does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.differential = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}D$", "diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "   does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "   match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");
    }

    // *****************************************************************************************************************************
    if (testBegin("PageChecksum"))
    {
        TEST_RESULT_UINT(PG_SEGMENT_PAGE_DEFAULT, 131072, "check pages per segment");

        // Test pages with all zeros (these are considered valid)
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(PG_PAGE_SIZE_DEFAULT * 3);
        Buffer *bufferOut = bufNew(0);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0};
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0};

        IoWrite *write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(ioWriteFilterGroup(write), pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResult(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE_STR)),
            "{\"align\":true,\"valid\":true}", "all zero pages");

        // Single valid page
        // -------------------------------------------------------------------------------------------------------------------------
        buffer = bufNew(PG_PAGE_SIZE_DEFAULT * 1);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        // Page 0 has good checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xlogid = 0xF0F0F0F0,
                .xrecoff = 0xF0F0F0F0,
            },
        };

        ((PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)))->pd_checksum = pgPageChecksum(
            bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00), 0);

        write = ioBufferWriteNew(bufferOut);

        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNewVar(varVarLst(jsonToVar(strNewFmt("[0,%u,%" PRIu64 "]", PG_SEGMENT_PAGE_DEFAULT, 0xFACEFACE00000000)))));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResult(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE_STR)),
            "{\"align\":true,\"valid\":true}", "single valid page");

        // Single checksum error
        // -------------------------------------------------------------------------------------------------------------------------
        buffer = bufNew(PG_PAGE_SIZE_DEFAULT * 1);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        // Page 0 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xlogid = 0xF0F0F0F0,
                .xrecoff = 0xF0F0F0F0,
            },
        };

        write = ioBufferWriteNew(bufferOut);

        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNewVar(varVarLst(jsonToVar(strNewFmt("[0,%u,%" PRIu64 "]", PG_SEGMENT_PAGE_DEFAULT, 0xFACEFACE00000000)))));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResult(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE_STR)),
            "{\"align\":true,\"error\":[0],\"valid\":false}", "single checksum error");

        // Various checksum errors some of which will be skipped because of the LSN
        // -------------------------------------------------------------------------------------------------------------------------
        buffer = bufNew(PG_PAGE_SIZE_DEFAULT * 8 - (PG_PAGE_SIZE_DEFAULT - 512));
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        // Page 0 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xlogid = 0xF0F0F0F0,
                .xrecoff = 0xF0F0F0F0,
            },
        };

        // Page 1 has bogus checksum but lsn above the limit
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xlogid = 0xFACEFACE,
                .xrecoff = 0x00000000,
            },
        };

        // Page 2 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xrecoff = 0x2,
            },
        };

        // Page 3 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x03)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xrecoff = 0x3,
            },
        };

        // Page 4 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x04)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xrecoff = 0x4,
            },
        };

        // Page 5 is zero
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x05)) = (PageHeaderData){.pd_upper = 0x00};

        // Page 6 has bogus checksum
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x06)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xrecoff = 0x6,
            },
        };

        // Page 7 has bogus checksum (and is misaligned but large enough to test)
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x07)) = (PageHeaderData)
        {
            .pd_upper = 0x01,
            .pd_lsn = (PageXLogRecPtr)
            {
                .xrecoff = 0x7,
            },
        };

        write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(ioWriteFilterGroup(write), pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0xFACEFACE00000000));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResult(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE_STR)),
            "{\"align\":false,\"error\":[0,[2,4],[6,7]],\"valid\":false}", "various checksum errors");

        // Impossibly misaligned page
        // -------------------------------------------------------------------------------------------------------------------------
        buffer = bufNew(256);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

        write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(ioWriteFilterGroup(write), pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0xFACEFACE00000000));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResult(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE_STR)),
            "{\"align\":false,\"valid\":false}", "misalignment");

        // Two misaligned buffers in a row
        // -------------------------------------------------------------------------------------------------------------------------
        buffer = bufNew(513);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

        write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(ioWriteFilterGroup(write), pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, 0xFACEFACE00000000));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        TEST_ERROR(ioWrite(write, buffer), AssertError, "should not be possible to see two misaligned pages in a row");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupType() and backupTypeStr()"))
    {
        TEST_RESULT_UINT(backupType(strNew("full")), backupTypeFull, "backup type full");
        TEST_RESULT_UINT(backupType(strNew("diff")), backupTypeDiff, "backup type diff");
        TEST_RESULT_UINT(backupType(strNew("incr")), backupTypeIncr, "backup type incr");
        TEST_ERROR(backupType(strNew("bogus")), AssertError, "invalid backup type 'bogus'");

        TEST_RESULT_STR_Z(backupTypeStr(backupTypeFull), "full", "backup type str full");
        TEST_RESULT_STR_Z(backupTypeStr(backupTypeDiff), "diff", "backup type str diff");
        TEST_RESULT_STR_Z(backupTypeStr(backupTypeIncr), "incr", "backup type str incr");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupLinkLatest()"))
    {
        String *backupStanzaPath = strNew("repo/backup/db");
        String *backupLatest = strNew("20181119-152800F");
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/%s/test" , strPtr(backupStanzaPath), strPtr(backupLatest))),
            BUFSTRDEF("tmp"));

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_VOID(backupLinkLatest(backupLatest), "create backup latest symlink");
        TEST_RESULT_STR(storageInfoP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_LINK_LATEST)).linkDestination,
            backupLatest, "latest link set");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
