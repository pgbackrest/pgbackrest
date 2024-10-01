#include "build.auto.h"

#include "walFilter.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/object.h"

#include "common/compress/helper.h"
#include "common/partialRestore.h"
#include "config/config.h"
#include "postgres/interface/crc32.h"
#include "postgres/version.h"
#include "postgresCommon.h"
#include "storage/helper.h"
#include "versions/recordProcessGPDB6.h"

#define WAL_FILTER_TYPE STRID5("wal-fltr", 0x95186db0370)

typedef enum
{
    noStep, // This means that the process of reading the record is in an uninterrupted state.
    stepBeginOfRecord,
    stepReadHeader,
    stepReadBody,
} ReadStep;

typedef enum
{
    ReadRecordNeedBuffer,
    ReadRecordSuccess
} ReadRecordStatus;

typedef struct WalInterface
{
    unsigned int pgVersion;
    StringId fork;

    uint16_t header_magic;
    void (*validXLogRecordHeader)(const XLogRecord *record, PgPageSize heapPageSize);
    void (*validXLogRecord)(const XLogRecord *record, PgPageSize heapPageSize);
    pg_crc32 (*xLogRecordChecksum)(const XLogRecord *record, PgPageSize heapPageSize);
} WalInterface;

static WalInterface interfaces[] = {
    {
        PG_VERSION_94,
        CFGOPTVAL_FORK_GPDB,
        GPDB6_XLOG_PAGE_MAGIC,
        validXLogRecordHeaderGPDB6,
        validXLogRecordGPDB6,
        xLogRecordChecksumGPDB6
    }
};

typedef struct WalFilter
{
    ReadStep currentStep;

    PgPageSize heapPageSize;
    PgPageSize walPageSize;
    uint32_t segSize;

    bool isBegin;

    size_t pageOffset;
    size_t inputOffset;
    XLogRecPtr recPtr;

    XLogPageHeaderData *currentPageHeader;

    XLogRecord *record;
    uint32_t recBufSize;
    // Size of header of the current record on the current page
    size_t headerSize;
    // How many bytes we read from this record
    size_t gotLen;
    // Total size of the current record on current page
    size_t totLen;

    List *pageHeaders;

    WalInterface *walInterface;

    const ArchiveGetFile *archiveInfo;

    // Records count for debug
    uint32_t recordNum;

    bool done;
    bool inputSame;
    bool isSwitchWal;
} WalFilterState;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static void
walFilterToLog(const WalFilterState *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog,
        "{recordNum: %u, step: %u isBegin: %s, pageOffset: %zu, inputOffset: %zu, recBufSize: %u, gotLen: %zu, totLen: %zu}",
        this->recordNum,
        this->currentStep,
        this->isBegin ? "true" : "false",
        this->pageOffset,
        this->inputOffset,
        this->recBufSize,
        this->gotLen,
        this->totLen
        );
}

#define FUNCTION_LOG_WAL_FILTER_TYPE                                                                                               \
    WalFilterState *
#define FUNCTION_LOG_WAL_FILTER_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(value, walFilterToLog, buffer, bufferSize)

static inline void
checkOutputSize(Buffer *const output, const size_t size)
{
    if (bufRemains(output) <= size)
    {
        bufResize(output, bufUsed(output) + size);
    }
}

// Save next page addr from input buffer to page field. If the input buffer is exhausted, remember the current step and returns
// false. In this case, we should exit the process function to get a new input buffer. Returns true on success page read.
static inline bool
getNextPage(WalFilterState *const this, const Buffer *const input)
{
    if (this->inputOffset >= bufUsed(input))
    {
        ASSERT(this->currentStep != noStep);
        this->inputOffset = 0;
        this->inputSame = false;
        return false;
    }

    this->currentStep = noStep;
    this->currentPageHeader = (XLogPageHeaderData *) (bufPtrConst(input) + this->inputOffset);
    this->pageOffset = XLogPageHeaderSize(this->currentPageHeader);
    this->inputOffset += this->walPageSize;

    // Make sure that WAL belongs to supported Postgres version, since magic value is different in different versions.
    if (this->currentPageHeader->xlp_magic != this->walInterface->header_magic)
    {
        THROW_FMT(FormatError, "%s - wrong page magic", strZ(pgLsnToStr(this->recPtr)));
    }

    lstAdd(this->pageHeaders, this->currentPageHeader);

    return true;
}

static inline uint32_t
getRecordSize(const unsigned char *const buffer)
{
    return ((XLogRecord *) (buffer))->xl_tot_len;
}

