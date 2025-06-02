#include "build.auto.h"

#include "../harnessWal.h"
#include "common/type/object.h"
#include "common/walFilter/versions/recordProcessGPDB7.h"
#include "string.h"

#define TRANSACTION_ID_PLACEHOLDER 0xADDE
#define PREV_RECPTR_PLACEHOLDER 0xAABB
#define RECORD_BODY_PLACEHOLDER 0XAB

#define WRITE_FIELD_CONST(type, data)                      \
    do {                                                   \
        record = memResize(record, offset + sizeof(type)); \
        *((type *) ((uint8 *) record + offset)) = data;    \
        offset += sizeof(type);                            \
    } while (0)

#define WRITE_FIELD(data)                     \
    WRITE_FIELD_CONST(__typeof__(data), data)

#define WRITE_DATA(data, size)                                                \
    do {                                                                      \
        record = memResize(record, offset + size);                            \
        if (data)                                                             \
            memcpy((uint8 *) record + offset, data, size);                    \
        else                                                                  \
            memset((uint8 *) record + offset, RECORD_BODY_PLACEHOLDER, size); \
        offset += size;                                                       \
    } while (0)

XLogRecordBase *
hrnGpdbCreateXRecord12GPDB(uint8 rmid, uint8 info, CreateXRecordParam param)
{
    XLogRecordGPDB7 *record = memNew(sizeof(XLogRecordGPDB7));
    *record = (XLogRecordGPDB7){
        .xl_xid = TRANSACTION_ID_PLACEHOLDER,
        .xl_info = info,
        .xl_rmid = (uint8) rmid,
        .xl_prev = PREV_RECPTR_PLACEHOLDER
    };

    size_t offset = sizeof(XLogRecordGPDB7);

    if (param.has_origin)
    {
        // block id
        WRITE_FIELD_CONST(uint8, XLR_BLOCK_ID_ORIGIN);
        WRITE_FIELD_CONST(uint16, 0);
    }

    if (param.backupBlocks && !lstEmpty(param.backupBlocks))
    {
        for (unsigned int i = 0; i < lstSize(param.backupBlocks); i++)
        {
            BackupBlockInfoGPDB7 *block = lstGet(param.backupBlocks, i);
            WRITE_FIELD(block->block_id);
            WRITE_FIELD(block->fork_flags);
            WRITE_FIELD(block->data_length);

            if (block->fork_flags & BKPBLOCK_HAS_IMAGE)
            {
                WRITE_FIELD(block->bimg_len);
                WRITE_FIELD(block->hole_offset);
                WRITE_FIELD(block->bimg_info);

                if (block->bimg_info & BKPIMAGE_HAS_HOLE)
                {
                    WRITE_FIELD(block->hole_length);
                }
            }

            if (!(block->fork_flags & BKPBLOCK_SAME_REL))
            {
                WRITE_FIELD(block->relFileNode);
            }
            WRITE_FIELD(block->blockNumber);
        }
    }

    if (param.main_data_size != 0)
    {
        if (param.main_data_size <= UINT8_MAX)
        {
            uint8 bodySizeSmall = (uint8) param.main_data_size;
            // block id
            WRITE_FIELD_CONST(uint8, XLR_BLOCK_ID_DATA_SHORT);
            WRITE_FIELD(bodySizeSmall);
        }
        else
        {
            // block id
            WRITE_FIELD_CONST(uint8, XLR_BLOCK_ID_DATA_LONG);
            WRITE_FIELD_CONST(uint32, param.main_data_size);
        }
    }

    if (param.backupBlocks && !lstEmpty(param.backupBlocks))
    {
        for (unsigned int i = 0; i < lstSize(param.backupBlocks); i++)
        {
            BackupBlockInfoGPDB7 *block = lstGet(param.backupBlocks, i);

            if (block->fork_flags & BKPBLOCK_HAS_IMAGE)
            {
                record = memResize(record, offset + block->bimg_len);
                memset((uint8 *) record + offset, RECORD_BODY_PLACEHOLDER, block->bimg_len);
                offset += block->bimg_len;
            }

            WRITE_DATA(block->data, block->data_length);
        }
    }

    if (param.main_data_size != 0)
    {
        WRITE_DATA(param.main_data, param.main_data_size);
    }
    ASSERT(offset <= UINT32_MAX);
    record->xl_tot_len = (uint32) offset;
    if (param.xl_crc == 0)
        record->xl_crc = xLogRecordChecksumGPDB7(record);
    else
        record->xl_crc = param.xl_crc;

    return (XLogRecordBase *) record;
}
