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
#include "storage/write.h"

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
    bool tag;                                                       // Are tags available?
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
    objNameToLog(value, "StorageWriteGcs", buffer, bufferSize)

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

        const Buffer *const md5actual = bufNewDecode(encodingBase64, md5base64);
        const Buffer *const md5expected = pckReadBinP(pckReadNew(ioFilterResult(this->md5hash)));

        if (!bufEq(md5actual, md5expected))
        {
            THROW_FMT(
                FormatError, "expected md5 '%s' for '%s' but actual is '%s'", strZ(strNewEncode(encodingHex, md5expected)),
                strZ(this->interface.name), strZ(strNewEncode(encodingHex, md5actual)));
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
storageWriteGcsBlockAsync(StorageWriteGcs *const this, const bool done)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(BOOL, done);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->chunkBuffer != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Complete prior async request, if any
        storageWriteGcsBlock(this, false);

        // Build query
        HttpQuery *const query = httpQueryNewP();
        httpQueryAdd(query, GCS_QUERY_NAME_STR, strSub(this->interface.name, 1));
        httpQueryAdd(query, GCS_QUERY_UPLOAD_TYPE_STR, GCS_QUERY_RESUMABLE_STR);

        // Get the upload id
        if (this->uploadId == NULL)
        {
            HttpResponse *response = storageGcsRequestP(
                this->storage, HTTP_VERB_POST_STR, .upload = true, .tag = this->tag, .query = query);

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->uploadId = strDup(httpHeaderGet(httpResponseHeader(response), GCS_HEADER_UPLOAD_ID_STR));
                CHECK(FormatError, this->uploadId != NULL, "upload id missing");
            }
            MEM_CONTEXT_OBJ_END();
        }

        // Add data to md5 hash
        if (!bufEmpty(this->chunkBuffer))
            ioFilterProcessIn(this->md5hash, this->chunkBuffer);

        // Upload the chunk. If this is the last chunk then add the total bytes in the file to the range rather than the * added to
        // prior chunks. This indicates that the resumable upload is complete. If the last chunk is zero-size, then the byte range
        // is * to indicate that there is no more data to upload.
        HttpHeader *const header = httpHeaderAdd(
            httpHeaderNew(NULL), HTTP_HEADER_CONTENT_RANGE_STR,
            strNewFmt(
                HTTP_HEADER_CONTENT_RANGE_BYTES " %s/%s",
                bufUsed(this->chunkBuffer) == 0 ?
                    "*" : zNewFmt("%" PRIu64 "-%" PRIu64, this->uploadTotal, this->uploadTotal + bufUsed(this->chunkBuffer) - 1),
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
storageWriteGcs(THIS_VOID, const Buffer *const buffer)
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
        // Copy as many bytes as possible into the chunk buffer
        const size_t bytesNext =
            bufRemains(this->chunkBuffer) > bufUsed(buffer) - bytesTotal ?
                bufUsed(buffer) - bytesTotal : bufRemains(this->chunkBuffer);

        bufCatSub(this->chunkBuffer, buffer, bytesTotal, bytesNext);
        bytesTotal += bytesNext;

        // If the chunk buffer is full then write it. It is possible that this is the last chunk and it would be better to wait, but
        // the chances of that are quite small so in general it is better to write now so there is less to write later.
        if (bufRemains(this->chunkBuffer) == 0)
        {
            storageWriteGcsBlockAsync(this, false);
            bufUsedZero(this->chunkBuffer);
        }
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
            if (this->uploadId != NULL || this->tag)
            {
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
FN_EXTERN StorageWrite *
storageWriteGcsNew(StorageGcs *const storage, const String *const name, const size_t chunkSize, const bool tag)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, chunkSize);
        FUNCTION_LOG_PARAM(BOOL, tag);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageWriteGcs, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageWriteGcs)
        {
            .storage = storage,
            .chunkSize = chunkSize,
            .tag = tag,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_GCS_TYPE,
                .name = strDup(name),
                .atomic = true,
                .createPath = true,
                .syncFile = true,
                .syncPath = true,
                .truncate = true,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWriteGcsClose,
                    .open = storageWriteGcsOpen,
                    .write = storageWriteGcs,
                },
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteNew(this, &this->interface));
}
