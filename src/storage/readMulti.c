/***********************************************************************************************************************************
Storage Read Multi
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/limitRead.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/read.intern.h"
#include "storage/readMulti.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageReadMulti
{
    StorageReadMultiPub pub;                                        // Publicly accessible variables
    const Storage *storage;                                         // Storage
    void *driver;                                                   // Driver
};

/***********************************************************************************************************************************
Default driver that queues reads directly on the storage
***********************************************************************************************************************************/
typedef struct StorageReadMultiDefault
{
    const StorageReadMultiInterface *interface;                     // Driver interface (must be first member)
    const Storage *storage;                                         // Storage
    List *requestList;                                              // List of read requests
    List *queue;                                                    // Queue of reads
    uint64_t readOver;                                              // Bytes to read over rather than open file with new offset
    unsigned int queueMax;                                          // Max size of read queue
    bool eof;                                                       // End-of-file indicator
} StorageReadMultiDefault;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_MULTI_DEFAULT_TYPE                                                                               \
    StorageReadMultiDefault *
#define FUNCTION_LOG_STORAGE_READ_MULTI_DEFAULT_FORMAT(value, buffer, bufferSize)                                                  \
    objNameToLog(value, "StorageReadMultiDefault", buffer, bufferSize)

/***********************************************************************************************************************************
Structure to store read requests
***********************************************************************************************************************************/
typedef struct StorageReadMultiRequest
{
    const String *file;
    bool compressible;                                              // Is the file compressible?
    uint64_t offset;                                                // Where to start reading in the file
    uint64_t limit;                                                 // Limit bytes to read from the file
    const String *versionId;                                        // Version to read (NULL for current version)
    List *rangeList;                                                // Ranges for read over
    uint64_t rangeRead;                                             // Bytes read in the current range
} StorageReadMultiRequest;

/***********************************************************************************************************************************
Structure to store ranges for read over
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
storageReadMultiDefaultQueue(StorageReadMultiDefault *const this, const bool prelim)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
        FUNCTION_LOG_PARAM(BOOL, prelim);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!prelim || !lstEmpty(this->requestList));

    // If the read queue is less than max then queue more reads
    if (lstSize(this->queue) < this->queueMax)
    {
        // Look for request to queue but skip the last request during prelim queueing since it might not be complete
        for (unsigned int requestIdx = lstSize(this->queue); requestIdx < lstSize(this->requestList) - prelim; requestIdx++)
        {
            const StorageReadMultiRequest *const request = lstGet(this->requestList, requestIdx);

            // Open the read request. The version id, when needed, was already resolved when the request was added.
            MEM_CONTEXT_OBJ_BEGIN(this->queue)
            {
                StorageRead *const read = storageReadNew(
                    this->storage, request->file, false, request->compressible, request->offset,
                    request->limit != STORAGE_READ_MULTI_NO_LIMIT ? VARUINT64(request->limit) : NULL,
                    request->versionId != NULL, request->versionId);
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
Open read
***********************************************************************************************************************************/
static bool
storageReadMultiDefaultOpen(THIS_VOID)
{
    THIS(StorageReadMultiDefault);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageReadMultiDefaultQueue(this, false);

    FUNCTION_LOG_RETURN(BOOL, true);
}

/***********************************************************************************************************************************
Read from multiple files as if they were one file, skipping unused bytes
***********************************************************************************************************************************/
static size_t
storageReadMultiDefault(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadMultiDefault);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->eof);
    ASSERT(!lstEmpty(this->requestList));
    ASSERT(!lstEmpty(this->queue));
    ASSERT(buffer != NULL && bufEmpty(buffer));

    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageReadMultiRequest *const request = lstGet(this->requestList, 0);
        const StorageReadMultiRange *const range = request->rangeList == NULL ? NULL : lstGet(request->rangeList, 0);
        StorageRead *const read = *(StorageRead **)lstGet(this->queue, 0);
        IoRead *const readIo = storageReadIo(read);

        // Set buffer limit so we do not read into unused bytes
        bufLimitClear(buffer);

        if (range != NULL && lstSize(request->rangeList) > 1)
        {
            uint64_t rangeRemains = range->limit - request->rangeRead;

            if (bufRemains(buffer) > rangeRemains)
                bufLimitSet(buffer, (size_t)rangeRemains);
        }

        // Read range
        result = ioRead(readIo, buffer);

        // Check range completion
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

            storageReadMultiDefaultQueue(this, false);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Close queued files
***********************************************************************************************************************************/
static void
storageReadMultiDefaultClose(THIS_VOID)
{
    THIS(StorageReadMultiDefault);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    for (unsigned int queueIdx = 0; queueIdx < lstSize(this->queue); queueIdx++)
        ioReadClose(storageReadIo(*(StorageRead **)lstGet(this->queue, queueIdx)));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has read reached EOF?
***********************************************************************************************************************************/
static bool
storageReadMultiDefaultEof(THIS_VOID)
{
    THIS(StorageReadMultiDefault);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->eof);
}

