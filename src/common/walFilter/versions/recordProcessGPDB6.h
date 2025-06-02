#ifndef COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H
#define COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H

#include "common/walFilter/postgresCommon.h"

typedef enum ForkNumber
{
    InvalidForkNumber = -1,
    MAIN_FORKNUM = 0,
    FSM_FORKNUM,
    VISIBILITYMAP_FORKNUM,
    INIT_FORKNUM
} ForkNumber;

/*
 * If we backed up any disk blocks with the XLOG record, we use flag bits in
 * xl_info to signal it.  We support backup of up to 4 disk blocks per XLOG
 * record.
 */
#define XLR_MAX_BKP_BLOCKS      4
#define XLR_BKP_BLOCK(iblk)     (0x08 >> (iblk)) /* iblk in 0..3 */

/*
 * The overall layout of an XLOG record is:
 *      Fixed-size header (XLogRecord struct)
 *      rmgr-specific data
 *      BkpBlock
 *      backup block data
 *      BkpBlock
 *      backup block data
 *      ...
 *
 * where there can be zero to four backup blocks (as signaled by xl_info flag
 * bits).  XLogRecord structs always start on MAXALIGN boundaries in the WAL
 * files, and we round up SizeOfXLogRecord so that the rmgr data is also
 * guaranteed to begin on a MAXALIGN boundary.  However, no padding is added
 * to align BkpBlock structs or backup block data.
 *
 * NOTE: xl_len counts only the rmgr data, not the XLogRecord header,
 * and also not any backup blocks.  xl_tot_len counts everything.  Neither
 * length field is rounded up to an alignment boundary.
 */
typedef struct XLogRecordGPDB6
{
    uint32 xl_tot_len;          /* total len of entire record */
    TransactionId xl_xid;       /* xact id */
    uint32 xl_len;              /* total len of rmgr data */
    uint8 xl_info;            /* flag bits, see below */
    RmgrId xl_rmid;             /* resource manager for this record */
    /* 2 bytes of padding here, initialize to zero */
    XLogRecPtr xl_prev;         /* ptr to previous record in log */
    pg_crc32 xl_crc;            /* CRC for this record */

    /* If MAXALIGN==8, there are 4 wasted bytes here */

    /* ACTUAL LOG DATA FOLLOWS AT END OF STRUCT */
} XLogRecordGPDB6;
_Static_assert(
    offsetof(XLogRecordGPDB6, xl_info) / 8 == offsetof(XLogRecordGPDB6, xl_rmid) / 8,
    "The xl_info and xl_rmid fields are in different 8 byte chunks.");
#define XLogRecGetData(record)  ((const unsigned char*) (record) + SizeOfXLogRecordGPDB6)
#define SizeOfXLogRecordGPDB6    MAXALIGN(sizeof(XLogRecordGPDB6))

typedef struct BkpBlock
{
    RelFileNode node;           /* relation containing block */
    ForkNumber fork;            /* fork within the relation */
    BlockNumber block;          /* block number */
    uint16 hole_offset;         /* number of bytes before "hole" */
    uint16 hole_length;         /* number of bytes in "hole" */

    /* ACTUAL BLOCK DATA FOLLOWS AT END OF STRUCT */
} BkpBlock;

FN_EXTERN pg_crc32 xLogRecordChecksumGPDB6(const XLogRecordGPDB6 *record, PgPageSize heapPageSize);
FN_EXTERN WalInterface getWalInterfaceGPDB6(PgPageSize heapPageSize);

#endif // COMMON_WALFILTER_VERSIONS_RECORDPROCESSGPDB6_H
