#include "build.auto.h"

#include "common/log.h"
#include "common/partialRestore.h"
#include "postgres/interface/crc32.h"
#include "recordProcessGPDB7.h"

static PgPageSize heapPageSizeGPDB7 = 0;

enum
{
    RM7_SMGR_ID = 2,
    RM7_HEAP2_ID = 9,
    RM7_BTREE_ID = 11,
    RM7_GIN_ID = 13,
    RM7_GIST_ID = 14,
    RM7_SEQ_ID = 15,
    RM7_BITMAP_ID = 22,
    RM7_APPEND_ONLY_ID = 24,
    RM_MAX_ID = RM7_APPEND_ONLY_ID
};

/*
 * Replication origin id - this is located in this file to avoid having to
 * include origin.h in a bunch of xlog related places.
 */
typedef uint16 RepOriginId;

typedef struct XLogRecordDataHeaderShort
{
    uint8 id;                   /* XLR_BLOCK_ID_DATA_SHORT */
    uint8 data_length;          /* number of payload bytes */
} __attribute__((packed)) XLogRecordDataHeaderShort;

typedef struct XLogRecordDataHeaderLong
{
    uint8 id;                   /* XLR_BLOCK_ID_DATA_LONG */
    uint32 data_length;         /* number of payload bytes */
} __attribute__((packed)) XLogRecordDataHeaderLong;

static void
XLogRecordGPDB7ToLog(const XLogRecordGPDB7 *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog,
        "{xl_tot_len: %u, xl_xid: %u, xl_prev: %" PRIu64 ", xl_info: %u, xl_rmid: %u, xl_crc: %u}",
        this->xl_tot_len,
        this->xl_xid,
        this->xl_prev,
        this->xl_info,
        this->xl_rmid,
        this->xl_crc
        );
}

#define FUNCTION_LOG_XLOG_RECORD_GPDB7_TYPE                                                                                        \
    XLogRecordGPDB7 *
#define FUNCTION_LOG_XLOG_RECORD_GPDB7_FORMAT(value, buffer, bufferSize)                                                           \
    FUNCTION_LOG_OBJECT_FORMAT(value, XLogRecordGPDB7ToLog, buffer, bufferSize)

static void
validXLogRecordHeaderGPDB7(const XLogRecordBase *recordBase)
{
    const XLogRecordGPDB7 *const record = (const XLogRecordGPDB7 *const) recordBase;

    if (record->xl_tot_len < sizeof(XLogRecordGPDB7))
    {
        THROW_FMT(FormatError, "invalid record length: wanted %zu, got %u", sizeof(XLogRecordGPDB7), record->xl_tot_len);
    }
    if (record->xl_rmid > RM_MAX_ID)
    {
        THROW_FMT(FormatError, "invalid resource manager ID");
    }
}

FN_EXTERN pg_crc32
xLogRecordChecksumGPDB7(const XLogRecordGPDB7 *const record)
{
    uint32 crc = crc32cInit();

    /* Calculate the CRC */
    crc = crc32cComp(crc, ((unsigned char *) record) + sizeof(XLogRecordGPDB7), record->xl_tot_len - sizeof(XLogRecordGPDB7));
    /* include the record header last */
    crc = crc32cComp(crc, (unsigned char *) record, offsetof(XLogRecordGPDB7, xl_crc));
    return crc32cFinish(crc);
}

static void
validXLogRecordGPDB7(const XLogRecordBase *const recordBase)
{
    const XLogRecordGPDB7 *const record = (const XLogRecordGPDB7 *const) recordBase;

    if (record->xl_crc != xLogRecordChecksumGPDB7(record))
    {
        THROW_FMT(FormatError, "incorrect resource manager data checksum in record");
    }
}

static bool
xLogRecordIsWalSwitchGPDB7(const XLogRecordBase *recordBase)
{
    const XLogRecordGPDB7 *const record = (const XLogRecordGPDB7 *const) recordBase;
    return record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH;
}