// Returns ReadRecordSuccess on success record read and returns ReadRecordNeedBuffer if a new input buffer is needed to continue
// reading.
static ReadRecordStatus
readRecord(WalFilterState *const this, const Buffer *const input)
{
    // Go back to the place where the reading was interrupted due to the exhaustion of the input buffer.
    switch (this->currentStep)
    {
        case noStep:
            // noting to do
            break;

        case stepBeginOfRecord:
            goto stepBeginOfRecord;

        case stepReadHeader:
            goto stepReadHeader;

        case stepReadBody:
            goto stepReadBody;
    }

    if (this->pageOffset == 0 || this->pageOffset == this->walPageSize)
    {
        this->currentStep = stepBeginOfRecord;
stepBeginOfRecord:
        if (!getNextPage(this, input))
        {
            return ReadRecordNeedBuffer;
        }

        if (this->isBegin)
        {
            this->isBegin = false;
            // There may be an unfinished record from the previous file at the beginning of the file. Just skip it.
            // We should have handled it elsewhere.
            if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD &&
                !(this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD))
            {
                this->pageOffset += MAXALIGN(this->currentPageHeader->xlp_rem_len);
            }
        }
        else if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD)
        {
            THROW_FMT(FormatError, "%s - should not be XLP_FIRST_IS_CONTRECORD", strZ(pgLsnToStr(this->recPtr)));
        }
    }

    // Record header can be split between pages but first field xl_tot_len is always on single page
    uint32_t record_size = getRecordSize(((unsigned char *) this->currentPageHeader) + this->pageOffset);

    if (this->recBufSize < record_size)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->record = memResize(this->record, record_size);
        }
        MEM_CONTEXT_OBJ_END();
        this->recBufSize = record_size;
    }

    memcpy(
        this->record,
        ((unsigned char *) this->currentPageHeader) + this->pageOffset,
        Min(SizeOfXLogRecord, this->walPageSize - this->pageOffset));

    this->totLen = this->record->xl_tot_len;

    // If header is split read rest of the header from next page
    if (SizeOfXLogRecord > this->walPageSize - this->pageOffset)
    {
        this->gotLen = this->walPageSize - this->pageOffset;
        this->currentStep = stepReadHeader;
stepReadHeader:
        if (!getNextPage(this, input))
        {
            return ReadRecordNeedBuffer;
        }

        if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD)
        {
            // This record has been overwritten.
            // Write to the output what we managed to read as is, skipping filtering.
            return ReadRecordSuccess;
        }

        if (!(this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD))
        {
            THROW_FMT(FormatError, "%s - should be XLP_FIRST_IS_CONTRECORD", strZ(pgLsnToStr(this->recPtr)));
        }

        memcpy(
            ((char *) this->record) + this->gotLen,
            ((unsigned char *) this->currentPageHeader) + this->pageOffset,
            SizeOfXLogRecord - this->gotLen);
        this->totLen -= this->gotLen;
        this->headerSize = SizeOfXLogRecord - this->gotLen;
    }
    else
    {
        this->headerSize = SizeOfXLogRecord;
    }
    this->gotLen = SizeOfXLogRecord;

    this->walInterface->validXLogRecordHeader(this->record, this->heapPageSize);
    // Read rest of the record on this page
    size_t toRead = Min(this->record->xl_tot_len - SizeOfXLogRecord, this->walPageSize - this->pageOffset - SizeOfXLogRecord);
    memcpy(
        (void *) XLogRecGetData(this->record),
        ((unsigned char *) this->currentPageHeader) + this->pageOffset + this->headerSize,
        toRead);
    this->gotLen += toRead;

    // Move pointer to the next record on the page
    this->pageOffset += MAXALIGN(this->totLen);

    // Rest of the record data is on the next page
    while (this->gotLen != this->record->xl_tot_len)
    {
        this->currentStep = stepReadBody;
stepReadBody:
        if (!getNextPage(this, input))
        {
            return ReadRecordNeedBuffer;
        }

        if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD)
        {
            // This record has been overwritten.
            // Write to the output what we managed to read as is, skipping filtering.
            return ReadRecordSuccess;
        }

        if (!(this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD))
        {
            THROW_FMT(FormatError, "%s - should be XLP_FIRST_IS_CONTRECORD", strZ(pgLsnToStr(this->recPtr)));
        }

        if (this->currentPageHeader->xlp_rem_len == 0 ||
            this->totLen != (this->currentPageHeader->xlp_rem_len + this->gotLen))
        {
            THROW_FMT(FormatError, "%s - invalid contrecord length: expect: %zu, get %u", strZ(pgLsnToStr(this->recPtr)),
                      this->record->xl_tot_len - this->gotLen, this->currentPageHeader->xlp_rem_len);
        }

        size_t to_write = Min(this->currentPageHeader->xlp_rem_len, this->walPageSize - this->pageOffset);
        memcpy(((char *) this->record) + this->gotLen, ((unsigned char *) this->currentPageHeader) + this->pageOffset, to_write);
        this->pageOffset += MAXALIGN(to_write);
        this->gotLen += to_write;
    }
    this->walInterface->validXLogRecord(this->record, this->heapPageSize);

    if (this->record->xl_rmid == RM_XLOG_ID && this->record->xl_info == XLOG_SWITCH)
    {
        this->isSwitchWal = true;
    }
    this->recordNum++;
    return ReadRecordSuccess;
}

