#include "build.auto.h"

#include <stdbool.h>
#include "common/log.h"
#include "postgres/interface/crc32.h"
#include "recordProcessGPDB6.h"
#include "xlogInfoGPDB6.h"

#define XLOG_HEAP_OPMASK        0x70

typedef uint32 CommandId;

typedef enum ForkNumber
{
    InvalidForkNumber = -1,
    MAIN_FORKNUM = 0,
    FSM_FORKNUM,
    VISIBILITYMAP_FORKNUM,
    INIT_FORKNUM
} ForkNumber;

typedef struct BkpBlock
{
    RelFileNode node;           /* relation containing block */
    ForkNumber fork;            /* fork within the relation */
    BlockNumber block;          /* block number */
    uint16 hole_offset;         /* number of bytes before "hole" */
    uint16 hole_length;         /* number of bytes in "hole" */

    /* ACTUAL BLOCK DATA FOLLOWS AT END OF STRUCT */
} BkpBlock;

typedef struct xl_smgr_truncate
{
    BlockNumber blkno;
    RelFileNode rnode;
} xl_smgr_truncate;

typedef struct xl_heap_new_cid
{
    /*
     * store toplevel xid so we don't have to merge cids from different
     * transactions
     */
    TransactionId top_xid;
    CommandId cmin;
    CommandId cmax;

    /*
     * don't really need the combocid since we have the actual values right in
     * this struct, but the padding makes it free and its useful for
     * debugging.
     */
    CommandId combocid;

    /*
     * Store the relfilenode/ctid pair to facilitate lookups.
     */
    // RelFileNode is the first field in xl_heaptid.
    RelFileNode target;
} xl_heap_new_cid;

// Get RelFileNode from XLOG record.
// Only XLOG_FPI contains RelFileNode, so the other record types are ignored.
static const RelFileNode *
getXlog(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_CHECKPOINT_SHUTDOWN:
        case XLOG_CHECKPOINT_ONLINE:
        case XLOG_NOOP:
        case XLOG_NEXTOID:
        case XLOG_NEXTRELFILENODE:
        case XLOG_RESTORE_POINT:
        case XLOG_BACKUP_END:
        case XLOG_PARAMETER_CHANGE:
        case XLOG_FPW_CHANGE:
        case XLOG_END_OF_RECOVERY:
        case XLOG_OVERWRITE_CONTRECORD:
        case XLOG_SWITCH:
//          ignore
            return NULL;

        case XLOG_FPI:
            return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "XLOG UNKNOWN: %d", info);
}

// Get RelFileNode from Storage record.
// in XLOG_SMGR_TRUNCATE, the RelFileNode is not at the beginning of the structure.
static const RelFileNode *
getStorage(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_SMGR_CREATE:
            return (const RelFileNode *) XLogRecGetData(record);

        case XLOG_SMGR_TRUNCATE:
        {
            const xl_smgr_truncate *xlrec = (const xl_smgr_truncate *) XLogRecGetData(record);
            return &xlrec->rnode;
        }
    }
    THROW_FMT(FormatError, "Storage UNKNOWN: %d", info);
}

// Get RelFileNode from Heap2 record.
// Only XLOG_HEAP2_REWRITE not contains RelFileNode, so it are ignored.
// in XLOG_HEAP2_NEW_CID, the RelFileNode is not at the beginning of the structure.
// this function does not throw errors because XLOG_HEAP_OPMASK contains only 3 non-zero bits, which gives 8 possible values, all of
// which are used.
static const RelFileNode *
getHeap2(const XLogRecord *record)
{
    uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    info &= XLOG_HEAP_OPMASK;

    if (info == XLOG_HEAP2_NEW_CID)
    {
        const xl_heap_new_cid *xlrec = (const xl_heap_new_cid *) XLogRecGetData(record);
        return &xlrec->target;
    }

    if (info == XLOG_HEAP2_REWRITE)
        return NULL;

    // XLOG_HEAP2_CLEAN
    // XLOG_HEAP2_FREEZE_PAGE
    // XLOG_HEAP2_CLEANUP_INFO
    // XLOG_HEAP2_VISIBLE
    // XLOG_HEAP2_MULTI_INSERT
    // XLOG_HEAP2_LOCK_UPDATED
    return (const RelFileNode *) XLogRecGetData(record);
}

// Get RelFileNode from Heap record.
// This function throws an exception only for XLOG_HEAP_MOVE since this type is no longer used.
static const RelFileNode *
getHeap(const XLogRecord *record)
{
    uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    info &= XLOG_HEAP_OPMASK;

    if (info == XLOG_HEAP_MOVE)
    {
        // XLOG_HEAP_MOVE is no longer used.
        THROW(FormatError, "There should be no XLOG_HEAP_MOVE entry for this version of Postgres.");
    }

    // XLOG_HEAP_INSERT
    // XLOG_HEAP_DELETE
    // XLOG_HEAP_UPDATE
    // XLOG_HEAP_HOT_UPDATE
    // XLOG_HEAP_NEWPAGE
    // XLOG_HEAP_LOCK
    // XLOG_HEAP_INPLACE
    return (const RelFileNode *) XLogRecGetData(record);
}

