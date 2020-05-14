/***********************************************************************************************************************************
Azure Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/azure/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Azure http headers
***********************************************************************************************************************************/
STRING_STATIC(AZURE_HEADER_BLOB_TYPE_STR,                           "x-ms-blob-type");
STRING_STATIC(AZURE_HEADER_VALUE_BLOCK_BLOB_STR,                    "BlockBlob");

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
STRING_STATIC(AZURE_QUERY_PART_NUMBER_STR,                          "partNumber");
STRING_STATIC(AZURE_QUERY_UPLOADS_STR,                              "uploads");
STRING_STATIC(AZURE_QUERY_UPLOAD_ID_STR,                            "uploadId");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(AZURE_XML_TAG_ETAG_STR,                               "ETag");
STRING_STATIC(AZURE_XML_TAG_UPLOAD_ID_STR,                          "UploadId");
STRING_STATIC(AZURE_XML_TAG_COMPLETE_MULTIPART_UPLOAD_STR,          "CompleteMultipartUpload");
STRING_STATIC(AZURE_XML_TAG_PART_STR,                               "Part");
STRING_STATIC(AZURE_XML_TAG_PART_NUMBER_STR,                        "PartNumber");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteAzure
{
    MemContext *memContext;                                         // Object mem context
    StorageWriteInterface interface;                                // Interface
    StorageAzure *storage;                                             // Storage that created this object

    size_t partSize;
    Buffer *partBuffer;
    const String *uploadId;
    StringList *uploadPartList;
} StorageWriteAzure;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_AZURE_TYPE                                                                                      \
    StorageWriteAzure *
#define FUNCTION_LOG_STORAGE_WRITE_AZURE_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageWriteAzure", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteAzureOpen(THIS_VOID)
{
    THIS(StorageWriteAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_AZURE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer == NULL);

    // Allocate the part buffer
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->partBuffer = bufNew(this->partSize);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Flush bytes to upload part
***********************************************************************************************************************************/
static void
storageWriteAzurePart(StorageWriteAzure *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_AZURE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);
    ASSERT(bufSize(this->partBuffer) > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the upload id if we have not already
        if (this->uploadId == NULL)
        {
            THROW(AssertError, "NOT YET IMPLEMENTED");

            // Initiate mult-part upload
            XmlNode *xmlRoot = xmlDocumentRoot(
                xmlDocumentNewBuf(
                    storageAzureRequestP(
                        this->storage, HTTP_VERB_POST_STR, .uri = this->interface.name,
                        .query = httpQueryAdd(httpQueryNew(), AZURE_QUERY_UPLOADS_STR, EMPTY_STR),
                        .returnContent = true).response));

            // Store the upload id
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->uploadId = xmlNodeContent(xmlNodeChild(xmlRoot, AZURE_XML_TAG_UPLOAD_ID_STR, true));
                this->uploadPartList = strLstNew();
            }
            MEM_CONTEXT_END();
        }

        // Upload the part and add etag to part list
        HttpQuery *query = httpQueryNew();
        httpQueryAdd(query, AZURE_QUERY_UPLOAD_ID_STR, this->uploadId);
        httpQueryAdd(query, AZURE_QUERY_PART_NUMBER_STR, strNewFmt("%u", strLstSize(this->uploadPartList) + 1));

        strLstAdd(
            this->uploadPartList,
            httpHeaderGet(
                storageAzureRequestP(
                    this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name, .query = query, .body = this->partBuffer,
                    .returnContent = true).responseHeader,
                HTTP_HEADER_ETAG_STR));

        ASSERT(strLstGet(this->uploadPartList, strLstSize(this->uploadPartList) - 1) != NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to internal buffer
***********************************************************************************************************************************/
static void
storageWriteAzure(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWriteAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_AZURE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);

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
            storageWriteAzurePart(this);
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
storageWriteAzureClose(THIS_VOID)
{
    THIS(StorageWriteAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_AZURE, this);
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
                THROW(AssertError, "NOT YET IMPLEMENTED");

                // If there is anything left in the part buffer then write it
                if (bufUsed(this->partBuffer) > 0)
                    storageWriteAzurePart(this);

                // Generate the xml part list
                XmlDocument *partList = xmlDocumentNew(AZURE_XML_TAG_COMPLETE_MULTIPART_UPLOAD_STR);

                for (unsigned int partIdx = 0; partIdx < strLstSize(this->uploadPartList); partIdx++)
                {
                    XmlNode *partNode = xmlNodeAdd(xmlDocumentRoot(partList), AZURE_XML_TAG_PART_STR);
                    xmlNodeContentSet(xmlNodeAdd(partNode, AZURE_XML_TAG_PART_NUMBER_STR), strNewFmt("%u", partIdx + 1));
                    xmlNodeContentSet(xmlNodeAdd(partNode, AZURE_XML_TAG_ETAG_STR), strLstGet(this->uploadPartList, partIdx));
                }

                // Finalize the multi-part upload
                storageAzureRequestP(
                    this->storage, HTTP_VERB_POST_STR, .uri = this->interface.name,
                    .query = httpQueryAdd(httpQueryNew(), AZURE_QUERY_UPLOAD_ID_STR, this->uploadId),
                    .body = xmlDocumentBuf(partList), .returnContent = true);
            }
            // Else upload all the data in a single put
            else
            {
                storageAzureRequestP(
                    this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name,
                    httpHeaderAdd(httpHeaderNew(NULL), AZURE_HEADER_BLOB_TYPE_STR, AZURE_HEADER_VALUE_BLOCK_BLOB_STR),
                    .body = this->partBuffer, .returnContent = true);
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
storageWriteAzureNew(StorageAzure *storage, const String *name, size_t partSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageWriteAzure")
    {
        StorageWriteAzure *driver = memNew(sizeof(StorageWriteAzure));

        *driver = (StorageWriteAzure)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .partSize = partSize,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_AZURE_TYPE_STR,
                .name = strDup(name),
                .atomic = true,
                .createPath = true,
                .syncFile = true,
                .syncPath = true,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWriteAzureClose,
                    .open = storageWriteAzureOpen,
                    .write = storageWriteAzure,
                },
            },
        };

        this = storageWriteNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