static void
filterRecord(WalFilterState *const this)
{
    const RelFileNode *const node = getRelFileNodeGPDB6(this->record);
    if (!node)
    {
        return;
    }

    bool isPassTheFilter;

    // isRelationNeeded can allocate memory on the first call. Therefore, we switch the context.
    MEM_CONTEXT_OBJ_BEGIN(this)
    isPassTheFilter = isRelationNeeded(node->dbNode, node->spcNode, node->relNode);
    MEM_CONTEXT_OBJ_END();

    if (isPassTheFilter)
    {
        return;
    }

    this->record->xl_rmid = RM_XLOG_ID;
    // Save 4 least significant bits which represent backup blocks flags.
    this->record->xl_info = (uint8_t) (XLOG_NOOP | (this->record->xl_info & XLR_INFO_MASK));
    this->record->xl_crc = this->walInterface->xLogRecordChecksum(this->record, this->heapPageSize);
}

static void
writeRecord(WalFilterState *const this, Buffer *const output, const unsigned char *recordData)
{
    uint32_t header_i = 0;
    if (this->recPtr % this->walPageSize == 0)
    {
        ASSERT(!lstEmpty(this->pageHeaders));
        const XLogPageHeaderData *const header = lstGet(this->pageHeaders, header_i);
        const size_t to_write = XLogPageHeaderSize(header);
        header_i++;

        bufCatC(output, (const unsigned char *) header, 0, to_write);

        this->recPtr += to_write;
    }

    size_t wrote = 0;
    while (this->gotLen != wrote)
    {
        const size_t space_on_page = this->walPageSize - (size_t) (this->recPtr % this->walPageSize);
        const size_t to_write = Min(space_on_page, this->gotLen - wrote);

        bufCatC(output, recordData, wrote, to_write);

        wrote += to_write;
        this->recPtr += to_write;

        // write header
        if (header_i < lstSize(this->pageHeaders))
        {
            bufCatC(output, lstGet(this->pageHeaders, header_i), 0, SizeOfXLogShortPHD);

            this->recPtr += SizeOfXLogShortPHD;
            header_i++;
        }
    }

    const size_t alignSize = MAXALIGN(this->gotLen) - this->gotLen;
    checkOutputSize(output, alignSize);
    memset(bufRemainsPtr(output), 0, alignSize);
    bufUsedInc(output, alignSize);
    this->recPtr += alignSize;

    this->gotLen = 0;
}

static const StorageRead *
getNearWal (WalFilterState *const this, bool isNext)
{
    const String *walSegment = NULL;
    const TimeLineID timeLine = this->currentPageHeader->xlp_tli;
    uint64_t segno = this->currentPageHeader->xlp_pageaddr / this->segSize;
    const String *const path = strNewFmt("%08X%08X", timeLine, (uint32) (segno / XLogSegmentsPerXLogId(this->segSize)));

    const StringList *const segmentList = storageListP(
        storageRepoIdx(this->archiveInfo->repoIdx),
        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(this->archiveInfo->archiveId), strZ(path)),
        .expression = strNewFmt("^[0-f]{24}-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$"));

    if (strLstEmpty(segmentList))
    {
        // an exotic case where we couldn't even find the current file.
        THROW(FormatError, "no WAL files were found in the repository");
    }

    uint64_t segnoDiff = UINT64_MAX;
    for (uint32_t i = 0; i < strLstSize(segmentList); i++)
    {
        const String *const file = strSubN(strLstGet(segmentList, i), 0, 24);
        TimeLineID tli;
        uint64_t fileSegNo = 0;
        XLogFromFileName(strZ(file), &tli, &fileSegNo, this->segSize);

        if (isNext)
        {
            if (fileSegNo - segno < segnoDiff && fileSegNo > segno)
            {
                segnoDiff = fileSegNo - segno;
                walSegment = strLstGet(segmentList, i);
            }
        }
        else
        {
            if (segno - fileSegNo < segnoDiff && fileSegNo < segno)
            {
                segnoDiff = segno - fileSegNo;
                walSegment = strLstGet(segmentList, i);
            }
        }
    }

    // current file is oldest/newest
    if (segnoDiff == UINT64_MAX)
    {
        return NULL;
    }

    const bool compressible =
        this->archiveInfo->cipherType == cipherTypeNone && compressTypeFromName(this->archiveInfo->file) == compressTypeNone;

    const StorageRead *const storageRead = storageNewReadP(
        storageRepoIdx(this->archiveInfo->repoIdx),
        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(this->archiveInfo->archiveId), strZ(walSegment)),
        .compressible = compressible);

    buildArchiveGetPipeLine(ioReadFilterGroup(storageReadIo(storageRead)), this->archiveInfo);
    return storageRead;
}

