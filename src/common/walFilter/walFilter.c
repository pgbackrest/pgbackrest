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
#include "versions/recordProcessGPDB7.h"

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

typedef struct WalFilter
{
    ReadStep currentStep;

    PgPageSize walPageSize;
    uint32 segSize;

    bool isBegin;
    // Are we reading remaining data of incomplete record?
    bool isReadOrphanedData;

    // How many bytes of the record are in the previous file
    size_t beginOffset;
    size_t pageOffset;
    size_t inputOffset;
    XLogRecPtr recPtr;

    XLogPageHeaderData *currentPageHeader;

    XLogRecordBase *record;
    uint32 recBufSize;
    // How many bytes we read from this record
    size_t gotLen;

    List *pageHeaders;

    WalInterface walInterface;

    const ArchiveGetFile *archiveInfo;

    // Records count for debug
    uint32 recordNum;

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
        "{recordNum: %u, step: %u isBegin: %s, pageOffset: %zu, inputOffset: %zu, recBufSize: %u, gotLen: %zu}",
        this->recordNum,
        this->currentStep,
        this->isBegin ? "true" : "false",
        this->pageOffset,
        this->inputOffset,
        this->recBufSize,
        this->gotLen
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
        this->inputOffset = 0;
        this->inputSame = false;
        return false;
    }

    this->currentStep = noStep;
    this->currentPageHeader = (XLogPageHeaderData *) (bufPtrConst(input) + this->inputOffset);
    this->pageOffset = XLogPageHeaderSize(this->currentPageHeader);
    this->inputOffset += this->walPageSize;

    // Make sure that WAL belongs to supported Postgres version, since magic value is different in different versions.
    if (this->currentPageHeader->xlp_magic != this->walInterface.header_magic)
    {
        THROW_FMT(FormatError, "%s - wrong page magic", strZ(pgLsnToStr(this->recPtr)));
    }

    lstAdd(this->pageHeaders, this->currentPageHeader);

    return true;
}

static inline uint32
getRecordSize(const unsigned char *const buffer)
{
    return ((XLogRecordBase *) (buffer))->xl_tot_len;
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

    ASSERT(this->pageOffset != 0);
    if (this->pageOffset == this->walPageSize)
    {
        this->currentStep = stepBeginOfRecord;
stepBeginOfRecord:
        if (!getNextPage(this, input))
        {
            return ReadRecordNeedBuffer;
        }

        ASSERT(!this->isBegin);
        if (this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD)
        {
            THROW_FMT(FormatError, "%s - should not be XLP_FIRST_IS_CONTRECORD", strZ(pgLsnToStr(this->recPtr)));
        }
    }

    // Record header can be split between pages but first field xl_tot_len is always on single page
    uint32 record_size = getRecordSize(((unsigned char *) this->currentPageHeader) + this->pageOffset);

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
        Min(this->walInterface.headerSize, this->walPageSize - this->pageOffset));

    // If header is split read rest of the header from next page
    if (this->walInterface.headerSize > this->walPageSize - this->pageOffset)
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
            this->walInterface.headerSize - this->gotLen);
        this->pageOffset += this->walInterface.headerSize - this->gotLen;
    }
    else
    {
        this->pageOffset += this->walInterface.headerSize;
    }
    this->gotLen = this->walInterface.headerSize;

    this->walInterface.validXLogRecordHeader(this->record);
    // Read rest of the record on this page
    size_t toRead = Min(this->record->xl_tot_len - this->walInterface.headerSize, this->walPageSize - this->pageOffset);
    memcpy(
        ((uint8 *) this->record) + this->walInterface.headerSize,
        ((uint8 *) this->currentPageHeader) + this->pageOffset,
        toRead);
    this->gotLen += toRead;

    // Move pointer to the next record on the page
    this->pageOffset += MAXALIGN(toRead);

    // Rest of the record data is on the next page
    while (this->gotLen < this->record->xl_tot_len)
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
            this->record->xl_tot_len != (this->currentPageHeader->xlp_rem_len + this->gotLen))
        {
            THROW_FMT(FormatError, "%s - invalid contrecord length: expect: %zu, get %u", strZ(pgLsnToStr(this->recPtr)),
                      this->record->xl_tot_len - this->gotLen, this->currentPageHeader->xlp_rem_len);
        }

        size_t to_write = Min(this->currentPageHeader->xlp_rem_len, this->walPageSize - this->pageOffset);
        memcpy(((char *) this->record) + this->gotLen, ((unsigned char *) this->currentPageHeader) + this->pageOffset, to_write);
        this->pageOffset += MAXALIGN(to_write);
        this->gotLen += to_write;
    }
    ASSERT(this->gotLen == this->record->xl_tot_len);

    this->walInterface.validXLogRecord(this->record);

    this->isSwitchWal = this->walInterface.xLogRecordIsWalSwitch(this->record);

    this->recordNum++;
    return ReadRecordSuccess;
}

