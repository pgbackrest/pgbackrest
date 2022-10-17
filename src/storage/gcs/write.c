/***********************************************************************************************************************************
GCS Storage File Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/object.h"
#include "storage/gcs/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
GCS query tokens
***********************************************************************************************************************************/
STRING_STATIC(GCS_QUERY_UPLOAD_TYPE_STR,                            "uploadType");
STRING_STATIC(GCS_QUERY_RESUMABLE_STR,                              "resumable");
STRING_STATIC(GCS_QUERY_FIELDS_VALUE_STR,                           GCS_JSON_MD5_HASH "," GCS_JSON_SIZE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteGcs
{
    StorageWriteInterface interface;                                // Interface
    StorageGcs *storage;                                            // Storage that created this object

    HttpRequest *request;                                           // Async chunk upload request
    size_t chunkSize;                                               // Size of chunks for resumable upload
    Buffer *chunkBuffer;                                            // Block buffer (stores data until chunkSize is reached)
    const String *uploadId;                                         // Id for resumable upload
    uint64_t uploadTotal;                                           // Total bytes uploaded
    IoFilter *md5hash;                                              // MD5 hash of file
} StorageWriteGcs;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_GCS_TYPE                                                                                        \
    StorageWriteGcs *
#define FUNCTION_LOG_STORAGE_WRITE_GCS_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "StorageWriteGcs", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteGcsOpen(THIS_VOID)
{
    THIS(StorageWriteGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->chunkBuffer == NULL);

    // Allocate the chunk buffer
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->chunkBuffer = bufNew(this->chunkSize);
        this->md5hash = cryptoHashNew(hashTypeMd5);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Verify upload
***********************************************************************************************************************************/
static void
storageWriteGcsVerify(StorageWriteGcs *const this, HttpResponse *const response)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, response);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        KeyValue *const content = varKv(jsonToVar(strNewBuf(httpResponseContent(response))));

        // Check the md5 hash
        const String *const md5base64 = varStr(kvGet(content, GCS_JSON_MD5_HASH_VAR));
        CHECK(FormatError, md5base64 != NULL, "MD5 missing");

        const Buffer *const md5actual = bufNewDecode(encodeBase64, md5base64);
        const Buffer *const md5expected = pckReadBinP(pckReadNew(ioFilterResult(this->md5hash)));

        if (!bufEq(md5actual, md5expected))
        {
            THROW_FMT(
                FormatError, "expected md5 '%s' for '%s' but actual is '%s'", strZ(bufHex(md5expected)), strZ(this->interface.name),
                strZ(bufHex(md5actual)));
        }

        // Check the size when available
        const String *const sizeStr = varStr(kvGet(content, GCS_JSON_SIZE_VAR));

        if (sizeStr != NULL)
        {
            const uint64_t size = cvtZToUInt64(strZ(sizeStr));

            if (size != this->uploadTotal)
            {
                THROW_FMT(
                    FormatError, "expected size %" PRIu64 " for '%s' but actual is %" PRIu64, size, strZ(this->interface.name),
                    this->uploadTotal);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Flush bytes to upload chunk
***********************************************************************************************************************************/
static void
storageWriteGcsBlock(StorageWriteGcs *const this, const bool done)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(BOOL, done);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If there is an outstanding async request then wait for the response to ensure the request did not error
    if (this->request != NULL)
    {
        HttpResponse *const response = storageGcsResponseP(this->request, .allowIncomplete = !done);

        // If done then verify the md5 checksum
        if (done)
            storageWriteGcsVerify(this, response);

        httpResponseFree(response);
        httpRequestFree(this->request);
        this->request = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

static void
storageWriteGcsBlockAsync(StorageWriteGcs *this, bool done)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(BOOL, done);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->chunkBuffer != NULL);
    ASSERT(bufSize(this->chunkBuffer) > 0);
    ASSERT(!done || this->uploadId != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Complete prior async request, if any
        storageWriteGcsBlock(this, false);

        // Build query
        HttpQuery *query = httpQueryNewP();
        httpQueryAdd(query, GCS_QUERY_NAME_STR, strSub(this->interface.name, 1));
        httpQueryAdd(query, GCS_QUERY_UPLOAD_TYPE_STR, GCS_QUERY_RESUMABLE_STR);

        // Get the upload id
        if (this->uploadId == NULL)
        {
            HttpResponse *response = storageGcsRequestP(this->storage, HTTP_VERB_POST_STR, .upload = true, .query = query);

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->uploadId = strDup(httpHeaderGet(httpResponseHeader(response), GCS_HEADER_UPLOAD_ID_STR));
                CHECK(FormatError, this->uploadId != NULL, "upload id missing");
            }
            MEM_CONTEXT_OBJ_END();
        }

        // Add data to md5 hash
        ioFilterProcessIn(this->md5hash, this->chunkBuffer);

        // Upload the chunk. If this is the last chunk then add the total bytes in the file to the range rather than the * added to
        // prior chunks. This indicates that the resumable upload is complete.
        HttpHeader *header = httpHeaderAdd(
            httpHeaderNew(NULL), HTTP_HEADER_CONTENT_RANGE_STR,
            strNewFmt(
                HTTP_HEADER_CONTENT_RANGE_BYTES " %" PRIu64 "-%" PRIu64 "/%s", this->uploadTotal,
                this->uploadTotal + bufUsed(this->chunkBuffer) - 1,
                done ? zNewFmt("%" PRIu64, this->uploadTotal + bufUsed(this->chunkBuffer)) : "*"));

        httpQueryAdd(query, GCS_QUERY_UPLOAD_ID_STR, this->uploadId);

        // Add fields needed to verify to upload
        if (done)
            httpQueryAdd(query, GCS_QUERY_FIELDS_STR, GCS_QUERY_FIELDS_VALUE_STR);

        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->request = storageGcsRequestAsyncP(
                this->storage, HTTP_VERB_PUT_STR, .upload = true, .noAuth = true, .header = header, .query = query,
                .content = this->chunkBuffer);
        }
        MEM_CONTEXT_OBJ_END();

        this->uploadTotal += bufUsed(this->chunkBuffer);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to internal buffer
***********************************************************************************************************************************/
static void
storageWriteGcs(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWriteGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->chunkBuffer != NULL);

    size_t bytesTotal = 0;

    // Continue until the write buffer has been exhausted
    do
    {
        // If the chunk buffer is full then write it. We can't write it at the end of this loop because this might be the end of the
        // input and we'd have no way to signal the end of the resumable upload when closing the file if there is no more data.
        if (bufRemains(this->chunkBuffer) == 0)
        {
            storageWriteGcsBlockAsync(this, false);
            bufUsedZero(this->chunkBuffer);
        }

        // Copy as many bytes as possible into the chunk buffer
        size_t bytesNext = bufRemains(this->chunkBuffer) > bufUsed(buffer) - bytesTotal ?
            bufUsed(buffer) - bytesTotal : bufRemains(this->chunkBuffer);
        bufCatSub(this->chunkBuffer, buffer, bytesTotal, bytesNext);
        bytesTotal += bytesNext;
    }
    while (bytesTotal != bufUsed(buffer));


    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWriteGcsClose(THIS_VOID)
{
    THIS(StorageWriteGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    if (this->chunkBuffer != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // If a resumable upload was started then finish that way
            if (this->uploadId != NULL)
            {
                ASSERT(!bufEmpty(this->chunkBuffer));

                // Write what is left in the chunk buffer
                storageWriteGcsBlockAsync(this, true);
                storageWriteGcsBlock(this, true);
            }
            // Else upload all the data in a single chunk
            else
            {
                // Add data to md5 hash
                if (bufUsed(this->chunkBuffer))
                    ioFilterProcessIn(this->md5hash, this->chunkBuffer);

                // Upload file
                HttpQuery *query = httpQueryNewP();
                httpQueryAdd(query, GCS_QUERY_NAME_STR, strSub(this->interface.name, 1));
                httpQueryAdd(query, GCS_QUERY_UPLOAD_TYPE_STR, GCS_QUERY_MEDIA_STR);
                httpQueryAdd(query, GCS_QUERY_FIELDS_STR, GCS_QUERY_FIELDS_VALUE_STR);

                this->uploadTotal = bufUsed(this->chunkBuffer);

                storageWriteGcsVerify(
                    this,
                    storageGcsRequestP(
                        this->storage, HTTP_VERB_POST_STR, .upload = true, .query = query, .content = this->chunkBuffer));
            }

            bufFree(this->chunkBuffer);
            this->chunkBuffer = NULL;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteGcsNew(StorageGcs *storage, const String *name, size_t chunkSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, chunkSize);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageWrite *this = NULL;

    OBJ_NEW_BEGIN(StorageWriteGcs, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageWriteGcs *driver = OBJ_NEW_ALLOC();

        *driver = (StorageWriteGcs)
        {
            .storage = storage,
            .chunkSize = chunkSize,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_GCS_TYPE,
                .name = strDup(name),
                .atomic = true,
                .createPath = true,
                .syncFile = true,
                .syncPath = true,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWriteGcsClose,
                    .open = storageWriteGcsOpen,
                    .write = storageWriteGcs,
                },
            },
        };

        this = storageWriteNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