// Get RelFileNode from Btree record.
static const RelFileNode *
getBtree(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_BTREE_INSERT_LEAF:
        case XLOG_BTREE_INSERT_UPPER:
        case XLOG_BTREE_INSERT_META:
        case XLOG_BTREE_SPLIT_L:
        case XLOG_BTREE_SPLIT_R:
        case XLOG_BTREE_SPLIT_L_ROOT:
        case XLOG_BTREE_SPLIT_R_ROOT:
        case XLOG_BTREE_VACUUM:
        case XLOG_BTREE_DELETE:
        case XLOG_BTREE_MARK_PAGE_HALFDEAD:
        case XLOG_BTREE_UNLINK_PAGE_META:
        case XLOG_BTREE_UNLINK_PAGE:
        case XLOG_BTREE_NEWROOT:
        case XLOG_BTREE_REUSE_PAGE:
            return (const RelFileNode *) XLogRecGetData(record);
    }

    THROW_FMT(FormatError, "Btree UNKNOWN: %d", info);
}

// Get RelFileNode from Gin record.
static const RelFileNode *
getGin(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_GIN_CREATE_INDEX:
        case XLOG_GIN_CREATE_PTREE:
        case XLOG_GIN_INSERT:
        case XLOG_GIN_SPLIT:
        case XLOG_GIN_VACUUM_PAGE:
        case XLOG_GIN_VACUUM_DATA_LEAF_PAGE:
        case XLOG_GIN_DELETE_PAGE:
        case XLOG_GIN_UPDATE_META_PAGE:
        case XLOG_GIN_INSERT_LISTPAGE:
        case XLOG_GIN_DELETE_LISTPAGE:
            return (const RelFileNode *) XLogRecGetData(record);
    }

    THROW_FMT(FormatError, "GIN UNKNOWN: %d", info);
}

// Get RelFileNode from Gist record.
static const RelFileNode *
getGist(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_GIST_PAGE_UPDATE:
        case XLOG_GIST_PAGE_SPLIT:
        case XLOG_GIST_CREATE_INDEX:
            return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "GIST UNKNOWN: %d", info);
}

// Get RelFileNode from Seq record.
static const RelFileNode *
getSeq(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    if (info == XLOG_SEQ_LOG)
    {
        return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "Sequence UNKNOWN: %d", info);
}

// Get RelFileNode from Spgist record.
static const RelFileNode *
getSpgist(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);

    switch (info)
    {
        case XLOG_SPGIST_CREATE_INDEX:
        case XLOG_SPGIST_ADD_LEAF:
        case XLOG_SPGIST_MOVE_LEAFS:
        case XLOG_SPGIST_ADD_NODE:
        case XLOG_SPGIST_SPLIT_TUPLE:
        case XLOG_SPGIST_PICKSPLIT:
        case XLOG_SPGIST_VACUUM_LEAF:
        case XLOG_SPGIST_VACUUM_ROOT:
        case XLOG_SPGIST_VACUUM_REDIRECT:
            return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "SPGIST UNKNOWN: %d", info);
}

// Get RelFileNode from Bitmap record.
static const RelFileNode *
getBitmap(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_BITMAP_INSERT_LOVITEM:
        case XLOG_BITMAP_INSERT_META:
        case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
        case XLOG_BITMAP_INSERT_WORDS:
        case XLOG_BITMAP_UPDATEWORD:
        case XLOG_BITMAP_UPDATEWORDS:
            return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "Bitmap UNKNOWN: %d", info);
}

// Get RelFileNode from Appendonly record.
static const RelFileNode *
getAppendonly(const XLogRecord *record)
{
    const uint8 info = (uint8) (record->xl_info & ~XLR_INFO_MASK);
    switch (info)
    {
        case XLOG_APPENDONLY_INSERT:
        case XLOG_APPENDONLY_TRUNCATE:
            return (const RelFileNode *) XLogRecGetData(record);
    }
    THROW_FMT(FormatError, "Appendonly UNKNOWN: %d", info);
}