static bool
readBeginOfRecord(WalFilterState *const this)
{
    bool result = false;
    MEM_CONTEXT_TEMP_BEGIN();

    const StorageRead *const storageRead = getNearWal(this, false);

    if (storageRead == NULL)
    {
        goto end;
    }

    ioReadOpen(storageReadIo(storageRead));
    this->isBegin = true;
    this->inputOffset = 0;
    this->pageOffset = 0;

    Buffer *const buffer = bufNew(this->walPageSize);
    size_t size = ioRead(storageReadIo(storageRead), buffer);
    bufUsedSet(buffer, size);

    while (!ioReadEof(storageReadIo(storageRead)))
    {
        if (readRecord(this, buffer) == ReadRecordNeedBuffer)
        {
            bufUsedZero(buffer);
            size = ioRead(storageReadIo(storageRead), buffer);
            bufUsedSet(buffer, size);
        }
        lstClearFast(this->pageHeaders);
    }
    // If xl_info and xl_rmid is in prev file then nothing to do
    result = this->gotLen < offsetof(XLogRecord, xl_rmid) + SIZE_OF_STRUCT_MEMBER(XLogRecord, xl_rmid);

    ioReadClose(storageReadIo(storageRead));
end:
    MEM_CONTEXT_TEMP_END();
    return result;
}

static void
getEndOfRecord(WalFilterState *const this)
{
    MEM_CONTEXT_TEMP_BEGIN();

    const StorageRead *const storageRead = getNearWal(this, true);

    if (storageRead == NULL)
    {
        THROW_FMT(FormatError, "The file with the end of the %s record is missing", strZ(pgLsnToStr(this->recPtr)));
    }

    ioReadOpen(storageReadIo(storageRead));

    Buffer *const buffer = bufNew(this->walPageSize);
    size_t size = ioRead(storageReadIo(storageRead), buffer);
    bufUsedSet(buffer, size);
    while (readRecord(this, buffer) == ReadRecordNeedBuffer)
    {
        if (ioReadEof(storageReadIo(storageRead)))
        {
            THROW_FMT(FormatError, "%s - Unexpected WAL end", strZ(pgLsnToStr(this->recPtr)));
        }

        bufUsedZero(buffer);
        size = ioRead(storageReadIo(storageRead), buffer);
        bufUsedSet(buffer, size);
    }
    ioReadClose(storageReadIo(storageRead));
    MEM_CONTEXT_TEMP_END();
}

