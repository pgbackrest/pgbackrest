#ifndef COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H
#define COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H

#include "common/walFilter/postgresCommon.h"

#define GPDB6_XLOG_PAGE_MAGIC 0xD07E

FN_EXTERN const RelFileNode *getRelFileNodeGPDB6(const XLogRecord *record);

FN_EXTERN void validXLogRecordHeaderGPDB6(const XLogRecord *record, PgPageSize heapPageSize);
FN_EXTERN void validXLogRecordGPDB6(const XLogRecord *record, PgPageSize heapPageSize);
FN_EXTERN pg_crc32 xLogRecordChecksumGPDB6(const XLogRecord *record, const PgPageSize heapPageSize);
#endif // COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H
