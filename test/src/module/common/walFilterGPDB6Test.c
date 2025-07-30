/***********************************************************************************************************************************
Test wal filter
***********************************************************************************************************************************/

#include "command/archive/get/file.h"
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessStorage.h"
#include "common/harnessWal.h"
#include "common/io/bufferRead.h"
#include "common/io/io.h"
#include "common/partialRestore.h"
#include "common/type/json.h"
#include "common/walFilter/versions/recordProcessGPDB6.h"
#include "info/infoArchive.h"
#include "postgres/interface/crc32.h"
#include "storage/posix/storage.h"

typedef enum WalFlags
{
    NO_SWITCH_WAL = 1 << 0, // Do not add a switch wal record at the end of the WAL.
} WalFlags;

typedef struct XRecordInfo
{
    uint8_t rmid;
    uint8_t info;
    uint32_t body_size;
    void *body;
} XRecordInfo;

typedef struct buildWalParam
{
    VAR_PARAM_HEADER;
    PgPageSize pageSize;
} buildWalParam;

static Buffer *
testFilter(IoFilter *filter, Buffer *wal, size_t inputSize, size_t outputSize)
{
    Buffer *filtered = bufNew(1024 * 1024);
    Buffer *output = bufNew(outputSize);
    ioBufferSizeSet(inputSize);

    IoRead *read = ioBufferReadNew(wal);
    ioFilterGroupAdd(ioReadFilterGroup(read), filter);
    ioReadOpen(read);

    while (!ioReadEof(read))
    {
        ioRead(read, output);
        bufCat(filtered, output);
        bufUsedZero(output);
    }

    ioReadClose(read);
    bufFree(output);
    ioFilterFree(filter);

    return filtered;
}

#define insertXRecord(wal, record, flags, ...) \
    hrnGpdbWalInsertXRecordP(wal, record, flags, .magic = GPDB6_XLOG_PAGE_HEADER_MAGIC, __VA_ARGS__)

#define createXRecord(rmid, info, ...) \
    hrnGpdbCreateXRecordP(PG_VERSION_94, rmid, info, __VA_ARGS__)