static const RelFileNode *
getRelFileNodeFromMainData(const XLogRecordGPDB7 *const record, const void *const mainData)
{
    uint8 info = record->xl_info & (uint8) ~XLR_INFO_MASK;
    switch (record->xl_rmid)
    {
        case RM7_SMGR_ID:
            switch (info)
            {
                case XLOG_SMGR_CREATE:
                    return (const RelFileNode *) mainData;

                case XLOG_SMGR_TRUNCATE:
                {
                    const xl_smgr_truncate *const xlrec = mainData;
                    return &xlrec->rnode;
                }

                default:
                    THROW_FMT(FormatError, "unknown Storage record: %" PRIu8, info);
            }

        case RM7_HEAP2_ID:
            switch (info)
            {
                case XLOG_HEAP2_CLEANUP_INFO:
                    return (const RelFileNode *) mainData;

                case XLOG_HEAP2_NEW_CID:
                {
                    const xl_heap_new_cid *const xlrec = mainData;
                    return &xlrec->target;
                }

                default:
                    break;
            }
            break;

        case RM7_BTREE_ID:
            if (info == XLOG_BTREE_REUSE_PAGE)
                return (const RelFileNode *) mainData;
            break;

        case RM7_GIN_ID:
            switch (info)
            {
                case XLOG_GIN_SPLIT:
                case XLOG_GIN_UPDATE_META_PAGE:
                    return (const RelFileNode *) mainData;

                default:
                    break;
            }
            break;

        case RM7_GIST_ID:
            if (info == XLOG_GIST_PAGE_REUSE)
                return (const RelFileNode *) mainData;
            break;

        case RM7_SEQ_ID:
            if (info == XLOG_SEQ_LOG)
                return (const RelFileNode *) mainData;
            THROW_FMT(FormatError, "unknown Sequence: %" PRIu8, info);

        case RM7_BITMAP_ID:
            switch (info)
            {
                case XLOG_BITMAP_INSERT_WORDS:
                case XLOG_BITMAP_UPDATEWORD:
                case XLOG_BITMAP_UPDATEWORDS:
                case XLOG_BITMAP_INSERT_LOVITEM:
                case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
                case XLOG_BITMAP_INSERT_META:
                    return (const RelFileNode *) mainData;

                default:
                    break;
            }
            break;

        case RM7_APPEND_ONLY_ID:
            switch (info)
            {
                case XLOG_APPENDONLY_INSERT:
                case XLOG_APPENDONLY_TRUNCATE:
                    return (const RelFileNode *) mainData;

                default:
                    THROW_FMT(FormatError, "unknown Appendonly: %" PRIu8, info);
            }
    }

    return NULL;
}

