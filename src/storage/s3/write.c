/***********************************************************************************************************************************
S3 Storage File Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/s3/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
S3 query tokens
***********************************************************************************************************************************/
STRING_STATIC(S3_QUERY_PART_NUMBER_STR,                             "partNumber");
STRING_STATIC(S3_QUERY_UPLOADS_STR,                                 "uploads");
STRING_STATIC(S3_QUERY_UPLOAD_ID_STR,                               "uploadId");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(S3_XML_TAG_ETAG_STR,                                  "ETag");
STRING_STATIC(S3_XML_TAG_UPLOAD_ID_STR,                             "UploadId");
STRING_STATIC(S3_XML_TAG_COMPLETE_MULTIPART_UPLOAD_STR,             "CompleteMultipartUpload");
STRING_STATIC(S3_XML_TAG_PART_STR,                                  "Part");
STRING_STATIC(S3_XML_TAG_PART_NUMBER_STR,                           "PartNumber");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteS3
{
    StorageWriteInterface interface;                                // Interface
    StorageS3 *storage;                                             // Storage that created this object

    HttpRequest *request;                                           // Async request
    size_t partSize;
    Buffer *partBuffer;
    const String *uploadId;
    StringList *uploadPartList;
} StorageWriteS3;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_S3_TYPE                                                                                         \
    StorageWriteS3 *
#define FUNCTION_LOG_STORAGE_WRITE_S3_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(value, "StorageWriteS3", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteS3Open(THIS_VOID)
{
    THIS(StorageWriteS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer == NULL);

    // Allocate the part buffer
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->partBuffer = bufNew(this->partSize);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Flush bytes to upload part
***********************************************************************************************************************************/
static void
storageWriteS3Part(StorageWriteS3 *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If there is an outstanding async request then wait for the response and store the part id
    if (this->request != NULL)
    {
        HttpResponse *const response = storageS3ResponseP(this->request);

        strLstAdd(this->uploadPartList, httpHeaderGet(httpResponseHeader(response), HTTP_HEADER_ETAG_STR));
        ASSERT(strLstGet(this->uploadPartList, strLstSize(this->uploadPartList) - 1) != NULL);

        httpResponseFree(response);
        httpRequestFree(this->request);
        this->request = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

static void
storageWriteS3PartAsync(StorageWriteS3 *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);
    ASSERT(bufSize(this->partBuffer) > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Complete prior async request, if any
        storageWriteS3Part(this);

        // Get the upload id if we have not already
        if (this->uploadId == NULL)
        {
            // Initiate mult-part upload
            XmlNode *xmlRoot = xmlDocumentRoot(
                xmlDocumentNewBuf(
                    httpResponseContent(
                        storageS3RequestP(
                            this->storage, HTTP_VERB_POST_STR, this->interface.name,
                            .query = httpQueryAdd(httpQueryNewP(), S3_QUERY_UPLOADS_STR, EMPTY_STR), .sseKms = true))));

            // Store the upload id
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->uploadId = xmlNodeContent(xmlNodeChild(xmlRoot, S3_XML_TAG_UPLOAD_ID_STR, true));
                this->uploadPartList = strLstNew();
            }
            MEM_CONTEXT_OBJ_END();
        }

        // Upload the part async
        HttpQuery *query = httpQueryNewP();
        httpQueryAdd(query, S3_QUERY_UPLOAD_ID_STR, this->uploadId);
        httpQueryAdd(query, S3_QUERY_PART_NUMBER_STR, strNewFmt("%u", strLstSize(this->uploadPartList) + 1));

        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->request = storageS3RequestAsyncP(
                this->storage, HTTP_VERB_PUT_STR, this->interface.name, .query = query, .content = this->partBuffer);
        }
        MEM_CONTEXT_OBJ_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to internal buffer
***********************************************************************************************************************************/
static void
storageWriteS3(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWriteS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_S3, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);
    ASSERT(buffer != NULL);

    size_t bytesTotal = 0;

    // Continue until the write buffer has been exhausted
    do
    {
        // Copy as many bytes as possible into the part buffer
        size_t bytesNext = bufRemains(this->partBuffer) > bufUsed(buffer) - bytesTotal ?
            bufUsed(buffer) - bytesTotal : bufRemains(this->partBuffer);
        bufCatSub(this->partBuffer, buffer, bytesTotal, bytesNext);
        bytesTotal += bytesNext;

        // If the part buffer is full then write it
        if (bufRemains(this->partBuffer) == 0)
        {
            storageWriteS3PartAsync(this);
            bufUsedZero(this->partBuffer);
        }
    }
    while (bytesTotal != bufUsed(buffer));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWriteS3Close(THIS_VOID)
{
    THIS(StorageWriteS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    if (this->partBuffer != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // If a multi-part upload was started we need to finish that way
            if (this->uploadId != NULL)
            {
                // If there is anything left in the part buffer then write it
                if (!bufEmpty(this->partBuffer))
                    storageWriteS3PartAsync(this);

                // Complete prior async request, if any
                storageWriteS3Part(this);

                // Generate the xml part list
                XmlDocument *partList = xmlDocumentNew(S3_XML_TAG_COMPLETE_MULTIPART_UPLOAD_STR);

                for (unsigned int partIdx = 0; partIdx < strLstSize(this->uploadPartList); partIdx++)
                {
                    XmlNode *partNode = xmlNodeAdd(xmlDocumentRoot(partList), S3_XML_TAG_PART_STR);
                    xmlNodeContentSet(xmlNodeAdd(partNode, S3_XML_TAG_PART_NUMBER_STR), strNewFmt("%u", partIdx + 1));
                    xmlNodeContentSet(xmlNodeAdd(partNode, S3_XML_TAG_ETAG_STR), strLstGet(this->uploadPartList, partIdx));
                }

                // Finalize the multi-part upload
                HttpRequest *request = storageS3RequestAsyncP(
                    this->storage, HTTP_VERB_POST_STR, this->interface.name,
                    .query = httpQueryAdd(httpQueryNewP(), S3_QUERY_UPLOAD_ID_STR, this->uploadId),
                    .content = xmlDocumentBuf(partList));
                HttpResponse *response = storageS3ResponseP(request);

                // Error if there is no etag in the result. This indicates that the request did not succeed despite the success code.
                if (xmlNodeChild(
                        xmlDocumentRoot(xmlDocumentNewBuf(httpResponseContent(response))), S3_XML_TAG_ETAG_STR, false) == NULL)
                {
                    httpRequestError(request, response);
                }
            }
            // Else upload all the data in a single put
            else
            {
                storageS3RequestP(
                    this->storage, HTTP_VERB_PUT_STR, this->interface.name, .content = this->partBuffer, .sseKms = true);
            }

            bufFree(this->partBuffer);
            this->partBuffer = NULL;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteS3New(StorageS3 *storage, const String *name, size_t partSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageWrite *this = NULL;

    OBJ_NEW_BEGIN(StorageWriteS3, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageWriteS3 *driver = OBJ_NEW_ALLOC();

        *driver = (StorageWriteS3)
        {
            .storage = storage,
            .partSize = partSize,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_S3_TYPE,
                .name = strDup(name),
                .atomic = true,
                .createPath = true,
                .syncFile = true,
                .syncPath = true,
                .truncate = true,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWriteS3Close,
                    .open = storageWriteS3Open,
                    .write = storageWriteS3,
                },
            },
        };

        this = storageWriteNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
