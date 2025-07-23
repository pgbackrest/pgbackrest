#include "build.auto.h"

#include <stdbool.h>
#include "common/log.h"
#include "common/partialRestore.h"
#include "postgres/interface/crc32.h"
#include "recordProcessGPDB6.h"

static PgPageSize heapPageSizeGPDB6 = 0;

#define XLOG_HEAP_OPMASK        0x70

enum
{
//    RM_XLOG_ID, defined in header with common definitions
    RM6_XACT_ID =            1,
    RM6_SMGR_ID =            2,
    RM6_CLOG_ID =            3,
    RM6_DBASE_ID =           4,
    RM6_TBLSPC_ID =          5,
    RM6_MULTIXACT_ID =       6,
    RM6_RELMAP_ID =          7,
    RM6_STANDBY_ID =         8,
    RM6_HEAP2_ID =           9,
    RM6_HEAP_ID =           10,
    RM6_BTREE_ID =          11,
    RM6_HASH_ID =           12,
    RM6_GIN_ID =            13,
    RM6_GIST_ID =           14,
    RM6_SEQ_ID =            15,
    RM6_SPGIST_ID =         16,
    RM6_BITMAP_ID =         17,
    RM6_DISTRIBUTEDLOG_ID = 18,
    RM6_APPEND_ONLY_ID =    19,
};

// Get RelFileNode from XLOG record.
// Only XLOG_FPI contains RelFileNode, so the other record types are ignored.
static const RelFileNode *
getXlog(const XLogRecordGPDB6 *record)
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
getStorage(const XLogRecordGPDB6 *record)
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
getHeap2(const XLogRecordGPDB6 *record)
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
getHeap(const XLogRecordGPDB6 *record)
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
getBtree(const XLogRecordGPDB6 *record)
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
getGin(const XLogRecordGPDB6 *record)
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
getGist(const XLogRecordGPDB6 *record)
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
getSeq(const XLogRecordGPDB6 *record)
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
getSpgist(const XLogRecordGPDB6 *record)
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
getBitmap(const XLogRecordGPDB6 *record)
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
getAppendonly(const XLogRecordGPDB6 *record)
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

static const RelFileNode *
getRelFileNode(const XLogRecordGPDB6 *const record)
{
    switch (record->xl_rmid)
    {
        case RM_XLOG_ID:
            return getXlog(record);

        case RM6_SMGR_ID:
            return getStorage(record);

        case RM6_HEAP2_ID:
            return getHeap2(record);

        case RM6_HEAP_ID:
            return getHeap(record);

        case RM6_BTREE_ID:
            return getBtree(record);

        case RM6_GIN_ID:
            return getGin(record);

        case RM6_GIST_ID:
            return getGist(record);

        case RM6_SEQ_ID:
            return getSeq(record);

        case RM6_SPGIST_ID:
            return getSpgist(record);

        case RM6_BITMAP_ID:
            return getBitmap(record);

        case RM6_APPEND_ONLY_ID:
            return getAppendonly(record);

        // Records of these types do not contain a RelFileNode.
        case RM6_XACT_ID:
        case RM6_CLOG_ID:
        case RM6_DBASE_ID:
        case RM6_TBLSPC_ID:
        case RM6_MULTIXACT_ID:
        case RM6_RELMAP_ID:
        case RM6_STANDBY_ID:
        case RM6_DISTRIBUTEDLOG_ID:
            // skip
            return NULL;

        case RM6_HASH_ID:
            THROW(FormatError, "Not supported in GPDB 6. Shouldn't be here");
    }
    THROW(FormatError, "Unknown resource manager");
}

