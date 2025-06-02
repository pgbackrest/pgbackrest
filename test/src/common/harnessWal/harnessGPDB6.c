#include "build.auto.h"

#include "../harnessWal.h"
#include "common/type/object.h"
#include "common/walFilter/versions/recordProcessGPDB6.h"
#include "string.h"

#define TRANSACTION_ID_PLACEHOLDER 0xADDE
#define PREV_RECPTR_PLACEHOLDER 0xAABB
#define RECORD_BODY_PLACEHOLDER 0XAB

XLogRecordBase *
hrnGpdbCreateXRecord94GPDB(uint8 rmid, uint8 info, CreateXRecordParam param)
{
    XLogRecordGPDB6 *record = memNew(SizeOfXLogRecordGPDB6 + param.body_size);
    *record = (XLogRecordGPDB6){
        .xl_tot_len = (uint32) (SizeOfXLogRecordGPDB6 + param.body_size),
        .xl_xid = TRANSACTION_ID_PLACEHOLDER,
        .xl_len = param.xl_len == 0 ? param.body_size : param.xl_len,
        .xl_info = info,
        .xl_rmid = (uint8) rmid,
        .xl_prev = PREV_RECPTR_PLACEHOLDER
    };

    size_t alignSize = MAXALIGN(sizeof(XLogRecordGPDB6)) - sizeof(XLogRecordGPDB6);
    memset((char *) record + sizeof(XLogRecordGPDB6), 0, alignSize);

    if (param.body == NULL)
        memset((void *) XLogRecGetData(record), RECORD_BODY_PLACEHOLDER, param.body_size);
    else
        memcpy((void *) XLogRecGetData(record), param.body, param.body_size);

    if (param.xl_crc == 0)
        record->xl_crc = xLogRecordChecksumGPDB6(record, param.heapPageSize);
    else
        record->xl_crc = param.xl_crc;

    return (XLogRecordBase *) record;
}
