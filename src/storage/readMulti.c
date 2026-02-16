/***********************************************************************************************************************************
Storage Read Multi
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/limitRead.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/readMulti.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageReadMulti
{
    StorageReadMultiPub pub;                                        // Publicly accessible variables
    const Storage *storage;                                         // Storage
    List *requestList;                                              // List of read requests
    List *queue;                                                    // Queue of reads
    uint64_t readOver;                                              // Bytes to read over rather than open file with new offset
    unsigned int queueMax;                                          // Max size of read queue
    bool eof;                                                       // End-of-file indicator
};

/***********************************************************************************************************************************
Structure to store read requests
***********************************************************************************************************************************/
typedef struct StorageReadMultiRequest
{
    const String *fileExp;
    uint64_t compressible;                                          // Is the file compressible?
    uint64_t offset;                                                // Where to start reading in the file
    uint64_t limit;                                                 // Limit bytes to read from the file
    List *rangeList;                                                // Ranges for read over
    uint64_t rangeRead;                                             // Bytes read in the current range
} StorageReadMultiRequest;

/***********************************************************************************************************************************
Structure to ranges for read over
***********************************************************************************************************************************/
typedef struct StorageReadMultiRange
{
    uint64_t offset;                                                // Where to start reading in the file
    uint64_t limit;                                                 // Limit bytes to read from the file
} StorageReadMultiRange;

/***********************************************************************************************************************************
Constant to indicate that there is no limit
***********************************************************************************************************************************/
#define STORAGE_READ_MULTI_NO_LIMIT                                 UINT64_MAX