/***********************************************************************************************************************************
Add a file to the read
***********************************************************************************************************************************/
static void
storageReadMultiDefaultAdd(THIS_VOID, const String *const file, const StorageReadMultiAddParam param)
{
    THIS(StorageReadMultiDefault);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_DEFAULT, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Check if new request can be combined with prior request
    StorageReadMultiRequest *const requestPrior = lstEmpty(this->requestList) ? NULL : lstGetLast(this->requestList);

    if (requestPrior != NULL && strEq(requestPrior->file, file))
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
            .file = requestPrior != NULL && strEq(requestPrior->file, file) ? requestPrior->file : strDup(file),
            .compressible = param.compressible,
            .offset = param.offset,
            .limit = param.limit == NULL ? STORAGE_READ_MULTI_NO_LIMIT : varUInt64(param.limit),
            .versionId = strDup(param.versionId),
        };

        lstAdd(this->requestList, &request);
    }
    MEM_CONTEXT_OBJ_END();

    // Update queue
    storageReadMultiDefaultQueue(this, true);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New multi-file read
***********************************************************************************************************************************/
static const StorageReadMultiInterface storageReadMultiDefaultInterface =
{
    .add = storageReadMultiDefaultAdd,
    .close = storageReadMultiDefaultClose,
    .eof = storageReadMultiDefaultEof,
    .open = storageReadMultiDefaultOpen,
    .read = storageReadMultiDefault,
};

static void *
storageReadMultiDefaultNew(const Storage *const storage, const unsigned int prefetch, const uint64_t readOver)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(UINT, prefetch);
        FUNCTION_LOG_PARAM(UINT64, readOver);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    OBJ_NEW_BEGIN(StorageReadMultiDefault, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadMultiDefault)
        {
            .interface = &storageReadMultiDefaultInterface,
            .storage = storage,
            .requestList = lstNewP(sizeof(StorageReadMultiRequest), .comparator = lstComparatorStr),
            .queue = lstNewP(sizeof(StorageRead *)),
            .queueMax = prefetch + 1,
            .readOver = readOver,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_MULTI_DEFAULT, this);
}

/***********************************************************************************************************************************
Open read interface
***********************************************************************************************************************************/
static bool
storageReadMultiOpen(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, storageReadMultiDriverInterface(this->driver)->open(this->driver));
}

/***********************************************************************************************************************************
Read interface
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
    ASSERT(buffer != NULL);

    FUNCTION_LOG_RETURN(SIZE, storageReadMultiDriverInterface(this->driver)->read(this->driver, buffer, block));
}

/***********************************************************************************************************************************
Close interface
***********************************************************************************************************************************/
static void
storageReadMultiClose(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageReadMultiDriverInterface(this->driver)->close(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Eof interface
***********************************************************************************************************************************/
static bool
storageReadMultiEof(THIS_VOID)
{
    THIS(StorageReadMulti);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_MULTI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, storageReadMultiDriverInterface(this->driver)->eof(this->driver));
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageReadMultiAdd(StorageReadMulti *const this, const String *const fileExp, StorageReadMultiAddParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(fileExp != NULL);
    ASSERT(param.limit == NULL || varType(param.limit) == varTypeUInt64);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const path = storagePathP(this->storage, fileExp);

        // If targeting by time and the version has not already been resolved then look up the version id
        if (param.versionId == NULL && storageTargetTime(this->storage) != 0)
        {
            param.versionId = storageInfoP(this->storage, fileExp, .ignoreMissing = true).versionId;

            if (param.versionId == NULL)
                THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(path));
        }

        storageReadMultiDriverInterface(this->driver)->add(this->driver, path, param);
    }
    MEM_CONTEXT_TEMP_END();

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
storageReadMultiNew(const Storage *const storage, const unsigned int prefetch, const uint64_t readOver)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(UINT, prefetch);
        FUNCTION_LOG_PARAM(UINT64, readOver);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    OBJ_NEW_BEGIN(StorageReadMulti, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadMulti)
        {
            .storage = storage,
            .driver =
                storageInterface(storage).newReadMulti != NULL ?
                    storageInterface(storage).newReadMulti(storageDriver(storage)) :
                    storageReadMultiDefaultNew(storage, prefetch, readOver),
        };

        ASSERT(storageReadMultiDriverInterface(this->driver)->add != NULL);
        ASSERT(storageReadMultiDriverInterface(this->driver)->close != NULL);
        ASSERT(storageReadMultiDriverInterface(this->driver)->eof != NULL);
        ASSERT(storageReadMultiDriverInterface(this->driver)->open != NULL);
        ASSERT(storageReadMultiDriverInterface(this->driver)->read != NULL);

        this->pub.io = ioReadNew(this, storageIoReadMultiInterface);

        // Set filter group when interface function exists
        if (storageReadMultiDriverInterface(this->driver)->filterGroup != NULL)
        {
            storageReadMultiDriverInterface(this->driver)->filterGroup(
                this->driver, ioReadFilterGroup(storageReadMultiIo(this)));
        }
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_MULTI, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageReadMultiToLog(const StorageReadMulti *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{storage: ");
    storageToLog(this->storage, debugLog);
    strStcCatChr(debugLog, '}');
}
