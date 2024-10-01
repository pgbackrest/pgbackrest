#ifndef TEST_COMMON_HARNESS_WAL_H
#define TEST_COMMON_HARNESS_WAL_H
#include "common/type/buffer.h"
#include "common/type/param.h"
#include "common/walFilter/postgresCommon.h"

#define DEFAULT_GDPB_XLOG_PAGE_SIZE pgPageSize32
#define DEFAULT_GDPB_PAGE_SIZE pgPageSize32
#define GPDB6_XLOG_PAGE_HEADER_MAGIC 0xD07E
#define GPDB6_XLOG_SEG_SIZE (64 * 1024 * 1024)

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
    uint16_t magic;
    uint32_t beginOffset;
    uint64_t segno;
    PgPageSize walPageSize;
} InsertXRecordParam;

typedef struct CreateXRecordParam
{
    VAR_PARAM_HEADER;
    PgPageSize heapPageSize;
    uint32_t xl_len;
} CreateXRecordParam;

#define hrnGpdbCreateXRecordP(rmid, info, bodySize, body, ...) \
    hrnGpdbCreateXRecord(rmid, info, bodySize, body, (CreateXRecordParam){VAR_PARAM_INIT, __VA_ARGS__})

XLogRecord *hrnGpdbCreateXRecord(uint8_t rmid, uint8_t info, uint32_t bodySize, void *body, CreateXRecordParam param);

#define hrnGpdbWalInsertXRecordP(wal, record, flags, ...) \
    hrnGpdbWalInsertXRecord(wal, record, (InsertXRecordParam){VAR_PARAM_INIT, __VA_ARGS__}, flags)

void hrnGpdbWalInsertXRecord(
    Buffer *const walBuffer,
    XLogRecord *record,
    InsertXRecordParam param,
    InsertRecordFlags flags);
void hrnGpdbWalInsertXRecordSimple(Buffer *const walBuffer, XLogRecord *record);
#endif // TEST_COMMON_HARNESS_WAL_H