static List *
getRelFileNodes(XLogRecordGPDB7 *const record)
{
    #define COPY_HEADER_FIELD(_dst)                     \
    do                                                  \
    {                                                   \
        if (record->xl_tot_len - offset < sizeof(_dst)) \
            return NULL;                                \
        memcpy(&_dst, ptr + offset, sizeof(_dst));      \
        offset += sizeof(_dst);                         \
    }                                                   \
    while(0)

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(XLOG_RECORD_GPDB7, record);
    FUNCTION_LOG_END();

    ASSERT(heapPageSizeGPDB7 != 0);

    /* Decode the headers */
    size_t datatotal = 0;
    int maxBlockId = -1;
    const uint8 *ptr = (uint8 *) record;
    size_t offset = sizeof(XLogRecordGPDB7);

    RelFileNode *relFileNode = NULL;
    List *result = lstNewP(sizeof(RelFileNode));
    uint32 mainDataSize = 0;
    // Read the headers only.
    // The next header exists if the remaining data is greater than the total size of all backup blocks and main data.
    while (record->xl_tot_len - offset > datatotal)
    {
        uint8 block_id;
        COPY_HEADER_FIELD(block_id);

        if (block_id == XLR_BLOCK_ID_DATA_SHORT)
        {
            uint8 size;
            COPY_HEADER_FIELD(size);
            mainDataSize = size;
            break;              /* by convention, the main data fragment is
                                 * always last */
        }
        if (block_id == XLR_BLOCK_ID_DATA_LONG)
        {
            COPY_HEADER_FIELD(mainDataSize);
            break;              /* by convention, the main data fragment is
                                 * always last */
        }
        if (block_id == XLR_BLOCK_ID_ORIGIN)
        {
            ptr += sizeof(RepOriginId);
            continue;
        }
        if (block_id > XLR_MAX_BLOCK_ID)
        {
            THROW_FMT(FormatError, "invalid block_id %" PRIu8, block_id);
        }

        /* XLogRecordBlockHeader */

        if (block_id <= maxBlockId)
        {
            THROW_FMT(FormatError, "out-of-order block_id %" PRIu8, block_id);
        }
        maxBlockId = block_id;

        uint8 fork_flags;
        COPY_HEADER_FIELD(fork_flags);

        uint16 data_len;
        COPY_HEADER_FIELD(data_len);
        /* cross-check that the HAS_DATA flag is set if data_length > 0 */
        if (fork_flags & BKPBLOCK_HAS_DATA)
        {
            if (data_len == 0)
                THROW_FMT(FormatError, "BKPBLOCK_HAS_DATA set, but no data included");
        }
        else if (data_len != 0)
            THROW_FMT(FormatError, "BKPBLOCK_HAS_DATA not set, but data length is %" PRIu16, data_len);

        datatotal += data_len;

        if (fork_flags & BKPBLOCK_HAS_IMAGE)
        {
            uint16 bimg_len;
            uint16 hole_offset;
            uint8 bimg_info;

            COPY_HEADER_FIELD(bimg_len);
            COPY_HEADER_FIELD(hole_offset);
            COPY_HEADER_FIELD(bimg_info);

            uint16 hole_length;
            if (bimg_info & BKPIMAGE_IS_COMPRESSED)
            {
                if (bimg_info & BKPIMAGE_HAS_HOLE)
                    COPY_HEADER_FIELD(hole_length);
                else
                    hole_length = 0;
            }
            else
                hole_length = (uint16) (heapPageSizeGPDB7 - bimg_len);
            datatotal += bimg_len;

            /*
             * cross-check that hole_offset > 0, hole_length > 0 and
             * bimg_len < BLCKSZ if the HAS_HOLE flag is set.
             */
            if ((bimg_info & BKPIMAGE_HAS_HOLE) &&
                (hole_offset == 0 ||
                 hole_length == 0 ||
                 bimg_len == heapPageSizeGPDB7))
            {
                THROW_FMT(FormatError,
                          "BKPIMAGE_HAS_HOLE set, but hole offset %" PRIu16 " length %" PRIu16 " block image length %" PRIu16,
                          hole_offset,
                          hole_length,
                          bimg_len);
            }

            /*
             * cross-check that hole_offset == 0 and hole_length == 0 if
             * the HAS_HOLE flag is not set.
             */
            if (!(bimg_info & BKPIMAGE_HAS_HOLE) &&
                (hole_offset != 0 || hole_length != 0))
            {
                THROW_FMT(FormatError,
                          "BKPIMAGE_HAS_HOLE not set, but hole offset %" PRIu16 " length %" PRIu16, hole_offset, hole_length);
            }

            /*
             * cross-check that bimg_len < BLCKSZ if the IS_COMPRESSED
             * flag is set.
             */
            if ((bimg_info & BKPIMAGE_IS_COMPRESSED) && bimg_len == heapPageSizeGPDB7)
            {
                THROW_FMT(FormatError, "BKPIMAGE_IS_COMPRESSED set, but block image length %" PRIu16, bimg_len);
            }
        }
        if (fork_flags & BKPBLOCK_SAME_REL)
        {
            if (relFileNode == NULL)
            {
                THROW_FMT(FormatError, "BKPBLOCK_SAME_REL set but no previous rel");
            }
        }
        else
        {
            relFileNode = (RelFileNode *) (ptr + offset);
            lstAdd(result, relFileNode);
            offset += sizeof(RelFileNode);
        }
        offset += sizeof(BlockNumber);
    }
#undef COPY_HEADER_FIELD

    if (mainDataSize)
    {
        ASSERT(record->xl_tot_len > mainDataSize);
        // Main data is always the last one
        const void *const mainDataPtr = ((uint8 *) record) + record->xl_tot_len - mainDataSize;
        const RelFileNode *const mainDataRelFileNode = getRelFileNodeFromMainData(record, mainDataPtr);
        if (mainDataRelFileNode)
            lstAdd(result, mainDataRelFileNode);
    }

    FUNCTION_LOG_RETURN(LIST, result);
}

