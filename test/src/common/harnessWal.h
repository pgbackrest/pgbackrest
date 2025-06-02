#ifndef TEST_COMMON_HARNESS_WAL_H
#define TEST_COMMON_HARNESS_WAL_H
#include "common/type/buffer.h"
#include "common/type/param.h"
#include "common/walFilter/postgresCommon.h"

#define DEFAULT_GDPB_XLOG_PAGE_SIZE pgPageSize32
#define DEFAULT_GDPB_PAGE_SIZE pgPageSize32
#define GPDB6_XLOG_PAGE_HEADER_MAGIC 0xD07E
#define GPDB7_XLOG_PAGE_HEADER_MAGIC 0xD101
#define GPDB_XLOG_SEG_SIZE (64 * 1024 * 1024)

typedef enum InsertRecordFlags
{
    NO_FLAGS = 0,
    INCOMPLETE_RECORD = 1 << 0, // Do not write the continuation of the record on the next page.
    OVERWRITE = 1 << 1, // Add the XLP_FIRST_IS_OVERWRITE_CONTRECORD flag to the page header.
    COND_FLAG = 1 << 2, // Force the XLP_FIRST_IS_CONTRECORD flag to be set in the page header.
    NO_COND_FLAG = 1 << 3, // Force not to set the XLP_FIRST_IS_CONTRECORD flag in the page header.
    ZERO_REM_LEN = 1 << 4, // Set the xlp_rem_len field to 0 in the page header of the next page.
    WRONG_REM_LEN = 1 << 5 // Set the xlp_rem_len field to a wrong non-zero value (1) in the page header of the next page.
} InsertRecordFlags;

typedef struct InsertXRecordParam
{
    VAR_PARAM_HEADER;
    uint16 magic;
    uint32 beginOffset;
    uint64 segno;
    uint32 segSize;
    PgPageSize walPageSize;
    uint32 incompletePosition;
} InsertXRecordParam;

typedef struct CreateXRecordParam
{
    VAR_PARAM_HEADER;
    PgPageSize heapPageSize;
    uint32 xl_crc;
    // GPDB 6
    uint32 xl_len;
    uint32 body_size;
    void *body;
    // GPDB 7
    List *backupBlocks;
    void *main_data;
    uint32 main_data_size;
    bool has_origin;
} CreateXRecordParam;

typedef struct
{
    uint8 block_id;
    uint8 fork_flags;
    uint16 data_length;

    uint16 bimg_len;
    uint16 hole_offset;
    uint8 bimg_info;
    uint16 hole_length;

    RelFileNode relFileNode;
    BlockNumber blockNumber;
    void *data;
} BackupBlockInfoGPDB7;

#define hrnGpdbCreateXRecordP(pgVersion, rmid, info, ...) \
    hrnGpdbCreateXRecord(pgVersion, rmid, info, (CreateXRecordParam){VAR_PARAM_INIT, __VA_ARGS__})

XLogRecordBase *hrnGpdbCreateXRecord(unsigned int pgVersion, uint8 rmid, uint8 info, CreateXRecordParam param);

#define hrnGpdbWalInsertXRecordP(wal, record, flags, ...) \
    hrnGpdbWalInsertXRecord(wal, record, (InsertXRecordParam){VAR_PARAM_INIT, __VA_ARGS__}, flags)

void hrnGpdbWalInsertXRecord(
    Buffer *const walBuffer,
    XLogRecordBase *record,
    InsertXRecordParam param,
    InsertRecordFlags flags);
#endif // TEST_COMMON_HARNESS_WAL_H
