#ifndef COMMON_WALFILTER_POSTGRESCOMMON_H
#define COMMON_WALFILTER_POSTGRESCOMMON_H

#include <stdint.h>
#include "postgres/interface/static.vendor.h"

// Common macros and constants for different versions of Postgres

// types
typedef uint32 TimeLineID;
typedef uint64 XLogRecPtr;
typedef uint8 RmgrId;
typedef uint32 pg_crc32;
typedef uint32 BlockNumber;

// constants
/* This flag indicates a "long" page header */
#define XLP_LONG_HEADER             0x0002
/* Define as the maximum alignment requirement of any C data type. */
#define MAXIMUM_ALIGNOF 8
/* When record crosses page boundary, set this flag in new page's header */
#define XLP_FIRST_IS_CONTRECORD     0x0001
/* Replaces a missing contrecord; see CreateOverwriteContrecordRecord */
#define XLP_FIRST_IS_OVERWRITE_CONTRECORD 0x0008

/*
 * XLOG uses only low 4 bits of xl_info.  High 4 bits may be used by rmgr.
 */
#define XLR_INFO_MASK           0x0F

#define RM_XLOG_ID 0
// macros

/* ----------------
 * Alignment macros: align a length or address appropriately for a given type.
 * The fooALIGN() macros round up to a multiple of the required alignment,
 * while the fooALIGN_DOWN() macros round down.  The latter are more useful
 * for problems like "how many X-sized structures will fit in a page?".
 *
 * NOTE: TYPEALIGN[_DOWN] will not work if ALIGNVAL is not a power of 2.
 * That case seems extremely unlikely to be needed in practice, however.
 *
 * NOTE: MAXIMUM_ALIGNOF, and hence MAXALIGN(), intentionally exclude any
 * larger-than-8-byte types the compiler might have.
 * ----------------
 */
#define TYPEALIGN(ALIGNVAL,LEN)  \
    (((uintptr_t) (LEN) + ((ALIGNVAL) - 1)) & ~((uintptr_t) ((ALIGNVAL) - 1)))
#define MAXALIGN(LEN)           TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

#define SizeOfXLogShortPHD  MAXALIGN(sizeof(XLogPageHeaderData))
#define SizeOfXLogLongPHD   MAXALIGN(sizeof(XLogLongPageHeaderData))
#define XLogPageHeaderSize(hdr)     \
    (((hdr)->xlp_info & XLP_LONG_HEADER) ? SizeOfXLogLongPHD : SizeOfXLogShortPHD)
#define Min(x, y)       ((x) < (y) ? (x) : (y))

/*
 * The XLOG is split into WAL segments (physical files) of the size indicated
 * by XLOG_SEG_SIZE.
 */
#define XLogSegmentsPerXLogId(segSize)   (0x100000000ULL / segSize)
#define XLogFromFileName(fname, tli, logSegNo, segSize)  \
    do {                                                \
        uint32 log;                                     \
        uint32 seg;                                     \
        sscanf(fname, "%08X%08X%08X", tli, &log, &seg); \
        *logSegNo = (uint64) log * XLogSegmentsPerXLogId(segSize) + seg; \
    } while (0)

// structs
typedef struct XLogPageHeaderData
{
    uint16 xlp_magic;           /* magic value for correctness checks */
    uint16 xlp_info;            /* flag bits, see below */
    TimeLineID xlp_tli;         /* TimeLineID of first record on page */
    XLogRecPtr xlp_pageaddr;    /* XLOG address of this page */

    /*
     * When there is not enough space on current page for whole record, we
     * continue on the next page.  xlp_rem_len is the number of bytes
     * remaining from a previous page.
     *
     * Note that xl_rem_len includes backup-block data; that is, it tracks
     * xl_tot_len not xl_len in the initial header.  Also note that the
     * continuation data isn't necessarily aligned.
     */
    uint32 xlp_rem_len;         /* total len of remaining data for record */
} XLogPageHeaderData;

typedef struct XLogLongPageHeaderData
{
    XLogPageHeaderData std;     /* standard header fields */
    uint64 xlp_sysid;           /* system identifier from pg_control */
    uint32 xlp_seg_size;        /* just as a cross-check */
    uint32 xlp_xlog_blcksz;         /* just as a cross-check */
} XLogLongPageHeaderData;

// This part of XLogRecord is the same in all supported versions of PostgreSQL.
typedef struct XLogRecordBase
{
    uint32 xl_tot_len;          /* total len of entire record */
    TransactionId xl_xid;       /* xact id */
} XLogRecordBase;

typedef uint32 CommandId;

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