static void
walFilterProcess(THIS_VOID, const Buffer *const input, Buffer *const output)
{
    THIS(WalFilterState);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(WAL_FILTER, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL);
    ASSERT(input == NULL || bufUsed(input) % this->walPageSize == 0);

    // Avoid creating local variables before the record is fully read,
    // since if the input buffer is exhausted, we can exit the function.

    if (input == NULL)
    {
        // We have an incomplete record at the end
        if (this->currentStep != noStep)
        {
            const size_t size_on_page = this->gotLen;

            // if xl_info and xl_rmid of the header is in current file then read end of record from next file if it exits
            if (this->gotLen >= offsetof(XLogRecord, xl_rmid) + SIZE_OF_STRUCT_MEMBER(XLogRecord, xl_rmid))
            {
                getEndOfRecord(this);
                filterRecord(this);
            }

            bufCatC(output, (const unsigned char *) this->record, 0, size_on_page);
        }
        this->done = true;
        goto end;
    }

    if (this->isBegin)
    {
        this->isBegin = false;

#ifdef DEBUG
        bool ret =
#endif
        getNextPage(this, input);
        ASSERT(ret);
        this->recPtr = this->currentPageHeader->xlp_pageaddr;
        if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD &&
            !(this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD))
        {
            if (readBeginOfRecord(this))
            {
                // Remember how much we read from the prev file in order to skip this size when writing.
                const size_t offset = this->gotLen;
                this->inputOffset = 0;
                lstClearFast(this->pageHeaders);
                if (readRecord(this, input) == ReadRecordNeedBuffer)
                {
                    // Since we are at the very beginning of the file, let's assume that the current input buffer is enough to fully
                    // read this record.
                    THROW_FMT(FormatError, "%s - record is too big", strZ(pgLsnToStr(this->recPtr)));
                }
                filterRecord(this);

                ASSERT(offset <= 8);
                this->gotLen -= offset;

                ASSERT(this->recPtr == this->currentPageHeader->xlp_pageaddr);
                writeRecord(this, output, ((const unsigned char *) this->record) + offset);
                this->inputSame = true;
                lstClearFast(this->pageHeaders);
                goto end;
            }

            this->inputOffset = 0;
            getNextPage(this, input);
            ASSERT(this->recPtr == this->currentPageHeader->xlp_pageaddr);
            bufCatC(output, (const unsigned char *) this->currentPageHeader, 0, SizeOfXLogLongPHD);
            this->recPtr += SizeOfXLogLongPHD;

            lstClearFast(this->pageHeaders);

            // The header that needs to be modified is in another file or not exists. Just copy it as is.
            bufCatC(output, bufPtrConst(input), this->pageOffset, MAXALIGN(this->currentPageHeader->xlp_rem_len));

            this->pageOffset += MAXALIGN(this->currentPageHeader->xlp_rem_len);
            this->recPtr += MAXALIGN(this->currentPageHeader->xlp_rem_len);
        }
    }

    // When meeting wal switch record, we write the rest of the file as is.
    if (this->isSwitchWal)
    {
        if (this->pageOffset != 0)
        {
            // Copy the rest of the current page
            bufCatC(output, (const unsigned char *) this->currentPageHeader, this->pageOffset, this->walPageSize - this->pageOffset);
            this->pageOffset = 0;
        }

        if (bufUsed(input) > this->inputOffset)
            bufCatC(output, bufPtrConst(input), this->inputOffset, bufUsed(input) - this->inputOffset);
        this->inputOffset = 0;
        this->inputSame = false;
        goto end;
    }

    if (readRecord(this, input) == ReadRecordSuccess)
    {
        // In the case of overwrite contrecord, we do not need to try to filter it, since the record may not have a body at all.
        if (this->gotLen == this->record->xl_tot_len)
        {
            filterRecord(this);
        }

        writeRecord(this, output, (const unsigned char *) this->record);

        this->inputSame = true;
        lstClearFast(this->pageHeaders);
    }
end:
    FUNCTION_LOG_RETURN_VOID();
}

static bool
WalFilterDone(const THIS_VOID)
{
    THIS(const WalFilterState);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(WAL_FILTER, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, this->done);
}

static bool
WalFilterInputSame(const THIS_VOID)
{
    THIS(const WalFilterState);
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(WAL_FILTER, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

FN_EXTERN IoFilter *
walFilterNew(const PgControl pgControl, const ArchiveGetFile *const archiveInfo)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PG_CONTROL, pgControl);
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(WalFilterState, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (WalFilterState){
            .isBegin = true,
            .record = memNew(pgControl.pageSize),
            .recBufSize = pgControl.pageSize,
            .pageHeaders = lstNewP(SizeOfXLogLongPHD),
            .archiveInfo = archiveInfo,
            .heapPageSize = pgControl.pageSize,
            .walPageSize = pgControl.walPageSize,
            .segSize = pgControl.walSegmentSize,
        };

        StringId fork = cfgOptionStrId(cfgOptFork);
        for (unsigned int i = 0; i < LENGTH_OF(interfaces); i++)
        {
            if (interfaces[i].pgVersion == pgControl.version && interfaces[i].fork == fork)
            {
                this->walInterface = &interfaces[i];
                break;
            }
        }
        if (this->walInterface == NULL)
        {
            THROW(VersionNotSupportedError, "WAL filtering is unsupported for this Postgres version");
        }
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            WAL_FILTER_TYPE, this, NULL, .done = WalFilterDone, .inOut = walFilterProcess,
            .inputSame = WalFilterInputSame));
}
