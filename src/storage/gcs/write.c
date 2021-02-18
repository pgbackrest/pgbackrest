/***********************************************************************************************************************************
GCS Storage File Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/io/filter/filter.intern.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/object.h"
#include "storage/gcs/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
GCS HTTP headers
***********************************************************************************************************************************/
// STRING_STATIC(GCS_HEADER_BLOB_TYPE_STR,                           "x-ms-blob-type");
// STRING_STATIC(GCS_HEADER_VALUE_BLOCK_BLOB_STR,                    "BlockBlob");

/***********************************************************************************************************************************
GCS query tokens
***********************************************************************************************************************************/
// STRING_STATIC(GCS_QUERY_BLOCK_ID_STR,                             "blockid");
//
// STRING_STATIC(GCS_QUERY_VALUE_BLOCK_STR,                          "block");
// STRING_STATIC(GCS_QUERY_VALUE_BLOCK_LIST_STR,                     "blocklist");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
// STRING_STATIC(GCS_XML_TAG_BLOCK_LIST_STR,                         "BlockList");
// STRING_STATIC(GCS_XML_TAG_UNCOMMITTED_STR,                        "Uncommitted");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteGcs
{
    MemContext *memContext;                                         // Object mem context
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
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->chunkBuffer = bufNew(this->chunkSize);
        this->md5hash = cryptoHashNew(HASH_TYPE_MD5_STR);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Verify upload
***********************************************************************************************************************************/
static void
storageWriteGcsVerify(StorageWriteGcs *this, HttpResponse *response)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, response);
    FUNCTION_LOG_END();

    LOG_DETAIL_FMT("HEADER:\n%s\nRESPONSE:\n%s", strZ(httpHeaderToLog(httpResponseHeader(response))), strZ(strNewBuf(httpResponseContent(response)))); // !!! REMOVE THIS

    KeyValue *content = jsonToKv(strNewBuf(httpResponseContent(response)));

    // Get the md5 hash
    const String *md5base64 = varStr(kvGet(content, VARSTRDEF("md5Hash")));
    CHECK(md5base64 != NULL);

    Buffer *md5 = bufNew(HASH_TYPE_M5_SIZE);
    ASSERT(decodeToBinSize(encodeBase64, strZ(md5base64)) == bufSize(md5));

    decodeToBin(encodeBase64, strZ(md5base64), bufPtr(md5));
    bufUsedSet(md5, bufSize(md5));

    const String *md5actual = bufHex(md5);
    const String *md5expected = varStr(ioFilterResult(this->md5hash));

    if (!strEq(md5actual, md5expected))
    {
        THROW_FMT(
            FormatError, "expected md5 '%s' for '%s' but actual is '%s'", strZ(md5expected), strZ(this->interface.name),
            strZ(md5actual));
    }

    // Get the size
    // !!! SINCE SIZE IS NOT SUPPORTED BY FAKE-GCS, PERHAPS DO INFO INSTEAD
    const String *sizeStr = varStr(kvGet(content, VARSTRDEF("size")));

    if (sizeStr != NULL)
    {
        uint64_t size = cvtZToUInt64(strZ(sizeStr));

        if (size != this->uploadTotal)
        {
            THROW_FMT(
                FormatError, "expected size %" PRIu64 " for '%s' but actual is %" PRIu64, size, strZ(this->interface.name),
                this->uploadTotal);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Flush bytes to upload chunk
***********************************************************************************************************************************/
static void
storageWriteGcsBlock(StorageWriteGcs *this, bool done)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
        FUNCTION_LOG_PARAM(BOOL, done);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If there is an outstanding async request then wait for the response. Since the part id has already been stored there is
    // nothing to do except make sure the request did not error.
    if (this->request != NULL)
    {
        HttpResponse *response = storageGcsResponseP(this->request, .allowIncomplete = !done);

        // If done then verify the md5 checksum
        if (done)
            storageWriteGcsVerify(this, response);

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
        httpQueryAdd(query, STRDEF("name"), strSub(this->interface.name, 1));
        httpQueryAdd(query, STRDEF("uploadType"), STRDEF("resumable"));

        // Get the session URI
        if (this->uploadId == NULL)
        {
            HttpResponse *response = storageGcsRequestP(this->storage, HTTP_VERB_POST_STR, .upload = true, .query = query);

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // THROW_FMT(AssertError, "URI: %s", strZ(httpHeaderGet(httpResponseHeader(response), STRDEF("location"))));
                this->uploadId = strDup(httpHeaderGet(httpResponseHeader(response), STRDEF("x-guploader-uploadid")));
            }
            MEM_CONTEXT_END();
        }

        // Upload the chunk
        ioFilterProcessIn(this->md5hash, this->chunkBuffer);

        HttpHeader *header = httpHeaderAdd(
            httpHeaderNew(NULL), STRDEF(HTTP_HEADER_CONTENT_RANGE),
            strNewFmt(
                "bytes %" PRIu64 "-%" PRIu64 "/%s", this->uploadTotal, this->uploadTotal + bufUsed(this->chunkBuffer) - 1,
                done ? strZ(strNewFmt("%" PRIu64, this->uploadTotal + bufUsed(this->chunkBuffer))) : "*"));

        httpQueryAdd(query, STRDEF("upload_id"), this->uploadId);

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->request = storageGcsRequestAsyncP(
                this->storage, HTTP_VERB_PUT_STR, .upload = true, .header = header, .query = query, .content = this->chunkBuffer);
        }
        MEM_CONTEXT_END();

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
    ASSERT(this->chunkBuffer != NULL);

    size_t bytesTotal = 0;

    // Continue until the write buffer has been exhausted
    do
    {
        // If the chunk buffer is full then write it
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
            // If a multi-chunk upload was started we need to finish that way
            if (this->uploadId != NULL)
            {
                LOG_DEBUG_FMT("!!!GOT TO FINAL %" PRIu64, this->uploadTotal);

                ASSERT(!bufEmpty(this->chunkBuffer));

                // Write what is left in the chunk buffer
                storageWriteGcsBlockAsync(this, true);
                storageWriteGcsBlock(this, true);
            }
            // Else upload all the data in a single chunk
            else
            {
                if (bufUsed(this->chunkBuffer))
                    ioFilterProcessIn(this->md5hash, this->chunkBuffer);

                HttpQuery *query = httpQueryNewP();
                httpQueryAdd(query, STRDEF("name"), strSub(this->interface.name, 1));
                httpQueryAdd(query, STRDEF("uploadType"), STRDEF("media"));

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

    MEM_CONTEXT_NEW_BEGIN("StorageWriteGcs")
    {
        StorageWriteGcs *driver = memNew(sizeof(StorageWriteGcs));

        *driver = (StorageWriteGcs)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .chunkSize = chunkSize,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_GCS_TYPE_STR,
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
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