enum
{
    // xlog
    XLOG_CHECKPOINT_SHUTDOWN =  0x00,
    XLOG_CHECKPOINT_ONLINE =    0x10,
    XLOG_NOOP =                 0x20,
    XLOG_NEXTOID =              0x30,
    XLOG_SWITCH =               0x40,
    XLOG_BACKUP_END =           0x50,
    XLOG_PARAMETER_CHANGE =     0x60,
    XLOG_RESTORE_POINT =        0x70,
    XLOG_FPW_CHANGE =           0x80,
    XLOG_END_OF_RECOVERY =      0x90,
    XLOG_FPI =                  0xA0,
    XLOG_NEXTRELFILENODE =      0xB0,
    XLOG_OVERWRITE_CONTRECORD = 0xC0,

// storage
    XLOG_SMGR_CREATE =    0x10,
    XLOG_SMGR_TRUNCATE =  0x20,

// heap
    XLOG_HEAP2_REWRITE =      0x00,
    XLOG_HEAP2_CLEAN =        0x10,
    XLOG_HEAP2_FREEZE_PAGE =  0x20,
    XLOG_HEAP2_CLEANUP_INFO = 0x30,
    XLOG_HEAP2_VISIBLE =      0x40,
    XLOG_HEAP2_MULTI_INSERT = 0x50,
    XLOG_HEAP2_LOCK_UPDATED = 0x60,
    XLOG_HEAP2_NEW_CID =      0x70,

    XLOG_HEAP_INSERT =        0x00,
    XLOG_HEAP_DELETE =        0x10,
    XLOG_HEAP_UPDATE =        0x20,
    XLOG_HEAP_MOVE =          0x30,
    XLOG_HEAP_HOT_UPDATE =    0x40,
    XLOG_HEAP_NEWPAGE =       0x50,
    XLOG_HEAP_LOCK =          0x60,
    XLOG_HEAP_INPLACE =       0x70,
    XLOG_HEAP_INIT_PAGE =     0x80,

// btree
    XLOG_BTREE_INSERT_LEAF =        0x00,
    XLOG_BTREE_INSERT_UPPER =       0x10,
    XLOG_BTREE_INSERT_META =        0x20,
    XLOG_BTREE_SPLIT_L =            0x30,
    XLOG_BTREE_SPLIT_R =            0x40,
    XLOG_BTREE_SPLIT_L_ROOT =       0x50,
    XLOG_BTREE_SPLIT_R_ROOT =       0x60,
    XLOG_BTREE_DELETE =             0x70,
    XLOG_BTREE_UNLINK_PAGE =        0x80,
    XLOG_BTREE_UNLINK_PAGE_META =   0x90,
    XLOG_BTREE_NEWROOT =            0xA0,
    XLOG_BTREE_MARK_PAGE_HALFDEAD = 0xB0,
    XLOG_BTREE_VACUUM =             0xC0,
    XLOG_BTREE_REUSE_PAGE =         0xD0,

// gin
    XLOG_GIN_CREATE_INDEX =          0x00,
    XLOG_GIN_CREATE_PTREE =          0x10,
    XLOG_GIN_INSERT =                0x20,
    XLOG_GIN_SPLIT =                 0x30,
    XLOG_GIN_VACUUM_PAGE =           0x40,
    XLOG_GIN_VACUUM_DATA_LEAF_PAGE = 0x90,
    XLOG_GIN_DELETE_PAGE =           0x50,
    XLOG_GIN_UPDATE_META_PAGE =      0x60,
    XLOG_GIN_INSERT_LISTPAGE =       0x70,
    XLOG_GIN_DELETE_LISTPAGE =       0x80,

// gist
    XLOG_GIST_PAGE_UPDATE =  0x00,
    XLOG_GIST_PAGE_SPLIT =   0x30,
    XLOG_GIST_CREATE_INDEX = 0x50,
    XLOG_GIST_PAGE_REUSE = 0x20, // GPDB 7

// sequence
    XLOG_SEQ_LOG = 0x00,

// spgist
    XLOG_SPGIST_CREATE_INDEX =    0x00,
    XLOG_SPGIST_ADD_LEAF =        0x10,
    XLOG_SPGIST_MOVE_LEAFS =      0x20,
    XLOG_SPGIST_ADD_NODE =        0x30,
    XLOG_SPGIST_SPLIT_TUPLE =     0x40,
    XLOG_SPGIST_PICKSPLIT =       0x50,
    XLOG_SPGIST_VACUUM_LEAF =     0x60,
    XLOG_SPGIST_VACUUM_ROOT =     0x70,
    XLOG_SPGIST_VACUUM_REDIRECT = 0x80,

// bitmap
    XLOG_BITMAP_INSERT_LOVITEM =          0x20,
    XLOG_BITMAP_INSERT_META =             0x30,
    XLOG_BITMAP_INSERT_BITMAP_LASTWORDS = 0x40,

    XLOG_BITMAP_INSERT_WORDS = 0x50,

    XLOG_BITMAP_UPDATEWORD =  0x70,
    XLOG_BITMAP_UPDATEWORDS = 0x80,

// appendonly
    XLOG_APPENDONLY_INSERT =   0x00,
    XLOG_APPENDONLY_TRUNCATE = 0x10,
};

typedef struct WalInterface
{
    uint16 header_magic;
    uint32 headerSize;
    void (*validXLogRecordHeader)(const XLogRecordBase *record);
    void (*validXLogRecord)(const XLogRecordBase *record);
    bool (*xLogRecordIsWalSwitch)(const XLogRecordBase *record);
    void (*xLogRecordFilter)(XLogRecordBase *record);
} WalInterface;

#endif // COMMON_WALFILTER_POSTGRESCOMMON_H
