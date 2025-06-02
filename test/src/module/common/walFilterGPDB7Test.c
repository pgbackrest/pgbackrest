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
#include "common/walFilter/versions/recordProcessGPDB7.h"
#include "info/infoArchive.h"
#include "postgres/interface/crc32.h"
#include "storage/posix/storage.h"

#define RM7_XACT_ID 0x01
#define XLOG_XACT_COMMIT 0x00
typedef enum WalFlags
{
    NO_SWITCH_WAL = 1 << 0, // Do not add a switch wal record at the end of the WAL.
} WalFlags;

typedef struct XRecordInfo
{
    uint8 rmid;
    uint8 info;
    RelFileNode node;
    bool main_data;
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
    hrnGpdbWalInsertXRecordP(wal, record, flags, .magic = GPDB7_XLOG_PAGE_HEADER_MAGIC, __VA_ARGS__)

#define createXRecord(rmid, info, ...) \
    hrnGpdbCreateXRecordP(PG_VERSION_12, rmid, info, __VA_ARGS__)

#define buildWalP(wal, records, count, flags, ...) \
    buildWal(wal, records, count, flags, (buildWalParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
buildWal(Buffer *wal, XRecordInfo *records, size_t count, WalFlags flags, buildWalParam param)
{
    if (param.pageSize == 0)
        param.pageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;

    for (size_t i = 0; i < count; i++)
    {
        XLogRecordBase *record = NULL;
        if (records[i].main_data)
        {
            record = createXRecord(
                records[i].rmid, records[i].info, .main_data_size = sizeof(RelFileNode), .main_data = &records[i].node);
        }
        else
        {
            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .blockNumber = 1,
                .relFileNode = records[i].node
            };
            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(records[i].rmid, records[i].info, .backupBlocks = backupBlocks);
        }

        if (records[i].rmid == RM_XLOG_ID && records[i].info == XLOG_NOOP)
        {
            overrideXLogRecordBody((XLogRecordGPDB7 *) record);
            ((XLogRecordGPDB7 *) record)->xl_crc = xLogRecordChecksumGPDB7((XLogRecordGPDB7 *) record);
        }
        insertXRecord(wal, record, NO_FLAGS, .walPageSize = param.pageSize);
    }
    if (!(flags & NO_SWITCH_WAL))
    {
        XLogRecordBase *record = createXRecord(0, XLOG_SWITCH);
        hrnGpdbWalInsertXRecordP(wal, record, NO_FLAGS, .walPageSize = param.pageSize);
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
testGetRelfilenode(uint8 rmid, uint8 info, bool expect_not_skip)
{
    RelFileNode node = {1, 2, 3};

    XLogRecordBase *record = createXRecord(rmid, info, .main_data_size = sizeof(node), .main_data = &node);

    const RelFileNode *nodeResult = getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, (uint8 *) record + sizeof(XLogRecordGPDB7) + 2);
    RelFileNode nodeExpect = {1, 2, 3};
    TEST_RESULT_BOOL(nodeResult != NULL, expect_not_skip, "RelFileNode is different from expected");
    if (expect_not_skip)
        TEST_RESULT_BOOL(memcmp(&nodeExpect, nodeResult, sizeof(RelFileNode)), 0, "RelFileNode is different from expected");
    else
        TEST_RESULT_PTR(nodeResult, NULL, "node_result is not empty");
    memFree(record);
}

static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();
    Buffer *wal;
    IoFilter *filter;
    Buffer *result;
    XLogRecordBase *record;

    const PgControl pgControl = {
        .version = PG_VERSION_12,
        .pageSize = DEFAULT_GDPB_PAGE_SIZE,
        .walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE,
        .walSegmentSize = GPDB_XLOG_SEG_SIZE
    };

    const Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    StringList *argBaseList = strLstNew();
    hrnCfgArgRawZ(argBaseList, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);
    hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
    hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
    hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");

    HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");
    hrnCfgArgRawZ(argBaseList, cfgOptFilter, TEST_PATH "/recovery_filter.json");

    HRN_CFG_LOAD(cfgCmdArchiveGet, argBaseList);

    if (testBegin("read begin of the record from prev file"))
    {
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");
        Buffer *wal2;
        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storagePgWrite(), PG_VERSION_12, 12010700, .systemId = HRN_PG_SYSTEMID_12, .catalogVersion = 301908232,
            .pageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE, .walSegmentSize = GPDB_XLOG_SEG_SIZE);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"12\"}");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/12-1");
        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("12-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        TEST_TITLE("simple read begin from prev file");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM7_XACT_ID,
                XLOG_XACT_COMMIT,
                .main_data_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - sizeof(XLogRecordGPDB7) - 24);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 100);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, pgPageSize32);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1, .compressType = compressTypeNone);
        }

        wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 100);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = record->xl_tot_len - 16);
        insertWalSwitchXRecord(wal2);
        fillLastPage(wal2, pgPageSize32);

        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
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
            storagePgWrite(), PG_VERSION_94, 12010700, .systemId = HRN_PG_SYSTEMID_94, .catalogVersion = 301908232,
            .pageSize = pgPageSize32, .walSegmentSize = GPDB_XLOG_SEG_SIZE);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=7374327172765320188\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_12_Z ",\"db-version\":\"12\"}");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/12-1");
        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("12-1"),
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

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 100);
            insertXRecord(wal1, record, NO_FLAGS, .beginOffset = 108);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // 1 is size of block id, 4 is size of main data size field, and 16 is size left in the end of page.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .main_data_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - sizeof(XLogRecordGPDB7) - 1 - 4 - 16);
        insertXRecord(wal2, record, NO_FLAGS);
        record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 100);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1);

        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("read valid wal"))
    {
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");
        TEST_TITLE("one simple record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM_XLOG_ID, XLOG_NOOP);
            insertXRecord(wal, record, NO_FLAGS);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("non noop record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT);
            insertXRecord(wal,record, NO_FLAGS);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has main data (short)");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 100);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has main data (long)");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 500);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has origin");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .main_data_size = 500, .has_origin = true);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has one backup block");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_DATA,
                .data_length = 100,
                .blockNumber = 10,
                .relFileNode = {
                    .relNode = 1000,
                    .dbNode = 1000,
                    .spcNode = 1000,
                }
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has one backup block with image");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_DATA | BKPBLOCK_HAS_IMAGE,
                .data_length = 100,
                .blockNumber = 10,
                .relFileNode = {
                    .relNode = 1000,
                    .dbNode = 1000,
                    .spcNode = 1000,
                },
                .bimg_len = pgPageSize32,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has two backup blocks");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block1 = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .data_length = 0,
                .blockNumber = 10,
                .relFileNode = {
                    .relNode = 1000,
                    .dbNode = 1000,
                    .spcNode = 1000,
                },
                .bimg_len = pgPageSize32,
            };

            BackupBlockInfoGPDB7 block2 = {
                .block_id = 2,
                .fork_flags = BKPBLOCK_HAS_DATA | BKPBLOCK_SAME_REL,
                .data_length = 200,
                .blockNumber = 20,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block1);
            lstAdd(backupBlocks, &block2);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks, .main_data_size = 1000);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has one backup block with compressed image");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_DATA | BKPBLOCK_HAS_IMAGE,
                .data_length = 100,
                .blockNumber = 10,
                .relFileNode = {
                    .relNode = 1000,
                    .dbNode = 1000,
                    .spcNode = 1000,
                },
                .bimg_len = 100,
                .bimg_info = BKPIMAGE_IS_COMPRESSED
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("record has one backup block with compressed image");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_DATA | BKPBLOCK_HAS_IMAGE,
                .data_length = 100,
                .blockNumber = 10,
                .relFileNode = {
                    .relNode = 1000,
                    .dbNode = 1000,
                    .spcNode = 1000,
                },
                .bimg_len = 100,
                .bimg_info = BKPIMAGE_IS_COMPRESSED | BKPIMAGE_HAS_HOLE,
                .hole_offset = 100,
                .hole_length = 10
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("long record with split header");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(
                RM_XLOG_ID,
                XLOG_XACT_COMMIT,
                // Leave exactly 8 bytes free at the end of the page.
                .main_data_size =
                    DEFAULT_GDPB_XLOG_PAGE_SIZE -
                    SizeOfXLogLongPHD -
                    sizeof(XLogRecordGPDB7) -
                    sizeof(XLogRecordDataHeaderLong) -
                    8
                );
            insertXRecord(wal, record, 0);
            record = createXRecord(RM_XLOG_ID, XLOG_XACT_COMMIT, .main_data_size = pgPageSize32 * 2);
            insertXRecord(wal, record, 0);
            record = createXRecord(RM_XLOG_ID, XLOG_XACT_COMMIT, .main_data_size = pgPageSize32 * 6);
            insertXRecord(wal, record, 0);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(wal, result), true, "WAL not the same");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("read invalid wal"))
    {
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", "[]");

        TEST_TITLE("wrong record size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT);
            record->xl_tot_len = 1;
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid record length: wanted 24, got 1");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("wrong resource manager");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(UINT8_MAX, 0);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid resource manager ID");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("wrong checksum");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .xl_crc = 10);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "incorrect resource manager data checksum in record");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("wrong block id");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = XLR_MAX_BLOCK_ID + 1,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "invalid block_id 33");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("out-of-order block_id");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block1 = {
                .block_id = 5,
            };
            BackupBlockInfoGPDB7 block2 = {
                .block_id = 1,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block1);
            lstAdd(backupBlocks, &block2);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "out-of-order block_id 1");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPBLOCK_HAS_DATA set, but no data included");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_HAS_DATA,
                .data_length = 0,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPBLOCK_HAS_DATA set, but no data included");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPBLOCK_HAS_DATA not set, but data length is not zero");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .data_length = 100,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPBLOCK_HAS_DATA not set, but data length is 100");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_HAS_HOLE set, but hole_offset is zero");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = pgPageSize32,
                .bimg_info = BKPIMAGE_HAS_HOLE,
                .hole_offset = 0,
                .hole_length = 100
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter,
                wal,
                bufSize(wal),
                bufSize(wal)),
            FormatError,
            "BKPIMAGE_HAS_HOLE set, but hole offset 0 length 0 block image length 32768");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_HAS_HOLE set, but hole_length is zero");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = pgPageSize32,
                .bimg_info = BKPIMAGE_HAS_HOLE,
                .hole_offset = 100,
                .hole_length = 0
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter,
                wal,
                bufSize(wal),
                bufSize(wal)),
            FormatError,
            "BKPIMAGE_HAS_HOLE set, but hole offset 100 length 0 block image length 32768");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_HAS_HOLE set, but bimg_len equal the page size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = pgPageSize32,
                .bimg_info = BKPIMAGE_HAS_HOLE | BKPIMAGE_IS_COMPRESSED,
                .hole_offset = 100,
                .hole_length = 10
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter,
                wal,
                bufSize(wal),
                bufSize(wal)),
            FormatError,
            "BKPIMAGE_HAS_HOLE set, but hole offset 100 length 10 block image length 32768");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_HAS_HOLE not set, but hole_offset is not zero");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = pgPageSize32,
                .bimg_info = BKPIMAGE_IS_COMPRESSED,
                .hole_offset = 100,
                .hole_length = 10
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPIMAGE_HAS_HOLE not set, but hole offset 100 length 0");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_HAS_HOLE not set, but hole_length is not zero");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = 1000,
                .hole_offset = 0,
                .hole_length = 100
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPIMAGE_HAS_HOLE not set, but hole offset 0 length 31768");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_IS_COMPRESSED set, but block image length equal page size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .fork_flags = BKPBLOCK_HAS_IMAGE,
                .block_id = 1,
                .bimg_len = pgPageSize32,
                .bimg_info = BKPIMAGE_IS_COMPRESSED
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPIMAGE_IS_COMPRESSED set, but block image length 32768");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("BKPIMAGE_IS_COMPRESSED set, but block image length equal page size");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block = {
                .block_id = 1,
                .fork_flags = BKPBLOCK_SAME_REL,
                .blockNumber = 10,
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(testFilter(filter, wal, bufSize(wal), bufSize(wal)), FormatError, "BKPBLOCK_SAME_REL set but no previous rel");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("Different relFileNode in the same record");
        MEM_CONTEXT_TEMP_BEGIN();
        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(1024 * 1024);

            BackupBlockInfoGPDB7 block1 = {
                .block_id = 10,
                .relFileNode = {
                    20000,
                    20000,
                    20000
                }
            };
            BackupBlockInfoGPDB7 block2 = {
                .block_id = 11,
                .relFileNode = {
                    30000,
                    30000,
                    30000
                }
            };
            BackupBlockInfoGPDB7 block3 = {
                .block_id = 12,
                .relFileNode = {
                    2000,
                    2000,
                    2000
                }
            };

            List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
            lstAdd(backupBlocks, &block1);
            lstAdd(backupBlocks, &block2);
            lstAdd(backupBlocks, &block3);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal, record, NO_FLAGS, .walPageSize = pgPageSize32);
            fillLastPage(wal, DEFAULT_GDPB_XLOG_PAGE_SIZE);
        }
        TEST_ERROR(
            testFilter(
                filter, wal, bufSize(wal), bufSize(wal)), ConfigError, "The following RefFileNodes cannot be filtered out because"
            " they are in the same XLogRecord as the RefFileNode that"
            " passes the filter."
            " [{20000, 20000, 20000}, {30000, 30000, 30000}]."
            "\nHINT: Add these RelFileNodes to your filter.");
        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("Filter test"))
    {
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
        StringList *argListCommon = strLstNew();
        hrnCfgArgRawZ(argListCommon, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argListCommon, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argListCommon, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argListCommon, cfgOptFork, CFGOPTVAL_FORK_GPDB_Z);

        storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", strZ(jsonstr));
        hrnCfgArgRawZ(argListCommon, cfgOptFilter, TEST_PATH "/recovery_filter.json");
        HRN_CFG_LOAD(cfgCmdRestore, argListCommon);

        TEST_TITLE("filter record");
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
                {2000, 20003, 1000},
                {2000, 20003, 1001},
            };

            XRecordInfo records[] = {
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[0]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[1]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[2]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[3]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[4]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[5]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[6]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[7]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[8]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[9]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[10]},
                {RM7_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT, nodes[11], true},
            };

            XRecordInfo records_expected[] = {
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[0]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[1]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[2]},

                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[3]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[4]},
                {RM_XLOG_ID, XLOG_NOOP,        nodes[5]},

                {RM_XLOG_ID, XLOG_NOOP,        nodes[6]},
                {RM_XLOG_ID, XLOG_NOOP,        nodes[7]},
                {RM_XLOG_ID, XLOG_NOOP,        nodes[8]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[9]},
                {RM7_XACT_ID, XLOG_XACT_COMMIT, nodes[10]},
                {RM7_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT, nodes[11], true},
            };

            buildWalP(wal, records, LENGTH_OF(records), 0);
            buildWalP(expect_wal, records_expected, LENGTH_OF(records_expected), 0);
            result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
            TEST_RESULT_BOOL(bufEq(expect_wal, result), true, "WAL not the same");
        }

        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("simple read begin from prev file with filter");
        MEM_CONTEXT_TEMP_BEGIN();

        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("9.4-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        BackupBlockInfoGPDB7 block = {
            .block_id = 1,
            .blockNumber = 1,
            .relFileNode = {
                20000,
                20000,
                20000
            }
        };
        List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
        lstAdd(backupBlocks, &block);

        filter = walFilterNew(pgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(
                RM7_XACT_ID,
                XLOG_XACT_COMMIT,
                .main_data_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - sizeof(XLogRecordGPDB7) - 16);
            insertXRecord(wal1, record, NO_FLAGS);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal1, record, INCOMPLETE_RECORD);
            fillLastPage(wal1, pgPageSize32);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1, .compressType = compressTypeNone);
        }

        Buffer *wal2 = bufNew(1024 * 1024);
        record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
        insertXRecord(wal2, record, NO_FLAGS, .segno = 2, .beginOffset = record->xl_tot_len - 8);
        insertWalSwitchXRecord(wal2);
        fillLastPage(wal2, pgPageSize32);

        Buffer *wal_expected = bufNew(1024 * 1024);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .backupBlocks = backupBlocks);
        overrideXLogRecordBody((XLogRecordGPDB7 *) record);
        ((XLogRecordGPDB7 *) record)->xl_crc = xLogRecordChecksumGPDB7((XLogRecordGPDB7 *) record);

        insertXRecord(wal_expected, record, NO_FLAGS, .segno = 2, .beginOffset = record->xl_tot_len - 8);
        insertWalSwitchXRecord(wal_expected);
        fillLastPage(wal_expected, pgPageSize32);

        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal_expected, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");
        archiveInfo.file = STRDEF(
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("filter record with the end in the next file");
        MEM_CONTEXT_TEMP_BEGIN();
        PgControl testPgControl = pgControl;
        testPgControl.walSegmentSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
        ArchiveGetFile archiveInfo = {
            .file = STRDEF(
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
            .repoIdx = 0,
            .archiveId = STRDEF("12-1"),
            .cipherType = cipherTypeNone,
            .cipherPassArchive = STRDEF("")
        };

        BackupBlockInfoGPDB7 block = {
            .block_id = 1,
            .blockNumber = 1,
            .relFileNode = {
                20000,
                20000,
                20000
            }
        };
        List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
        lstAdd(backupBlocks, &block);

        filter = walFilterNew(testPgControl, &archiveInfo);
        {
            Buffer *wal1 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);

            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
            insertXRecord(wal1, record, NO_FLAGS, .beginOffset = record->xl_tot_len - 24);
            fillLastPage(wal1, DEFAULT_GDPB_XLOG_PAGE_SIZE);

            HRN_STORAGE_PUT(
                storageRepoWrite(),
                STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
                wal1);
        }

        Buffer *wal2 = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        // 1 is size of block id, 4 is size of main data size field, and 16 is size left in the end of page.
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .main_data_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - sizeof(XLogRecordGPDB7) - 1 - 4 - 24);
        insertXRecord(wal2, record, NO_FLAGS);
        record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks);
        insertXRecord(wal2, record, INCOMPLETE_RECORD, .segno = 1);
        fillLastPage(wal2, DEFAULT_GDPB_XLOG_PAGE_SIZE);

        Buffer *wal2_expected = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(
            RM_XLOG_ID,
            XLOG_NOOP,
            .main_data_size = DEFAULT_GDPB_XLOG_PAGE_SIZE - SizeOfXLogLongPHD - sizeof(XLogRecordGPDB7) - 1 - 4 - 24);
        insertXRecord(wal2_expected, record, NO_FLAGS);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .backupBlocks = backupBlocks);
        overrideXLogRecordBody((XLogRecordGPDB7 *) record);
        ((XLogRecordGPDB7 *) record)->xl_crc = xLogRecordChecksumGPDB7((XLogRecordGPDB7 *) record);
        insertXRecord(wal2_expected, record, INCOMPLETE_RECORD, .segno = 1);
        fillLastPage(wal2_expected, DEFAULT_GDPB_XLOG_PAGE_SIZE);

        result = testFilter(filter, wal2, bufSize(wal2), bufSize(wal2));
        TEST_RESULT_BOOL(bufEq(wal2_expected, result), true, "WAL not the same");

        HRN_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/12-1/0000000100000000/000000010000000000000002-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        MEM_CONTEXT_TEMP_END();

        TEST_TITLE("override long main data");
        MEM_CONTEXT_TEMP_BEGIN();

        BackupBlockInfoGPDB7 block = {
            .block_id = 1,
            .blockNumber = 1,
            .relFileNode = {
                20000,
                20000,
                20000
            }
        };
        List *backupBlocks = lstNewP(sizeof(BackupBlockInfoGPDB7));
        lstAdd(backupBlocks, &block);

        filter = walFilterNew(pgControl, NULL);
        {
            wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
            record = createXRecord(RM7_XACT_ID, XLOG_XACT_COMMIT, .backupBlocks = backupBlocks, .main_data_size = 1000);
            insertXRecord(wal, record, NO_FLAGS);
            insertWalSwitchXRecord(wal);
            fillLastPage(wal, pgPageSize32);
        }
        Buffer *expected_wal = wal = bufNew(DEFAULT_GDPB_XLOG_PAGE_SIZE);
        record = createXRecord(RM_XLOG_ID, XLOG_NOOP, .backupBlocks = backupBlocks, .main_data_size = 1000);
        overrideXLogRecordBody((XLogRecordGPDB7 *) record);
        ((XLogRecordGPDB7 *) record)->xl_crc = xLogRecordChecksumGPDB7((XLogRecordGPDB7 *) record);
        insertXRecord(expected_wal, record, NO_FLAGS);
        insertWalSwitchXRecord(expected_wal);
        fillLastPage(expected_wal, pgPageSize32);
        result = testFilter(filter, wal, bufSize(wal), bufSize(wal));
        TEST_RESULT_BOOL(bufEq(expected_wal, result), true, "WAL not the same");

        MEM_CONTEXT_TEMP_END();
    }

    if (testBegin("getRelFileNodeFromMainData test"))
    {
        TEST_TITLE("Storage");
        testGetRelfilenode(RM7_SMGR_ID, XLOG_SMGR_CREATE, true);

        {
            xl_smgr_truncate xlrec = {
                .rnode = {1, 2, 3}
            };
            record = createXRecord(RM7_SMGR_ID, XLOG_SMGR_TRUNCATE, .main_data_size = sizeof(xlrec), .main_data = &xlrec);
            const RelFileNode *nodeResult = getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, (uint8 *) record + sizeof(XLogRecordGPDB7) + 2);
            TEST_RESULT_BOOL(nodeResult != NULL, true, "relFileNode not found");
            TEST_RESULT_BOOL(memcmp(&xlrec.rnode, nodeResult, sizeof(RelFileNode)) == 0, true, "relFileNode is not the same");
        }
        {
            record = createXRecord(RM7_SMGR_ID, 0xFF);
            TEST_ERROR(getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, NULL), FormatError, "unknown Storage record: 240");
        }
        TEST_TITLE("Heap2");

        testGetRelfilenode(RM7_HEAP2_ID, XLOG_HEAP2_CLEANUP_INFO, true);
        testGetRelfilenode(RM7_HEAP2_ID, 0, false);
        {
            xl_heap_new_cid xlrec = {
                .target = {1, 2, 3}
            };
            record = createXRecord(RM7_HEAP2_ID, XLOG_HEAP2_NEW_CID, .main_data_size = sizeof(xlrec), .main_data = &xlrec);
            const RelFileNode *nodeResult = getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, (uint8 *) record + sizeof(XLogRecordGPDB7) + 2);
            TEST_RESULT_BOOL(nodeResult != NULL, true, "relFileNode not found");
            TEST_RESULT_BOOL(memcmp(&xlrec.target, nodeResult, sizeof(RelFileNode)) == 0, true, "relFileNode is not the same");
        }

        TEST_TITLE("Btree");
        testGetRelfilenode(RM7_BTREE_ID, XLOG_BTREE_REUSE_PAGE, true);
        testGetRelfilenode(RM7_BTREE_ID, 0, false);

        TEST_TITLE("Gin");
        testGetRelfilenode(RM7_GIN_ID, XLOG_GIN_SPLIT, true);
        testGetRelfilenode(RM7_GIN_ID, XLOG_GIN_UPDATE_META_PAGE, true);
        testGetRelfilenode(RM7_GIN_ID, 0, false);

        TEST_TITLE("Gist");
        testGetRelfilenode(RM7_GIST_ID, XLOG_GIST_PAGE_REUSE, true);
        testGetRelfilenode(RM7_GIST_ID, 0, false);

        TEST_TITLE("Sequence");
        testGetRelfilenode(RM7_SEQ_ID, XLOG_SEQ_LOG, true);
        {
            record = createXRecord(RM7_SEQ_ID, 0xFF);
            TEST_ERROR(getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, NULL), FormatError, "unknown Sequence: 240");
        }

        TEST_TITLE("Bitmap");
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_INSERT_WORDS, true);
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_UPDATEWORD, true);
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_UPDATEWORDS, true);
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_INSERT_LOVITEM, true);
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_INSERT_BITMAP_LASTWORDS, true);
        testGetRelfilenode(RM7_BITMAP_ID, XLOG_BITMAP_INSERT_META, true);
        testGetRelfilenode(RM7_BITMAP_ID, 0, false);

        TEST_TITLE("Appendonly");
        testGetRelfilenode(RM7_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT, true);
        testGetRelfilenode(RM7_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT, true);
        {
            record = createXRecord(RM7_APPEND_ONLY_ID, 0xFF);
            TEST_ERROR(getRelFileNodeFromMainData((XLogRecordGPDB7 *) record, NULL), FormatError, "unknown Appendonly: 240");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