static void
validXLogRecordHeaderGPDB6(const XLogRecordBase *const recordBase)
{
    ASSERT(heapPageSizeGPDB6 != 0);

    const XLogRecordGPDB6 *const record = (const XLogRecordGPDB6 *const) recordBase;

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
    if (record->xl_tot_len < SizeOfXLogRecordGPDB6 + record->xl_len ||
        record->xl_tot_len > SizeOfXLogRecordGPDB6 + record->xl_len +
        XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + heapPageSizeGPDB6))
    {
        THROW_FMT(FormatError, "invalid record length");
    }
    if (record->xl_rmid > RM6_APPEND_ONLY_ID)
    {
        THROW_FMT(FormatError, "invalid resource manager ID %u", record->xl_rmid);
    }
}

static void
validXLogRecordGPDB6(const XLogRecordBase *const recordBase)
{
    ASSERT(heapPageSizeGPDB6 != 0);

    const XLogRecordGPDB6 *const record = (const XLogRecordGPDB6 *const) recordBase;
    const uint32 len = record->xl_len;
    uint32 remaining = record->xl_tot_len;

    remaining -= (uint32) (SizeOfXLogRecordGPDB6 + len);
    pg_crc32 crc = crc32cInit();
    crc = crc32cComp(crc, XLogRecGetData(record), len);

    /* Add in the backup blocks, if any */
    const unsigned char *blk = XLogRecGetData(record) + len;
    for (unsigned int i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
    {
        if (!(record->xl_info & XLR_BKP_BLOCK(i)))
            continue;

        if (remaining < sizeof(BkpBlock))
        {
            THROW_FMT(FormatError, "invalid backup block size in record");
        }

        const BkpBlock *bkpb = (const BkpBlock *) blk;
        if (bkpb->hole_offset + bkpb->hole_length > heapPageSizeGPDB6)
        {
            THROW_FMT(FormatError, "incorrect hole size in record");
        }

        const uint32 blen = (uint32) sizeof(BkpBlock) + heapPageSizeGPDB6 - bkpb->hole_length;
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
    crc = crc32cComp(crc, (const unsigned char *) record, offsetof(XLogRecordGPDB6, xl_crc));
    crc = crc32cFinish(crc);

    if (crc != record->xl_crc)
    {
        THROW_FMT(FormatError, "incorrect resource manager data checksum in record. expect: %u, but got: %u", record->xl_crc, crc);
    }
}

static bool
xLogRecordIsWalSwitchGPDB6(const XLogRecordBase *recordBase)
{
    const XLogRecordGPDB6 *const record = (const XLogRecordGPDB6 *const) recordBase;
    return record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH;
}

FN_EXTERN pg_crc32
xLogRecordChecksumGPDB6(const XLogRecordGPDB6 *const record, const PgPageSize heapPageSize)
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
    crc = crc32cComp(crc, (const unsigned char *) record, offsetof(XLogRecordGPDB6, xl_crc));

    return crc32cFinish(crc);
}

static void
filterRecordGPDB6(XLogRecordBase *const recordBase)
{
    ASSERT(heapPageSizeGPDB6 != 0);

    XLogRecordGPDB6 *const record = (XLogRecordGPDB6 *const) recordBase;

    const RelFileNode *const node = getRelFileNode(record);

    if (node == NULL || isRelationNeeded(node->dbNode, node->spcNode, node->relNode))
    {
        return;
    }

    record->xl_rmid = RM_XLOG_ID;
    record->xl_info = (uint8_t) XLOG_NOOP; // Clear backup blocks bits
    // If the initial record has backup blocks, then treat them as rmgr-specific data
    record->xl_len = record->xl_tot_len - (uint32) SizeOfXLogRecordGPDB6;
    record->xl_crc = xLogRecordChecksumGPDB6(record, heapPageSizeGPDB6);
}

FN_EXTERN WalInterface
getWalInterfaceGPDB6(PgPageSize heapPageSize)
{
    ASSERT(pgPageSizeValid(heapPageSize));

    heapPageSizeGPDB6 = heapPageSize;

    return (WalInterface){
               0xD07E,
               SizeOfXLogRecordGPDB6,
               validXLogRecordHeaderGPDB6,
               validXLogRecordGPDB6,
               xLogRecordIsWalSwitchGPDB6,
               filterRecordGPDB6
    };
}