static void
overrideXLogRecordBody(XLogRecordGPDB7 *const record)
{
    uint8 *recordData = XLogRecordData(record);
    if (record->xl_tot_len - sizeof(XLogRecordGPDB7) - sizeof(XLogRecordDataHeaderShort) <= UINT8_MAX)
    {
        *((XLogRecordDataHeaderShort *) recordData) = (XLogRecordDataHeaderShort){
            XLR_BLOCK_ID_DATA_SHORT,
            (uint8) (record->xl_tot_len - sizeof(XLogRecordGPDB7) - sizeof(XLogRecordDataHeaderShort))
        };
    }
    else
    {
        *((XLogRecordDataHeaderLong *) recordData) = (XLogRecordDataHeaderLong){
            XLR_BLOCK_ID_DATA_LONG,
            (uint32) (record->xl_tot_len - sizeof(XLogRecordGPDB7) - sizeof(XLogRecordDataHeaderLong))
        };
    }
}

static void
filterRecordGPDB7(XLogRecordBase *const recordBase)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
    FUNCTION_LOG_END();

    XLogRecordGPDB7 *const record = (XLogRecordGPDB7 *const) recordBase;

    if (record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_NOOP)
        goto end;

    List *const nodes = getRelFileNodes(record);
    if (lstEmpty(nodes))
    {
        lstFree(nodes);
        goto end;
    }

    bool hasPassFilter = false;
    bool hasNotPassFilter = false;
    List *notPassFilterList = lstNewP(sizeof(RelFileNode));
    for (unsigned int i = 0; i < lstSize(nodes); i++)
    {
        const RelFileNode *const relFileNode = lstGet(nodes, i);
        if (isRelationNeeded(relFileNode->dbNode, relFileNode->spcNode, relFileNode->relNode))
        {
            hasPassFilter = true;
        }
        else
        {
            lstAdd(notPassFilterList, relFileNode);
            hasNotPassFilter = true;
        }
    }
    lstFree(nodes);

    // For now, I can't find a real-life example of such a case, so let's handle it the safest way.
    // If a record has two and more relFileNodes and one of them is passing a filter while the others are not, then throw an error.
    // Since:
    //  1. If we pass that record, we may get an error during recovery because one of the files will be missing.
    //  2. If we filter out that record, we can get an inconsistent database.
    if (hasPassFilter && hasNotPassFilter)
    {
        ASSERT(!lstEmpty(notPassFilterList));
        String *errMessage = strCatZ(strNew(), "The following RefFileNodes cannot be filtered out because they are in the same"
                                     " XLogRecord as the RefFileNode that passes the filter. [");
        for (unsigned int i = 0; i < lstSize(notPassFilterList); i++)
        {
            if (i > 0)
                strCatZ(errMessage, ", ");
            const RelFileNode *const relFileNode = lstGet(notPassFilterList, i);
            strCatFmt(errMessage, "{%u, %u, %u}", relFileNode->spcNode, relFileNode->dbNode, relFileNode->relNode);
        }
        strCatZ(errMessage, "].\nHINT: Add these RelFileNodes to your filter.");
        THROW(ConfigError, strZ(errMessage));
    }
    lstFree(notPassFilterList);
    if (hasPassFilter)
        goto end;
    ASSERT(hasNotPassFilter);

    overrideXLogRecordBody(record);
    record->xl_info = XLOG_NOOP;
    record->xl_rmid = RM_XLOG_ID;
    record->xl_crc = xLogRecordChecksumGPDB7(record);
end:
    FUNCTION_LOG_RETURN_VOID();
}

FN_EXTERN WalInterface
getWalInterfaceGPDB7(PgPageSize heapPageSize)
{
    ASSERT(pgPageSizeValid(heapPageSize));
    heapPageSizeGPDB7 = heapPageSize;

    return (WalInterface){
               0xD101,
               sizeof(XLogRecordGPDB7),
               validXLogRecordHeaderGPDB7,
               validXLogRecordGPDB7,
               xLogRecordIsWalSwitchGPDB7,
               filterRecordGPDB7
    };
}