FN_EXTERN const RelFileNode *
getRelFileNodeGPDB6(const XLogRecord *record)
{
    switch (record->xl_rmid)
    {
        case RM_XLOG_ID:
            return getXlog(record);

        case RM_SMGR_ID:
            return getStorage(record);

        case RM_HEAP2_ID:
            return getHeap2(record);

        case RM_HEAP_ID:
            return getHeap(record);

        case RM_BTREE_ID:
            return getBtree(record);

        case RM_GIN_ID:
            return getGin(record);

        case RM_GIST_ID:
            return getGist(record);

        case RM_SEQ_ID:
            return getSeq(record);

        case RM_SPGIST_ID:
            return getSpgist(record);

        case RM_BITMAP_ID:
            return getBitmap(record);

        case RM_APPEND_ONLY_ID:
            return getAppendonly(record);

        // Records of these types do not contain a RelFileNode.
        case RM_XACT_ID:
        case RM_CLOG_ID:
        case RM_DBASE_ID:
        case RM_TBLSPC_ID:
        case RM_MULTIXACT_ID:
        case RM_RELMAP_ID:
        case RM_STANDBY_ID:
        case RM_DISTRIBUTEDLOG_ID:
            // skip
            return NULL;

        case RM_HASH_ID:
            THROW(FormatError, "Not supported in GPDB 6. Shouldn't be here");
    }
    THROW(FormatError, "Unknown resource manager");
}

FN_EXTERN void
validXLogRecordHeaderGPDB6(const XLogRecord *const record, const PgPageSize heapPageSize)
{
    /*
     * xl_len == 0 is bad data for everything except XLOG SWITCH, where it is
     * required.
     */
    if (record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH)
    {
        if (record->xl_len != 0)
        {
            THROW_FMT(FormatError, "invalid xlog switch record");
        }
    }
    else if (record->xl_len == 0)
    {
        THROW_FMT(FormatError, "record with zero length");
    }
    if (record->xl_tot_len < SizeOfXLogRecord + record->xl_len ||
        record->xl_tot_len > SizeOfXLogRecord + record->xl_len +
        XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + heapPageSize))
    {
        THROW_FMT(FormatError, "invalid record length");
    }
    if (record->xl_rmid > RM_APPEND_ONLY_ID)
    {
        THROW_FMT(FormatError, "invalid resource manager ID %u", record->xl_rmid);
    }
}

FN_EXTERN void
validXLogRecordGPDB6(const XLogRecord *const record, const PgPageSize heapPageSize)
{
    const uint32 len = record->xl_len;
    uint32 remaining = record->xl_tot_len;

    remaining -= (uint32) (SizeOfXLogRecord + len);
    pg_crc32 crc = crc32cInit();
    crc = crc32cComp(crc, XLogRecGetData(record), len);

    /* Add in the backup blocks, if any */
    const unsigned char *blk = XLogRecGetData(record) + len;
    for (int i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
    {
        if (!(record->xl_info & XLR_BKP_BLOCK(i)))
            continue;

        if (remaining < sizeof(BkpBlock))
        {
            THROW_FMT(FormatError, "invalid backup block size in record");
        }

        const BkpBlock *bkpb = (const BkpBlock *) blk;
        if (bkpb->hole_offset + bkpb->hole_length > heapPageSize)
        {
            THROW_FMT(FormatError, "incorrect hole size in record");
        }

        const uint32 blen = (uint32) sizeof(BkpBlock) + heapPageSize - bkpb->hole_length;
        if (remaining < blen)
        {
            THROW_FMT(FormatError, "invalid backup block size in record");
        }
        remaining -= blen;
        crc = crc32cComp(crc, blk, blen);
        blk += blen;
    }

    /* Check that xl_tot_len agrees with our calculation */
    if (remaining != 0)
    {
        THROW_FMT(FormatError, "incorrect total length in record");
    }

    /* Finally include the record header */
    crc = crc32cComp(crc, (const unsigned char *) record, offsetof(XLogRecord, xl_crc));
    crc = crc32cFinish(crc);

    if (crc != record->xl_crc)
    {
        THROW_FMT(FormatError, "incorrect resource manager data checksum in record. expect: %u, but got: %u", record->xl_crc, crc);
    }
}

pg_crc32
xLogRecordChecksumGPDB6(const XLogRecord *record, const PgPageSize heapPageSize)
{
    const uint32 len = record->xl_len;

    pg_crc32 crc = crc32cInit();
    crc = crc32cComp(crc, XLogRecGetData(record), len);

    /* Add in the backup blocks, if any */
    const unsigned char *blk = XLogRecGetData(record) + len;
    for (int i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
    {
        if (!(record->xl_info & XLR_BKP_BLOCK(i)))
            continue;

        const BkpBlock *bkpb = (const BkpBlock *) blk;

        const uint32 blen = (uint32) sizeof(BkpBlock) + heapPageSize - bkpb->hole_length;

        crc = crc32cComp(crc, blk, blen);
        blk += blen;
    }

    /* Finally include the record header */
    crc = crc32cComp(crc, (const unsigned char *) record, offsetof(XLogRecord, xl_crc));

    return crc32cFinish(crc);
}