static void
writeRecord(WalFilterState *const this, Buffer *const output, const unsigned char *recordData)
{
    // We are in the beginning of the segment file, and we have an incomplete record from the previous segment.
    if (this->beginOffset != 0)
    {
        this->gotLen -= this->beginOffset;
        recordData += this->beginOffset;
        this->beginOffset = 0;
    }

    uint32 header_i = 0;
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

        // Stop if we have reached the end of the segment file.
        // This can happen if we have an incomplete record at the end of the file.
        if (this->recPtr % this->segSize == 0)
            return;

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

// Based on XLogFilePath from Postgres
static inline const String *
xLogFileName (const TimeLineID timeLine, uint64 segno, uint32 segSize)
{
    return strNewFmt("%08X%08X%08X", timeLine,
                     (uint32) ((segno) / XLogSegmentsPerXLogId(segSize)),
                     (uint32) ((segno) % XLogSegmentsPerXLogId(segSize)));
}

static const StorageRead *
getNearWal (WalFilterState *const this, bool isNext)
{
    const TimeLineID timeLine = this->currentPageHeader->xlp_tli;
    uint64 segno = this->currentPageHeader->xlp_pageaddr / this->segSize;

    ASSERT(isNext == true || segno != 0);
    if (isNext)
        segno++;
    else
        segno--;

    const String *walName = xLogFileName(timeLine, segno, this->segSize);

    String *walDir = strNewFmt("%08X%08X", timeLine, (uint32) (segno / XLogSegmentsPerXLogId(this->segSize)));

    const String *expression;
    // The next file may be partial if the timeline has been switched.
    if (isNext)
        expression = strNewFmt("^%s(\\.partial)?-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(walName));
    else
        expression = strNewFmt("^%s-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(walName));

    const StringList *const segmentList = storageListP(
        storageRepoIdx(this->archiveInfo->repoIdx),
        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(this->archiveInfo->archiveId), strZ(walDir)),
        .expression = expression);

    if (strLstEmpty(segmentList))
    {
        return NULL;
    }

    const String *walSegment = strLstGet(segmentList, 0);

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
        LOG_WARN_FMT("%s - Missing previous WAL file. Is current file is first in the chain?", strZ(pgLsnToStr(this->recPtr)));
        goto end;
    }

    ioReadOpen(storageReadIo(storageRead));
    this->inputOffset = 0;
    this->pageOffset = 0;

    Buffer *const buffer = bufNew(this->walPageSize);
    size_t size = ioRead(storageReadIo(storageRead), buffer);
    bufUsedSet(buffer, size);

    // There may be an unfinished record from the previous file at the beginning of the file. Just skip it.
    while (true)
    {
        if (!getNextPage(this, buffer))
        {
            goto end;
        }

        if (!(this->currentPageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD) ||
            this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD)
        {
            break;
        }

        if (this->currentPageHeader->xlp_rem_len <= this->walPageSize - this->pageOffset)
        {
            this->pageOffset += MAXALIGN(this->currentPageHeader->xlp_rem_len);
            break;
        }

        bufUsedZero(buffer);
        size = ioRead(storageReadIo(storageRead), buffer);
        bufUsedSet(buffer, size);
        this->inputOffset = 0;
    }

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
    ASSERT(this->currentStep != noStep);

    ioReadClose(storageReadIo(storageRead));
    result = true;
end:
    MEM_CONTEXT_TEMP_END();
    return result;
}

static void
getEndOfRecord(WalFilterState *const this)
{
    MEM_CONTEXT_TEMP_BEGIN();

    ReadRecordStatus result = ReadRecordNeedBuffer;

    while (result != ReadRecordSuccess)
    {
        const StorageRead *const storageRead = getNearWal(this, true);

        if (storageRead == NULL)
        {
            LOG_WARN_FMT(
                "The file with the end of the %s record is missing. Has the timeline switch happened?",
                strZ(pgLsnToStr(this->recPtr)));
            goto end;
        }

        ioReadOpen(storageReadIo(storageRead));

        Buffer *const buffer = bufNew(this->walPageSize);
        size_t size = ioRead(storageReadIo(storageRead), buffer);
        bufUsedSet(buffer, size);
        while ((result = readRecord(this, buffer)) == ReadRecordNeedBuffer)
        {
            if (ioReadEof(storageReadIo(storageRead)))
            {
                break;
            }

            bufUsedZero(buffer);
            size = ioRead(storageReadIo(storageRead), buffer);
            bufUsedSet(buffer, size);
        }
        ioReadClose(storageReadIo(storageRead));
    }
end:
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
        // We have an incomplete record at the end, and we have already read something
        if (this->currentStep != noStep && this->currentStep != stepBeginOfRecord)
        {
            TRY_BEGIN()
            getEndOfRecord(this);
            CATCH_ANY()
            LOG_WARN_FMT("Error when reading the end of a record from the next file: %s", errorMessage());
            TRY_END();
            if (this->record->xl_tot_len == this->gotLen)
            {
                this->walInterface.xLogRecordFilter(this->record);
            }
            writeRecord(this, output, (const unsigned char *) this->record);
        }
        this->done = true;
        goto end;
    }

    if (this->isReadOrphanedData)
        goto readOrphanedData;

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
                this->beginOffset = this->gotLen;
                this->inputOffset = 0;
                lstClearFast(this->pageHeaders);
            }
            else
            {
                this->inputOffset = 0;
                this->isReadOrphanedData = true;
                do
                {
readOrphanedData:
                    if (!getNextPage(this, input))
                    {
                        this->inputSame = false;
                        this->inputOffset = 0;
                        goto end;
                    }
                    bufCatC(
                        output, (const unsigned char *) this->currentPageHeader, 0, XLogPageHeaderSize(this->currentPageHeader));
                    ASSERT(this->recPtr == this->currentPageHeader->xlp_pageaddr);
                    this->recPtr += XLogPageHeaderSize(this->currentPageHeader);

                    size_t toCopy = Min(MAXALIGN(this->currentPageHeader->xlp_rem_len), this->walPageSize - this->pageOffset);
                    ASSERT(!(this->currentPageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD) || toCopy == 0);
                    bufCatC(output, (const unsigned char *) this->currentPageHeader, this->pageOffset, toCopy);
                    this->recPtr += toCopy;
                }
                while (this->currentPageHeader->xlp_rem_len > this->walPageSize - this->pageOffset);
                this->isReadOrphanedData = false;
                this->pageOffset += MAXALIGN(this->currentPageHeader->xlp_rem_len);
                lstClearFast(this->pageHeaders);
            }
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
            this->walInterface.xLogRecordFilter(this->record);
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
        if (cfgOptionStrId(cfgOptFork) != CFGOPTVAL_FORK_GPDB)
        {
            THROW(VersionNotSupportedError, "WAL filtering is only supported for GPDB 6 and 7");
        }

        *this = (WalFilterState){
            .isBegin = true,
            .record = memNew(pgControl.pageSize),
            .recBufSize = pgControl.pageSize,
            .pageHeaders = lstNewP(SizeOfXLogLongPHD),
            .archiveInfo = archiveInfo,
            .walPageSize = pgControl.walPageSize,
            .segSize = pgControl.walSegmentSize,
        };

        if (pgControl.version == PG_VERSION_94)
        {
            this->walInterface = getWalInterfaceGPDB6(pgControl.pageSize);
        }
        else if (pgControl.version == PG_VERSION_12)
        {
            this->walInterface = getWalInterfaceGPDB7(pgControl.pageSize);
        }
        else
        {
            THROW(VersionNotSupportedError, "WAL filtering is not supported for this version of GPDB");
        }

        relationFilterInit();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            WAL_FILTER_TYPE, this, NULL, .done = WalFilterDone, .inOut = walFilterProcess,
            .inputSame = WalFilterInputSame));
}