/***********************************************************************************************************************************
Manage the read queue
***********************************************************************************************************************************/
static void
storageReadMultiQueue(StorageReadMulti *const this, const bool prelim)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
        FUNCTION_LOG_PARAM(BOOL, prelim);
    FUNCTION_LOG_END();

    // If the read queue is less than max then queue more reads
    if (lstSize(this->queue) < this->queueMax)
    {
        // Look for request to queue but skip the last request during prelim queueing since it might not be complete
        for (unsigned int requestIdx = lstSize(this->queue); requestIdx < lstSize(this->requestList) - prelim; requestIdx++)
        {
            const StorageReadMultiRequest *const request = lstGet(this->requestList, requestIdx);

            MEM_CONTEXT_OBJ_BEGIN(this->queue)
            {
                StorageRead *const read = storageNewReadP(
                    this->storage, request->fileExp, .compressible = request->compressible, .offset = request->offset,
                    .limit = request->limit != STORAGE_READ_MULTI_NO_LIMIT ? VARUINT64(request->limit) : NULL);
                ioReadOpen(storageReadIo(read));

                lstAdd(this->queue, &read);
            }
            MEM_CONTEXT_OBJ_END();

            // Break when queue is full
            if (lstSize(this->queue) == this->queueMax)
                break;
        }
    }

    // If the queue is empty and not in prelim queueing then eof
    if (lstEmpty(this->queue) && !prelim)
        this->eof = true;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadMultiOpen(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageReadMultiQueue(this, false);

    // !!! DEBUG RANGES
    MEM_CONTEXT_TEMP_BEGIN()
    {
        LOG_DEBUG_FMT("!!!MULTI SIZE %u", lstSize(this->requestList));

        for (unsigned int requestIdx = 0; requestIdx < lstSize(this->requestList); requestIdx++)
        {
            StorageReadMultiRequest *request = lstGet(this->requestList, requestIdx);
            LOG_DEBUG_FMT(
                "!!!  REQUEST %s OFFSET %" PRIu64 " LIMIT %" PRIu64 ", RANGE SIZE %u", strZ(request->fileExp), request->offset,
                request->limit, request->rangeList != NULL ? lstSize(request->rangeList) : 0);

            if (request->rangeList != NULL)
            {
                for (unsigned int rangeIdx = 0; rangeIdx < lstSize(request->rangeList); rangeIdx++)
                {
                    StorageReadMultiRange *range = lstGet(request->rangeList, rangeIdx);
                    String *gap = strNew();

                    if (rangeIdx != 0)
                    {
                        StorageReadMultiRange *rangePrior = lstGet(request->rangeList, rangeIdx - 1);
                        strCatFmt(gap, " (gap %" PRIu64 ")", range->offset - (rangePrior->offset + rangePrior->limit));
                    }

                    LOG_DEBUG_FMT(
                        "!!!    RANGE OFFSET %" PRIu64 " LIMIT %" PRIu64 "%s", range->offset, range->limit, strZ(gap));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/***********************************************************************************************************************************
Read from a file and retry when there is a read failure
***********************************************************************************************************************************/
static size_t
storageReadMulti(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadMulti);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    // ASSERT(buffer != NULL && !bufEmpty(buffer));

    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageReadMultiRequest *const request = lstGet(this->requestList, 0);
        const StorageReadMultiRange *const range = request->rangeList == NULL ? NULL : lstGet(request->rangeList, 0);
        StorageRead *const read = *(StorageRead **)lstGet(this->queue, 0);
        IoRead *const readIo = storageReadIo(read);

        // !!!
        bufLimitClear(buffer);

        if (range != NULL && lstSize(request->rangeList) > 1)
        {
            uint64_t rangeRemains = range->limit - request->rangeRead;

            if (bufRemains(buffer) > rangeRemains)
                bufLimitSet(buffer, (size_t)rangeRemains);
        }

        result = ioRead(readIo, buffer);

        // !!!
        if (range != NULL)
        {
            request->rangeRead += result;

            // If range is complete and there are more ranges to process
            if (range->limit - request->rangeRead == 0 && lstSize(request->rangeList) > 1)
            {
                // Read over unused bytes
                StorageReadMultiRange *const rangeNext = lstGet(request->rangeList, 1);
                IoRead *const readOver = ioLimitReadNew(readIo, rangeNext->offset - (range->offset + range->limit));

                ioReadDrain(readOver);

                // Remove range and reset read
                lstRemoveIdx(request->rangeList, 0);
                request->rangeRead = 0;
            }
        }

        // On eof close read and update queue
        if (ioReadEof(readIo))
        {
            ioReadClose(readIo);
            storageReadFree(read);

            lstRemoveIdx(this->requestList, 0);
            lstRemoveIdx(this->queue, 0);

            storageReadMultiQueue(this, false);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageReadMultiClose(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // this->ioInterface.close(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadMultiEof(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->eof);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageReadMultiAdd(StorageReadMulti *const this, const String *const fileExp, const StorageReadMultiAddParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(UINT64, param.compressible);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    // Check if new request can be combined with prior request
    StorageReadMultiRequest *const requestPrior = lstEmpty(this->requestList) ? NULL : lstGetLast(this->requestList);

    if (requestPrior != NULL && strEq(requestPrior->fileExp, fileExp))
    {
        CHECK(AssertError, requestPrior->limit != STORAGE_READ_MULTI_NO_LIMIT, "cannot add request after request with no limit");
        CHECK(AssertError, param.limit != NULL, "request with no limit must be first");
        CHECK_FMT(
            AssertError, param.offset >= requestPrior->offset + requestPrior->limit,
            "new request offset %" PRIu64 " must be after prior request (offset %" PRIu64 ", limit %" PRIu64 ")", param.offset,
            requestPrior->offset, requestPrior->limit);

        // If the new offset is within the allowed gap from prior range end then extend the prior range
        const uint64_t limit = varUInt64(param.limit);
        const uint64_t requestPriorEnd = requestPrior->offset + requestPrior->limit;

        if (param.offset - requestPriorEnd <= this->readOver)
        {
            // If the new range offset is not exactly after the prior range end then add to the subrange list
            if (param.offset != requestPriorEnd)
            {
                // If the subrange list does not exist then add it
                if (requestPrior->rangeList == NULL)
                {
                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        requestPrior->rangeList = lstNewP(sizeof(StorageReadMultiRange));
                    }
                    MEM_CONTEXT_OBJ_END();

                    // Add the prior range to the range list
                    lstAdd(
                        requestPrior->rangeList,
                        &(StorageReadMultiRange){.offset = requestPrior->offset, .limit = requestPrior->limit});
                }

                lstAdd(requestPrior->rangeList, &(StorageReadMultiRange){.offset = param.offset, .limit = limit});
            }
            // Else combine with prior range if it exists
            else if (requestPrior->rangeList != NULL)
            {
                StorageReadMultiRange *const range = lstGetLast(requestPrior->rangeList);
                range->limit = param.offset - range->offset + limit;
            }

            // Combine limit with prior limit
            requestPrior->limit = param.offset - requestPrior->offset + limit;

            FUNCTION_LOG_RETURN_VOID();
        }
    }

    // Add request if not combined with prior
    MEM_CONTEXT_OBJ_BEGIN(this->requestList)
    {
        const StorageReadMultiRequest request =
        {
            .fileExp = requestPrior != NULL && strEq(requestPrior->fileExp, fileExp) ? requestPrior->fileExp : strDup(fileExp),
            .compressible = param.compressible,
            .offset = param.offset,
            .limit = param.limit == NULL ? STORAGE_READ_MULTI_NO_LIMIT : varUInt64(param.limit),
        };

        lstAdd(this->requestList, &request);
    }
    MEM_CONTEXT_OBJ_END();

    // Update queue
    storageReadMultiQueue(this, true);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const IoReadInterface storageIoReadMultiInterface =
{
    .close = storageReadMultiClose,
    .eof = storageReadMultiEof,
    .open = storageReadMultiOpen,
    .read = storageReadMulti,
};

FN_EXTERN StorageReadMulti *
storageReadMultiNew(const Storage *const storage, const unsigned int concurrency, const uint64_t readOver)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(UINT, concurrency);
        FUNCTION_LOG_PARAM(UINT64, readOver);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(concurrency != 0);

    OBJ_NEW_BEGIN(StorageReadMulti, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadMulti)
        {
            .storage = storage,
            .requestList = lstNewP(sizeof(StorageReadMultiRequest), .comparator = lstComparatorStr),
            .queue = lstNewP(sizeof(StorageRead *)),
            .queueMax = concurrency,
            .readOver = readOver,
            .pub =
            {
                .io = ioReadNew(this, storageIoReadMultiInterface),
            },
        };
    }
    OBJ_NEW_END();

    // !!! SHOULD WE EXPOSE A StorageRead INTERFACE? This would allow it to work with storageGetP() etc.
    FUNCTION_LOG_RETURN(STORAGE_READ_MULTI, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageReadMultiToLog(const StorageReadMulti *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{storage: ");
    storageToLog(this->storage, debugLog);
    strStcFmt(debugLog, ", size: %u}", lstSize(this->requestList));
}