#define buildWalP(wal, records, count, flags, ...) \
    buildWal(wal, records, count, flags, (buildWalParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
buildWal(Buffer *wal, XRecordInfo *records, size_t count, WalFlags flags, buildWalParam param)
{
    if (param.pageSize == 0)
        param.pageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;

    for (size_t i = 0; i < count; i++)
    {
        XLogRecordBase *record = createXRecord(
            records[i].rmid, records[i].info, .body_size = records[i].body_size, .body = records[i].body);
        insertXRecord(wal, record, NO_FLAGS, .walPageSize = param.pageSize);
    }
    if (!(flags & NO_SWITCH_WAL))
    {
        XLogRecordBase *record = createXRecord(0, XLOG_SWITCH);
        insertXRecord(wal, record, NO_FLAGS, .walPageSize = param.pageSize);
        size_t toWrite = param.pageSize - bufUsed(wal) % param.pageSize;
        memset(bufRemainsPtr(wal), 0, toWrite);
        bufUsedInc(wal, toWrite);
    }
}

static void
fillLastPage(Buffer *wal, PgPageSize pageSize)
{
    if (bufUsed(wal) % pageSize == 0)
        return;

    size_t toWrite = pageSize - bufUsed(wal) % pageSize;
    memset(bufRemainsPtr(wal), 0, toWrite);
    bufUsedInc(wal, toWrite);
}

static void
insertWalSwitchXRecord(Buffer *wal)
{
    XLogRecordBase *record = createXRecord(0, XLOG_SWITCH);
    insertXRecord(wal, record, NO_FLAGS);
}

void
testGetRelfilenode(uint8_t rmid, uint8_t info, bool expect_not_skip)
{
    RelFileNode node = {1, 2, 3};

    XLogRecordBase *record = createXRecord(rmid, info, .body_size = sizeof(node), .body = &node);

    const RelFileNode *nodeResult = getRelFileNode((XLogRecordGPDB6 *) record);
    RelFileNode nodeExpect = {1, 2, 3};
    TEST_RESULT_BOOL(nodeResult != NULL, expect_not_skip, "RelFileNode is different from expected");
    if (expect_not_skip)
        TEST_RESULT_BOOL(memcmp(&nodeExpect, nodeResult, sizeof(RelFileNode)), 0, "RelFileNode is different from expected");
    else
        TEST_RESULT_PTR(nodeResult, NULL, "node_result is not empty");
    memFree(record);
}

static uint32
getRemainingLenOnPage(XLogRecordBase *record, uint32 targetPage, uint32 segSize, XLogRecPtr recPtr)
{
    if (targetPage == 1)
    {
        return 0;
    }

    uint32 leftLen = record->xl_tot_len;
    uint32 pageOffset = recPtr % DEFAULT_GDPB_XLOG_PAGE_SIZE;
    uint32 pageN = 0;
    while (pageN != targetPage - 1)
    {
        uint32 spaceOnPage = DEFAULT_GDPB_XLOG_PAGE_SIZE - recPtr % DEFAULT_GDPB_XLOG_PAGE_SIZE;
        if (pageOffset == 0)
        {
            if (recPtr % segSize == 0)
            {
                spaceOnPage -= (uint32) SizeOfXLogLongPHD;
                recPtr += SizeOfXLogLongPHD;
            }
            else
            {
                spaceOnPage -= (uint32) SizeOfXLogShortPHD;
                recPtr += SizeOfXLogShortPHD;
            }
        }
        leftLen -= spaceOnPage;
        recPtr += spaceOnPage;
        pageN++;
        pageOffset = 0;
    }

    return leftLen;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();
    Buffer *wal;
    IoFilter *filter;
    Buffer *result;
    XLogRecordBase *record;
    const Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    if (testBegin("read begin of the record from prev file"))
    {
        Buffer *wal2;
        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");

        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");
        hrnCfgArgRawZ(argBaseList, cfgOptFilter, TEST_PATH "/recovery_filter.json");

        HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);

        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storagePgWrite(), PG_VERSION_94, 9420600, .systemId = HRN_PG_SYSTEMID_94, .catalogVersion = 301908232,
            .pageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE, .walSegmentSize = GPDB_XLOG_SEG_SIZE);

        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/9.4-1");
        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE
                "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz"),
            .repoIdx = 0,
            .archiveId = STRDEF("9.4-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        TEST_TITLE("simple read begin from prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1, .compressType = compressTypeGz);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("prev file is partial");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001.partial-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/8000000 - Missing previous WAL file. Is current file is first in the chain?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001.partial-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("prev file is in the another directory");
        MEM_CONTEXT_TEMP_BEGIN();
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000001/000000010000000100000000-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the first file:
            // 40  24 32688 8  |
            // LPH 32   B   RH |
            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS, .segno = 63);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD, .segno = 63);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/00000001000000000000003F-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the first file:
        // 40  124 32
        // LPH RM  RH
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 64, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/00000001000000000000003F-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("incomplete record in the beginning of prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            hrnGpdbWalInsertXRecordP(wal1, record, NO_FLAGS, .beginOffset = 132 - 8);
            // 124 - size of first record on current page
            // 8 - the number of bytes that we want to leave free at the end of the page.
            // 4 - making the record size aligned to 8 bytes
            record = createXRecord(
                RM_XLOG_ID,
                XLOG_NOOP,
                .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogRecordGPDB6 - SizeOfXLogLongPHD - 124 - 8 - 4);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }
        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long incomplete record in the beginning of prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the first file:
            // 40  32728 |24  32744|24  64 32 32648 |
            // LPH  RM   |SPH  RM  |SPH RM RH   B   |
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            insertXRecord(wal1, record, NO_FLAGS, .beginOffset = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32728 |24  160 32 32552 |24  32744 |24  208 32
        // LPH  RM   |SPH  RM RH   B   |SPH  RM   |SPH RM  RH
        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        hrnGpdbWalInsertXRecordP(
            wal2,
            record,
            NO_FLAGS,
            .segno = 2,
            .beginOffset = 32728 + 160);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("a long overridden incomplete record in the beginning of prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the first file:
            // 40  32728 |24  32744 |24  32 32712 |
            // LPH  RM   |SPH  RM   |SPH RH   B   |
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            hrnGpdbWalInsertXRecordP(
                wal1, record, INCOMPLETE_RECORD, .beginOffset = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, .incompletePosition = 1);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            insertXRecord(wal1, record, INCOMPLETE_RECORD | OVERWRITE);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32728 |24  96 32
        // LPH  RM   |SPH RM RH
        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertXRecord(
            wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 32728 + 96);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override record in the beginning of prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, OVERWRITE, .beginOffset = 100);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            hrnGpdbWalInsertXRecordP(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }
        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000033-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);
        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/8000000 - Missing previous WAL file. Is current file is first in the chain?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000033-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no prev file for this backup");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, NO_FLAGS, .segno = 10, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);
        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/28000000 - Missing previous WAL file. Is current file is first in the chain?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no WAL files in repository");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, INCOMPLETE_RECORD, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/8000000 - Missing previous WAL file. Is current file is first in the chain?");

        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("usefully part of the record in prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 16);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 132 - 16);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record is larger than buffer with prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertXRecord(wal2, record, 0, .segno = 2, .beginOffset = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 2 + SizeOfXLogRecordGPDB6) - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("multiply WAL files");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            hrnGpdbWalInsertXRecordP(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000033-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000040-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, 0, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000033-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000040-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("read multiply pages from prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 4);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            insertXRecord(wal1, record, NO_FLAGS);

            // 120 - The size occupied by the previous entry on page 4.
            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogShortPHD - 120 - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            hrnGpdbWalInsertXRecordP(wal1, record, INCOMPLETE_RECORD);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        hrnGpdbWalInsertXRecordP(wal2, record, 0, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long record at the end of the previous file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(wal1), 0, bufUsed(wal1));
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000021-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32728 |24  32744 |24  16552 32 16160 |24  32744 |24  16600 32
        // LPH  RM   |SPH  RM   |SPH  RM   RH   B   |SPH  RM   |SPH  RM   RH
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        insertXRecord(wal2, record, 0, .segno = 2, .beginOffset = 32728 + 32744 + 16552);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertXRecord(wal2, record, NO_FLAGS);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, 1024 * 1024, 1024 * 1024);
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/8000000 - Missing previous WAL file. Is current file is first in the chain?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long incomplete record at the end of the previous file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the second file:
            // 40  32 16384 32 16280 |
            // LPH RH   B   RH   B   |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE / 2);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32728 |24  32 32712 |24  32744 |24  80 32
        // LPH  RM   |SPH RH   B   |SPH  RM   |SPH RM RH
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        insertXRecord(
            wal2, record, INCOMPLETE_RECORD, .segno = 2,
            .beginOffset = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 3 + SizeOfXLogRecordGPDB6) - (SizeOfXLogRecordGPDB6 + 16280));
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertXRecord(wal2, record, OVERWRITE, .segno = 2);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, 1024 * 1024, 1024 * 1024);
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long record at the end of the previous file with small buffer");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(wal1), 0, bufUsed(wal1));
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000011-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32728 |24  32744 |24  16552 32 16160 |24  32744 |24  16600 32
        // LPH  RM   |SPH  RM   |SPH  RM   RH   B   |SPH  RM   |SPH  RM   RH
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        insertXRecord(
            wal2,
            record,
            0,
            .segno = 2,
            .beginOffset = 32728 + 32744 + 16552);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertXRecord(wal2, record, NO_FLAGS);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE, 1024 * 1024);
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: 0/8000000 - Missing previous WAL file. Is current file is first in the chain?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("compressed and encrypted WAL file");
        MEM_CONTEXT_TEMP_BEGIN();
        HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .recurse = true);
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE "\"\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}",
            .cipherType = cipherTypeAes256Cbc);

        hrnCfgArgRawStrId(argBaseList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);

        archiveInfo.cipherType = cipherTypeAes256Cbc;
        archiveInfo.cipherPassArchive = STRDEF(TEST_CIPHER_PASS_ARCHIVE);
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1,
                .compressType = compressTypeGz,
                .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS_ARCHIVE);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, 0, .segno = 2, .beginOffset = 132 - 8);
        insertWalSwitchXRecord(wal2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .recurse = true);
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("read end of the record from next file"))
    {
        Buffer *wal2;
        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");

        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");
        hrnCfgArgRawZ(argBaseList, cfgOptFilter, TEST_PATH "/recovery_filter.json");

        HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);

        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storagePgWrite(), PG_VERSION_94, 9420600, .systemId = HRN_PG_SYSTEMID_94, .catalogVersion = 301908232,
            .pageSize = pgPageSize32, .walSegmentSize = GPDB_XLOG_SEG_SIZE);

        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/9.4-1");
        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("9.4-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        TEST_TITLE("simple read end from next file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 100);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("the next file is partial");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 100);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001.partial-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001.partial-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("next file in the another directory");
        archiveInfo.file = STRDEF("/9.4-1/0000000100000000/00000001000000000000003F-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 100, .segno = 63);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000001/000000010000000100000000-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(GPDB_XLOG_SEG_SIZE);

        // GPDB_XLOG_SEG_SIZE = 64*1024*1024 = 67108864
        // GPDB_XLOG_SEG_SIZE / DEFAULT_GDPB_XLOG_PAGE_SIZE = 2048
        // 2047 * SizeOfXLogShortPHD = 49128
        // 49128 + SizeOfXLogLongPHD + SizeOfXLogRecordGPDB6 = 49200
        // 67108864 - 49200 = 67059664
        // 67059664 - SizeOfXLogRecordGPDB6 = 67059632
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 67059632);

        insertXRecord(wal2, record, NO_FLAGS, .segno = 63);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 63);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000001/000000010000000100000000-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no files in repository");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(
            RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 16);
        insertXRecord(wal2, record, NO_FLAGS);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: The file with the end of the 0/7ff0 record is missing. Has the timeline switch happened?");

        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("multiply WAL files");
        MEM_CONTEXT_TEMP_BEGIN();

        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 100, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000040-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000040-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no next file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *zeros = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            memset(bufPtr(zeros), 0, bufUsed(zeros));

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                zeros);
        }

        wal2 = bufNew(1024 * 1024);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 2);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 2);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: The file with the end of the 0/8007fe0 record is missing. Has the timeline switch happened?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("read more then one page from next file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2 - SizeOfXLogRecordGPDB6);
            insertXRecord(wal1, record, 0, .beginOffset = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2 - SizeOfXLogRecordGPDB6, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID, XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3 - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogShortPHD - SizeOfXLogShortPHD - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 2) - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("a long record at the end of the file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 4);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // B   - record body (var len)
            // layout of the first file:
            // 40  32 32696 |24  72 32 32640 |24 32744
            // LPH RH   B   |SPH B  RH   B   |PH   B
            // DEFAULT_GDPB_XLOG_PAGE_SIZE * 4 - 32640 - 32744 = 65688
            insertXRecord(wal1, record, 0, .beginOffset = 65688, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 4);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3, .incompletePosition = 1);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("absent of the end of record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 2) - SizeOfXLogRecordGPDB6);
            insertXRecord(wal1, record, 0, .beginOffset = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 2) - SizeOfXLogRecordGPDB6);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);
            bufResize(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = (DEFAULT_GDPB_XLOG_PAGE_SIZE * 2) - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");
        TEST_RESULT_LOG("P00   WARN: The file with the end of the 0/ffe0 record is missing. Has the timeline switch happened?");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("usefully part of header in the next file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the second file:
            // 40  124
            // LPH  RM
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 124, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(1024 * 1024);
        // LPH - long page header (40 bytes)
        // SPH - short page header (24 bytes)
        // RH  - record header (32 bytes)
        // RM  - remaining data from prev page (var len)
        // B   - record body (var len)
        // layout of the second file:
        // 40  32 32688 8  |
        // LPH RH   B   RH |
        record = createXRecord(
            RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
        insertXRecord(wal2, record, NO_FLAGS, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE, .segno = 1);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("WAL record larger then WAL segment");
        // Test case when single WAL record is larger than WAL segment
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2;
        Buffer *wals[] = {
            bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2),
            bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2),
            bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2),
            bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2),
            bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2)
        };

        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 9);

        for (uint32 i = 0; i < LENGTH_OF(wals) - 1; i++)
        {
            insertXRecord(
                wals[i],
                record,
                INCOMPLETE_RECORD,
                .beginOffset = getRemainingLenOnPage(record, i * 2 + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, 0),
                .incompletePosition = 1,
                .segno = i + 1,
                .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            String *walName = strNewFmt(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/%s-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                strZ(xLogFileName(1, i + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2)));
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                strZ(walName),
                wals[i]);
        }

        insertXRecord(
            wals[4],
            record,
            NO_FLAGS,
            .beginOffset = getRemainingLenOnPage(record, 9, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, 0),
            .segno = 5,
            .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertWalSwitchXRecord(wals[4]);
        fillLastPage(wals[4], DEFAULT_GDPB_XLOG_PAGE_SIZE);
        HRN_STORAGE_PUT(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000005-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
            wals[4]);

        for (uint32 i = 0; i < LENGTH_OF(wals); i++)
        {
            filter = walFilterNew(testPgControl, &archiveInfo);
            result = testFilter(filter, wals[i], bufSize(wals[i]), bufSize(wals[i]));
            TEST_RESULT_BOOL(bufEq(wals[i], result), true, "WAL not the same");
        }

        for (uint32 i = 0; i < LENGTH_OF(wals); i++)
        {
            String *walName = strNewFmt(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/%s-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                strZ(xLogFileName(1, i + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2)));
            HRN_STORAGE_REMOVE(storageRepoWrite(), strZ(walName));
        }
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("read 64 next WAL files");
        MEM_CONTEXT_TEMP_BEGIN();

        // The maximum size of XLogRecord is 4 GB. Which, with a WAL segment size of 64 MB, gives somewhere between 64-65 file
        // segments to store the maximum size record.
        // In order not to create 4 GB of files during the tests, we simulate the same number of WAL files, but of a smaller size.
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2;

        Buffer *wals[65];
        for (uint32 i = 0; i < LENGTH_OF(wals); i++)
        {
            Buffer *wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            wals[i] = wal;
        }
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 128);

        for (uint32 i = 0; i < LENGTH_OF(wals) - 1; i++)
        {
            insertXRecord(
                wals[i],
                record,
                INCOMPLETE_RECORD,
                .beginOffset = getRemainingLenOnPage(record, i * 2 + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, 0),
                .incompletePosition = 1,
                .segno = i + 1,
                .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);

            String *walName = strNewFmt(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/%s-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                strZ(xLogFileName(1, i + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2)));
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                strZ(walName),
                wals[i]);
        }

        insertXRecord(
            wals[64],
            record,
            NO_FLAGS,
            .beginOffset = getRemainingLenOnPage(record, 129, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, 0),
            .segno = LENGTH_OF(wals) - 1,
            .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        insertWalSwitchXRecord(wals[64]);
        fillLastPage(wals[64], DEFAULT_GDPB_XLOG_PAGE_SIZE);
        HRN_STORAGE_PUT(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000041-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
            wals[64]);

        for (uint32 i = 0; i < LENGTH_OF(wals); i++)
        {
            filter = walFilterNew(testPgControl, &archiveInfo);
            result = testFilter(filter, wals[i], bufSize(wals[i]), bufSize(wals[i]));
            TEST_RESULT_BOOL(bufEq(wals[i], result), true, "WAL not the same");
        }

        for (uint32 i = 0; i < LENGTH_OF(wals); i++)
        {
            String *walName = strNewFmt(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/%s-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                strZ(xLogFileName(1, i + 1, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2)));
            HRN_STORAGE_REMOVE(storageRepoWrite(), strZ(walName));
        }

        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("compressed and encrypted WAL file");
        MEM_CONTEXT_TEMP_BEGIN();
        HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .recurse = true);
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE
            "\"\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}",
            .cipherType = cipherTypeAes256Cbc);

        hrnCfgArgRawStrId(argBaseList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);

        archiveInfo.cipherType = cipherTypeAes256Cbc;
        archiveInfo.cipherPassArchive = STRDEF(TEST_CIPHER_PASS_ARCHIVE);
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal1, record, 0, .beginOffset = 100, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1, .compressType = compressTypeGz, .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS_ARCHIVE);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // Subtract SizeOfXLogRecord twice to leave exactly space at the end of the page for the header of the next record.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 0, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 0, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .recurse = true);
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);
        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("read valid wal"))
    {
        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");

        TEST_TITLE("one simple record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("split header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8},
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("split body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - 500},
                {RM_XLOG_ID, XLOG_NOOP, 1000}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("not enough input buffer - begin of record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6},
                {RM_XLOG_ID, XLOG_NOOP, 100},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, DEFAULT_GDPB_XLOG_PAGE_SIZE, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        TEST_RESULT_BOOL(bufEq(wal, result), true, "not enough input buffer - begin of record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("not enough input buffer - header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8},
                {RM_XLOG_ID, XLOG_NOOP, 100},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, DEFAULT_GDPB_XLOG_PAGE_SIZE, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("not enough input buffer - body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 500},
                {RM_XLOG_ID, XLOG_NOOP, 1000},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, DEFAULT_GDPB_XLOG_PAGE_SIZE, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("copy data after wal switch");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };

            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
            size_t to_write = DEFAULT_GDPB_XLOG_PAGE_SIZE * 3 - bufUsed(wal);
            memset(bufRemainsPtr(wal), 0xFF, to_write);
            bufUsedInc(wal, to_write);
        }
        result = testFilter(filter, wal, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override record in header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);

            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal, record, NO_FLAGS);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 3000);
            insertXRecord(wal, record, INCOMPLETE_RECORD);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, OVERWRITE);
            insertWalSwitchXRecord(wal);

            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override record in body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - 3000);
            insertXRecord(wal, record, NO_FLAGS);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 3000);
            insertXRecord(wal, record, INCOMPLETE_RECORD);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, OVERWRITE);
            insertWalSwitchXRecord(wal);

            size_t to_write = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2 - bufUsed(wal);
            memset(bufRemainsPtr(wal), 0, to_write);
            bufUsedInc(wal, to_write);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override record in long body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - 3000);
            insertXRecord(wal, record, NO_FLAGS);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            insertXRecord(wal, record, INCOMPLETE_RECORD, .incompletePosition = 1);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, OVERWRITE);
            insertWalSwitchXRecord(wal);

            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override record at the beginning");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, OVERWRITE, .beginOffset = 100);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("valid full page image with max size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI;
            info |= XLR_BKP_BLOCK(0);
            info |= XLR_BKP_BLOCK(1);
            info |= XLR_BKP_BLOCK(2);
            info |= XLR_BKP_BLOCK(3);

            uint32_t bodySize = 1 + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE);
            void *body = memNew(bodySize);
            memset(body, 0, bodySize);

            record = createXRecord(0, info, .body_size = bodySize, .body = body, .xl_len = 1);

            insertXRecord(wal, record, NO_FLAGS);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE * 6}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long record with split header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                // Leave exactly 8 bytes free at the end of the page.
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8},
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE * 2},
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE * 6}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("complete wal without wal switch");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6},
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogShortPHD - SizeOfXLogRecordGPDB6},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("wal switch at the end of page");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - SizeOfXLogRecordGPDB6},
                {RM_XLOG_ID, XLOG_SWITCH, 0},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("read invalid wal"))
    {
        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");

        TEST_TITLE("wrong header magic");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);

            hrnGpdbWalInsertXRecordP(wal, record, 0, .magic = 0xDEAD);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/0 - wrong page magic");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("XLP_FIRST_IS_CONTRECORD in the beginning of the record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            hrnGpdbWalInsertXRecordP(wal, record, COND_FLAG);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/8000 - should not be XLP_FIRST_IS_CONTRECORD");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no XLP_FIRST_IS_CONTRECORD in split header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, NO_COND_FLAG);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/7ff8 - should be XLP_FIRST_IS_CONTRECORD");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("no XLP_FIRST_IS_CONTRECORD in split body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 250},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 500);
            insertXRecord(wal, record, NO_COND_FLAG);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/7f08 - should be XLP_FIRST_IS_CONTRECORD");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("zero rem_len");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 250},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 500);
            insertXRecord(wal, record, ZERO_REM_LEN);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/7f08 - invalid contrecord length: expect: 284, get 0");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("wrong rem_len");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 250},
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 500);
            insertXRecord(wal, record, WRONG_REM_LEN);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), FormatError, "0/7f08 - invalid contrecord length: expect: 284, get 1");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("non zero length of xlog switch record body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), NO_SWITCH_WAL);
            record = createXRecord(0, XLOG_SWITCH, .body_size = 100);
            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid xlog switch record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record with zero length");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {0, XLOG_NOOP, 0}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "record with zero length");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("invalid record length");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(0, XLOG_NOOP, .body_size = 100);
            record->xl_tot_len = 60;
            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid record length");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("invalid record length 2");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(0, XLOG_NOOP, .body_size = 100, .xl_len = 10);
            record = memResize(record, 100 + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE) + 1);
            memset(((char *) record) + 100, 0, XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE) + 1);
            record->xl_tot_len = 100 + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE) + 1;
            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid record length");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("invalid resource manager ID");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(UINT8_MAX, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid resource manager ID 255");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("invalid backup block size in record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI | XLR_BKP_BLOCK(0);
            record = createXRecord(0, info, .body_size = 100, .xl_crc = 10);
            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid backup block size in record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("incorrect hole size in record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI | XLR_BKP_BLOCK(0);
            record = createXRecord(
                0, info, .body_size = 100 + sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE, .xl_len = 100, .xl_crc = 10);

            BkpBlock *blkp = (BkpBlock *) (XLogRecGetData(record) + 100);
            blkp->hole_offset = DEFAULT_GDPB_PAGE_SIZE;
            blkp->hole_length = DEFAULT_GDPB_PAGE_SIZE;

            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "incorrect hole size in record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("invalid backup block size in record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI | XLR_BKP_BLOCK(0);
            record = createXRecord(0, info, .body_size = 1000, .xl_len = 100, .xl_crc = 10);

            BkpBlock *blkp = (BkpBlock *) (XLogRecGetData(record) + 100);
            blkp->hole_offset = 0;
            blkp->hole_length = 0;

            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid backup block size in record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("incorrect total length in record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI | XLR_BKP_BLOCK(0);
            record = createXRecord(
                0, info, .body_size = sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE + 100 + 200, .xl_len = 100, .xl_crc = 10);

            BkpBlock *blkp = (BkpBlock *) (XLogRecGetData(record) + 100);
            blkp->hole_offset = 0;
            blkp->hole_length = 0;

            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "incorrect total length in record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("incorrect resource manager data checksum in record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(0, XLOG_NOOP, .body_size = 100, .xl_crc = 10);

            insertXRecord(wal, record, NO_FLAGS);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError,
                   "incorrect resource manager data checksum in record. expect: 10, but got: 942755737");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("GPDB 6 specific tests"))
    {
        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        TEST_TITLE("XLOG");
        testGetRelfilenode(RM_XLOG_ID, XLOG_CHECKPOINT_SHUTDOWN, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_CHECKPOINT_ONLINE, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_NOOP, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_NEXTOID, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_SWITCH, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_BACKUP_END, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_PARAMETER_CHANGE, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_RESTORE_POINT, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_FPW_CHANGE, false);
        testGetRelfilenode(RM_XLOG_ID, XLOG_END_OF_RECOVERY, false);

        testGetRelfilenode(RM_XLOG_ID, XLOG_FPI, true);

        record = createXRecord(RM_XLOG_ID, 0xD0, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "XLOG UNKNOWN: 208");
        memFree(record);

        TEST_TITLE("Storage");
        testGetRelfilenode(RM6_SMGR_ID, XLOG_SMGR_CREATE, true);

        record = createXRecord(RM6_SMGR_ID, XLOG_SMGR_TRUNCATE, .body_size = 100);

        {
            xl_smgr_truncate *xlrec = (xl_smgr_truncate *) XLogRecGetData(record);
            xlrec->rnode.spcNode = 1;
            xlrec->rnode.dbNode = 2;
            xlrec->rnode.relNode = 3;

            const RelFileNode *node = getRelFileNode((XLogRecordGPDB6 *) record);
            RelFileNode node_expect = {1, 2, 3};
            TEST_RESULT_PTR_NE(node, NULL, "wrong result from get_relfilenode");
            TEST_RESULT_BOOL(memcmp(&node_expect, node, sizeof(RelFileNode)), 0, "RelFileNode is different from expected");
        }
        memFree(record);

        record = createXRecord(RM6_SMGR_ID, 0x30, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Storage UNKNOWN: 48");
        memFree(record);

        TEST_TITLE("Heap2");
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_REWRITE, false);

        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_CLEAN, true);
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_FREEZE_PAGE, true);
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_CLEANUP_INFO, true);
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_VISIBLE, true);
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_MULTI_INSERT, true);
        testGetRelfilenode(RM6_HEAP2_ID, XLOG_HEAP2_LOCK_UPDATED, true);

        record = createXRecord(RM6_HEAP2_ID, XLOG_HEAP2_NEW_CID, .body_size = 100);

        {
            xl_heap_new_cid *xlrec = (xl_heap_new_cid *) XLogRecGetData(record);
            xlrec->target.spcNode = 1;
            xlrec->target.dbNode = 2;
            xlrec->target.relNode = 3;

            const RelFileNode *node = getRelFileNode((XLogRecordGPDB6 *) record);
            RelFileNode node_expect = {1, 2, 3};
            TEST_RESULT_PTR_NE(node, NULL, "wrong result from get_relfilenode");
            TEST_RESULT_BOOL(memcmp(&node_expect, node, sizeof(RelFileNode)), 0, "RelFileNode is different from expected");
        }
        memFree(record);

        TEST_TITLE("Heap");
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_INSERT, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_DELETE, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_UPDATE, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_HOT_UPDATE, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_NEWPAGE, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_LOCK, true);
        testGetRelfilenode(RM6_HEAP_ID, XLOG_HEAP_INPLACE, true);

        record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_MOVE, .body_size = 100);
        TEST_ERROR(
            getRelFileNode((XLogRecordGPDB6 *) record),
            FormatError,
            "There should be no XLOG_HEAP_MOVE entry for this version of Postgres.");
        memFree(record);

        TEST_TITLE("Btree");
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_INSERT_LEAF, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_INSERT_UPPER, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_SPLIT_L, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_SPLIT_R, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_SPLIT_L_ROOT, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_SPLIT_R_ROOT, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_VACUUM, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_DELETE, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_MARK_PAGE_HALFDEAD, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_UNLINK_PAGE_META, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_UNLINK_PAGE, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_NEWROOT, true);
        testGetRelfilenode(RM6_BTREE_ID, XLOG_BTREE_REUSE_PAGE, true);

        record = createXRecord(RM6_BTREE_ID, 0xF0, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Btree UNKNOWN: 240");
        memFree(record);

        TEST_TITLE("GIN");
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_CREATE_INDEX, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_CREATE_PTREE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_INSERT, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_SPLIT, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_VACUUM_PAGE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_VACUUM_DATA_LEAF_PAGE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_DELETE_PAGE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_UPDATE_META_PAGE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_INSERT_LISTPAGE, true);
        testGetRelfilenode(RM6_GIN_ID, XLOG_GIN_DELETE_LISTPAGE, true);

        record = createXRecord(RM6_GIN_ID, 0xA0, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "GIN UNKNOWN: 160");
        memFree(record);

        TEST_TITLE("GIST");
        testGetRelfilenode(RM6_GIST_ID, XLOG_GIST_PAGE_UPDATE, true);
        testGetRelfilenode(RM6_GIST_ID, XLOG_GIST_PAGE_SPLIT, true);
        testGetRelfilenode(RM6_GIST_ID, XLOG_GIST_CREATE_INDEX, true);

        record = createXRecord(RM6_GIST_ID, 0x60, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "GIST UNKNOWN: 96");
        memFree(record);

        TEST_TITLE("Sequence");
        testGetRelfilenode(RM6_SEQ_ID, XLOG_SEQ_LOG, true);

        record = createXRecord(RM6_SEQ_ID, 0x10, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Sequence UNKNOWN: 16");
        memFree(record);

        TEST_TITLE("SPGIST");
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_CREATE_INDEX, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_ADD_LEAF, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_MOVE_LEAFS, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_ADD_NODE, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_SPLIT_TUPLE, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_PICKSPLIT, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_VACUUM_LEAF, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_VACUUM_ROOT, true);
        testGetRelfilenode(RM6_SPGIST_ID, XLOG_SPGIST_VACUUM_REDIRECT, true);

        record = createXRecord(RM6_SPGIST_ID, 0x90, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "SPGIST UNKNOWN: 144");
        memFree(record);

        TEST_TITLE("Bitmap");
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_INSERT_LOVITEM, true);
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_INSERT_META, true);
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_INSERT_BITMAP_LASTWORDS, true);
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_INSERT_WORDS, true);
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_UPDATEWORD, true);
        testGetRelfilenode(RM6_BITMAP_ID, XLOG_BITMAP_UPDATEWORDS, true);

        record = createXRecord(RM6_BITMAP_ID, 0x90, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Bitmap UNKNOWN: 144");
        memFree(record);

        TEST_TITLE("Appendonly");
        testGetRelfilenode(RM6_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT, true);
        testGetRelfilenode(RM6_APPEND_ONLY_ID, XLOG_APPENDONLY_TRUNCATE, true);

        record = createXRecord(RM6_APPEND_ONLY_ID, 0x30, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Appendonly UNKNOWN: 48");
        memFree(record);

        TEST_TITLE("Resource managers without Relfilenode");
        testGetRelfilenode(RM6_XACT_ID, 0, false);
        testGetRelfilenode(RM6_CLOG_ID, 0, false);
        testGetRelfilenode(RM6_DBASE_ID, 0, false);
        testGetRelfilenode(RM6_TBLSPC_ID, 0, false);
        testGetRelfilenode(RM6_MULTIXACT_ID, 0, false);
        testGetRelfilenode(RM6_RELMAP_ID, 0, false);
        testGetRelfilenode(RM6_STANDBY_ID, 0, false);
        testGetRelfilenode(RM6_DISTRIBUTEDLOG_ID, 0, false);

        TEST_TITLE("Unsupported hash resource manager");
        record = createXRecord(RM6_HASH_ID, 0, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Not supported in GPDB 6. Shouldn't be here");
        memFree(record);

        TEST_TITLE("Unknown resource manager");
        record = createXRecord(RM6_APPEND_ONLY_ID + 1, 0, .body_size = 100);
        TEST_ERROR(getRelFileNode((XLogRecordGPDB6 *) record), FormatError, "Unknown resource manager");
        memFree(record);

        TEST_TITLE("filter record when begin or end of the record is in the other file");

        ArchiveGetFile archiveInfo = {
            .file =
                STRDEF(
                    STORAGE_REPO_ARCHIVE
                    "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("9.4-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        StringList *argListCommon = strLstNew();
        hrnCfgArgRawZ(argListCommon, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argListCommon, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argListCommon, cfgOptRepoPath, TEST_PATH "/repo");

        hrnCfgArgRawZ(argListCommon, cfgOptFilter, TEST_PATH "/recovery_filter.json");
        hrnCfgArgRawZ(argListCommon, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);
        HRN_CFG_LOAD(cfgCmdRestore, argListCommon);

        storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");

        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        filter = walFilterNew(testPgControl, &archiveInfo);
        RelFileNode node1 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 17000
        };
        RelFileNode node2 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 18000
        };
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the second file:
            // 40  32 32688 8  |
            // LPH RH   B   RH |
            record = createXRecord(
                RM_XLOG_ID, XLOG_NOOP, .body_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8);
            insertXRecord(wal1, record, NO_FLAGS, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal1, record, INCOMPLETE_RECORD, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);

            Buffer *wal4 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // layout of the second file:
            // 40  36
            // LPH RM
            // sizeof(node2) = 12
            // SizeOfXLogRecordGPDB6 = 32
            // 32 + 12 = 44
            // 44 - 8 = 36
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = sizeof(node2), .body = &node2);
            insertXRecord(wal4, record, 0, .beginOffset = 36, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000003-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal4);
        }
        Buffer *wal2;
        {
            wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the second file:
            // 40  36 4 32 32644 4 8  |
            // LPH RM P RH   B   P RH |
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal2, record, 0, .segno = 2, .beginOffset = (sizeof(node1) + SizeOfXLogRecordGPDB6) - 8, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(
                RM_XLOG_ID,
                XLOG_NOOP,
                .body_size =
                    DEFAULT_GDPB_XLOG_PAGE_SIZE -
                    SizeOfXLogLongPHD -
                    SizeOfXLogRecordGPDB6 -
                    ((sizeof(node1) + SizeOfXLogRecordGPDB6) - 8)
                    - 16);
            insertXRecord(wal2, record, NO_FLAGS, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node2), .body = &node2);
            insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 2, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

            fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        Buffer *wal3;
        {
            wal3 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the second file:
            // 40  36 4 32 32644 4 8  |
            // LPH RM P RH   B   P RH |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal3, record, 0, .segno = 2, .beginOffset = (sizeof(node1) + SizeOfXLogRecordGPDB6) - 8, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(
                RM_XLOG_ID,
                XLOG_NOOP,
                .body_size =
                    DEFAULT_GDPB_XLOG_PAGE_SIZE -
                    SizeOfXLogLongPHD -
                    SizeOfXLogRecordGPDB6 -
                    ((sizeof(node1) + SizeOfXLogRecordGPDB6) - 8) -
                    16);
            insertXRecord(wal3, record, NO_FLAGS, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = sizeof(node2), .body = &node2);
            insertXRecord(wal3, record, INCOMPLETE_RECORD, .segno = 2, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

            fillLastPage(wal3, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
                TEST_RESULT_BOOL(bufEq(wal3, result), true, "WAL not the same");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("Filter record that splits between 2 files");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        RelFileNode node1 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 17000
        };

        Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        Buffer *wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        Buffer *wal1_expected = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        Buffer *wal2_expected = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32 32680 16 |
            // LPH RH   B   RH |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 32680);
            insertXRecord(wal1, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal1, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);

            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  28 32
            // LPH RM RH
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = 28, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            insertWalSwitchXRecord(wal2);
            fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal2);
        }

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32 32680 16 |
            // LPH RH   B   RH |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 32680);
            insertXRecord(wal1_expected, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal1_expected, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            fillLastPage(wal1_expected, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  28 32
            // LPH RM RH
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal2_expected, record, NO_FLAGS, .segno = 2, .beginOffset = 28, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            insertWalSwitchXRecord(wal2_expected);
            fillLastPage(wal2_expected, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }

        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                archiveInfo.file = STRDEF(
                    STORAGE_REPO_ARCHIVE
                    "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
                filter = walFilterNew(testPgControl, &archiveInfo);
                result = testFilter(filter, wal1, bufSize(wal1), bufSize(wal1));
                TEST_RESULT_BOOL(bufEq(wal1_expected, result), true, "WAL not the same");

                archiveInfo.file = STRDEF(
                    STORAGE_REPO_ARCHIVE
                    "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
                filter = walFilterNew(testPgControl, &archiveInfo);
                result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
                TEST_RESULT_BOOL(bufEq(wal2_expected, result), true, "WAL not the same");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();

        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("overwrite contrecord at the beginning of the next file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        RelFileNode node1 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 17000
        };

        Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        Buffer *wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32 32680 16 |
            // LPH RH   B   RH |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 32680);
            insertXRecord(wal1, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = sizeof(node1), .body = &node1);
            insertXRecord(wal1, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32 100 32
            // LPH RH  B  RH
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal2, record, OVERWRITE, .segno = 2, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE);
            insertWalSwitchXRecord(wal2);
            fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal2);
        }

        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                archiveInfo.file = STRDEF(
                    STORAGE_REPO_ARCHIVE
                    "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
                filter = walFilterNew(testPgControl, &archiveInfo);
                result = testFilter(filter, wal1, bufSize(wal1), bufSize(wal1));
                TEST_RESULT_BOOL(bufEq(wal1, result), true, "WAL not the same");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("overwrite contrecord at the beginning of the next file in the second page");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2;
        RelFileNode node1 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 17000
        };

        Buffer *bodyBuf = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);
        memset(bufPtr(bodyBuf), 0, bufSize(bodyBuf));
        *((RelFileNode *)bufRemainsPtr(bodyBuf)) = node1;
        bufUsedSet(bodyBuf, DEFAULT_GDPB_XLOG_PAGE_SIZE * 3);

        Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        Buffer *wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32 32696 |24  32728 16 |
            // LPH RH   B   |SPH  RM   RH |
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 32696 + 32728);
            insertXRecord(wal1, record, NO_FLAGS, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = (uint32) bufUsed(bodyBuf), .body = bufPtr(bodyBuf));
            insertXRecord(wal1, record, INCOMPLETE_RECORD, .segno = 1, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
        }

        {
            // LPH - long page header (40 bytes)
            // SPH - short page header (24 bytes)
            // RH  - record header (32 bytes)
            // RM  - remaining data from prev page (var len)
            // B   - record body (var len)
            // P   - padding
            // layout of the first file:
            // 40  32728 |24  32 100 32
            // LPH  RM   |SPH RH  B  RH
            // DEFAULT_GDPB_XLOG_PAGE_SIZE * 3 - 16 = 98288
            record = createXRecord(RM6_HEAP_ID, XLOG_HEAP_INSERT, .body_size = (uint32) bufUsed(bodyBuf), .body = bufPtr(bodyBuf));
            insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 2, .beginOffset = 98288, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = 100);
            insertXRecord(wal2, record, OVERWRITE, .segno = 2, .segSize = DEFAULT_GDPB_XLOG_PAGE_SIZE * 2);
            insertWalSwitchXRecord(wal2);
            fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal2);
        }

        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                archiveInfo.file = STRDEF(
                    STORAGE_REPO_ARCHIVE
                    "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
                filter = walFilterNew(testPgControl, &archiveInfo);
                result = testFilter(filter, wal1, bufSize(wal1), bufSize(wal1));
                TEST_RESULT_BOOL(bufEq(wal1, result), true, "WAL not the same");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("Filter record with backup blocks");
        MEM_CONTEXT_TEMP_BEGIN();
        RelFileNode node1 = {
            .dbNode = 30000,
            .spcNode = 1000,
            .relNode = 17000
        };
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI;
            info |= XLR_BKP_BLOCK(0);
            info |= XLR_BKP_BLOCK(1);
            info |= XLR_BKP_BLOCK(2);
            info |= XLR_BKP_BLOCK(3);

            uint32_t bodySize = sizeof(node1) + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE);
            char *body = memNew(bodySize);
            RelFileNode *node = (RelFileNode *) body;
            *node = node1;
            memset(body + sizeof(node1), 0, bodySize - sizeof(node1));

            record = createXRecord(RM_XLOG_ID, info, .body_size = bodySize, .body = body, .xl_len = sizeof(node1));
            insertXRecord(wal, record, NO_FLAGS);

            insertWalSwitchXRecord(wal);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        Buffer *expect_wal = bufNew(1024 * 1024);
        {
            uint32_t bodySize = sizeof(node1) + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + DEFAULT_GDPB_PAGE_SIZE);
            char *body = memNew(bodySize);
            RelFileNode *node = (RelFileNode *) body;
            *node = node1;
            memset(body + sizeof(node1), 0, bodySize - sizeof(node1));

            record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .body_size = bodySize, .body = body, .xl_len = bodySize);
            insertXRecord(expect_wal, record, NO_FLAGS);

            insertWalSwitchXRecord(expect_wal);

            fillLastPage(expect_wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
                TEST_RESULT_BOOL(bufEq(expect_wal, result), true, "filtered wal is different from expected");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("Filter");
        const String *jsonstr = STRDEF("[\n"
                                       "  {\n"
                                       "    \"dbOid\": 20000,\n"
                                       "    \"tables\": [\n"
                                       "      {\n"
                                       "        \"tablespace\": 1600,\n"
                                       "        \"relfilenode\": 16384\n"
                                       "      },\n"
                                       "      {\n"
                                       "        \"tablespace\": 1601,\n"
                                       "        \"relfilenode\": 16385\n"
                                       "      }\n"
                                       "    ]\n"
                                       "  },\n"
                                       "  {\n"
                                       "    \"dbOid\": 20001,\n"
                                       "    \"tables\": [\n"
                                       "      {\n"
                                       "        \"tablespace\": 1700,\n"
                                       "        \"relfilenode\": 16386\n"
                                       "      }\n"
                                       "    ]\n"
                                       "  },\n"
                                       "  {\n"
                                       "    \"dbOid\": 20002,\n"
                                       "    \"tables\": []\n"
                                       "  }\n"
                                       "]");

        argListCommon = strLstNew();
        hrnCfgArgRawZ(argListCommon, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argListCommon, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argListCommon, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);

        storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", strZ(jsonstr));

        hrnCfgArgRawZ(argListCommon, cfgOptFilter, TEST_PATH "/recovery_filter.json");
        HRN_CFG_LOAD(cfgCmdRestore, argListCommon);

        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        wal = bufNew(1024 * 1024);
        Buffer *expect_wal = bufNew(1024 * 1024);

        {
            RelFileNode nodes[] = {
                // records that should pass the filter
                {1600, 20000, 16384},
                {1601, 20000, 16385},
                {1700, 20001, 16386},
                // we should filter out all record for this database expect system catalog
                {1700, 20002, 13836},
                // should not be filter out
                {1600, 20002, 11612},
                // should be filter out
                {1600, 20002, 19922},

                {1800, 20000, 35993},
                {1800, 20000, 25928},
                {2000, 20001, 48457},
                // should pass filter
                {2000, 20001, 5445},
                {2000, 20003, 1000}
            };

            XRecordInfo records[] = {
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[0]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[1]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[2]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[3]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[4]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[5]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[6]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[7]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[8]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[9]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[10]},
            };

            XRecordInfo records_expected[] = {
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[0]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[1]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[2]},

                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[3]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[4]},
                {RM_XLOG_ID, XLOG_NOOP,        sizeof(RelFileNode), &nodes[5]},

                {RM_XLOG_ID, XLOG_NOOP,        sizeof(RelFileNode), &nodes[6]},
                {RM_XLOG_ID, XLOG_NOOP,        sizeof(RelFileNode), &nodes[7]},
                {RM_XLOG_ID, XLOG_NOOP,        sizeof(RelFileNode), &nodes[8]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[9]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[10]},
            };

            buildWalP(wal, records, LENGTH_OF(records), 0);
            buildWalP(expect_wal, records_expected, LENGTH_OF(records_expected), 0);
        }

        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                Buffer *result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
                TEST_RESULT_BOOL(bufEq(expect_wal, result), true, "filtered wal is different from expected");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("Filter - empty filer list");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        wal = bufNew(1024 * 1024);
        Buffer *expect_wal = bufNew(1024 * 1024);

        {
            RelFileNode nodes[] = {
                {1600, 1, 1000},      // template1
                {1600, 12809, 1000},  // template0
                {1700, 12812, 1000},  // postgres system catalog
                {1700, 12812, 17000}, // postgres
                {1700, 16399, 17000}, // user database
            };

            XRecordInfo records[] = {
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[0]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[1]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[2]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[3]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[4]},
            };

            XRecordInfo records_expected[] = {
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[0]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[1]},
                {RM6_HEAP_ID, XLOG_HEAP_INSERT, sizeof(RelFileNode), &nodes[2]},
                {RM_XLOG_ID, XLOG_NOOP, sizeof(RelFileNode), &nodes[3]},
                {RM_XLOG_ID, XLOG_NOOP, sizeof(RelFileNode), &nodes[4]},
            };

            buildWalP(wal, records, LENGTH_OF(records), 0);
            buildWalP(expect_wal, records_expected, LENGTH_OF(records_expected), 0);
        }

        // We run filtering in the child process to clear the filter list after the test is completed.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                Buffer *result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
                TEST_RESULT_BOOL(bufEq(expect_wal, result), true, "filtered wal is different from expected");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("Alternative page sizes"))
    {
        // Some basic tests of non-standard (for gpdb) xlog page sizes.
        const PgControl pgControl = {
            .version = PG_VERSION_94,
            .pageSize = pgPageSize8,
            .walPageSize = pgPageSize8,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");

        TEST_TITLE("one simple record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0, .pageSize = pgPageSize8);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("split header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, pgPageSize8 - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 8},
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0, .pageSize = pgPageSize8);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("split body");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            XRecordInfo walRecords[] = {
                {RM_XLOG_ID, XLOG_NOOP, pgPageSize8 - SizeOfXLogLongPHD - SizeOfXLogRecordGPDB6 - 40},
                {RM_XLOG_ID, XLOG_NOOP, 100}
            };
            buildWalP(wal, walRecords, LENGTH_OF(walRecords), 0, .pageSize = pgPageSize8);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("valid full page image with max size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            uint8_t info = XLOG_FPI;
            info |= XLR_BKP_BLOCK(0);
            info |= XLR_BKP_BLOCK(1);
            info |= XLR_BKP_BLOCK(2);
            info |= XLR_BKP_BLOCK(3);

            uint32_t bodySize = 1 + XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + pgPageSize8);
            void *body = memNew(bodySize);
            memset(body, 0, bodySize);

            XLogRecordBase *record = createXRecord(0, info, .body_size = bodySize, .body = body, .xl_len = 1, .heapPageSize = pgPageSize8);

            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize8);
            record = createXRecord(0, XLOG_SWITCH);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize8);
            fillLastPage(wal, pgPageSize8);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("Unsupported GPDB version"))
    {
        PgControl pgControl = {
            .version = PG_VERSION_95,
            .pageSize = DEFAULT_GDPB_PAGE_SIZE,
            .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
            .walSegmentSize = GPDB_XLOG_SEG_SIZE
        };

        TEST_ERROR(
            walFilterNew(pgControl, NULL),
            VersionNotSupportedError, "WAL filtering is not supported for this version of GPDB");

        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptFork, CFGOPTVAL_FORK_POSTGRESQL_Z);
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);
        pgControl.version = PG_VERSION_94;
        TEST_ERROR(
            walFilterNew(pgControl, NULL),
            VersionNotSupportedError, "WAL filtering is only supported for GPDB 6 and 7");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
