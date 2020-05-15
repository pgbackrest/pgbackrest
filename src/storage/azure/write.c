/***********************************************************************************************************************************
Azure Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/encode.h"
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
STRING_STATIC(AZURE_QUERY_BLOCK_ID_STR,                             "blockid");

STRING_STATIC(AZURE_QUERY_VALUE_BLOCK_STR,                          "block");
STRING_STATIC(AZURE_QUERY_VALUE_BLOCK_LIST_STR,                     "blocklist");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(AZURE_XML_TAG_BLOCK_LIST_STR,                         "BlockList");
STRING_STATIC(AZURE_XML_TAG_UNCOMMITTED_STR,                        "Uncommitted");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteAzure
{
    MemContext *memContext;                                         // Object mem context
    StorageWriteInterface interface;                                // Interface
    StorageAzure *storage;                                          // Storage that created this object

    size_t blockSize;                                               // Size of blocks during multi-block upload
    Buffer *blockBuffer;                                            // Block buffer (stores data until blockSize is reached)
    StringList *blockIdList;                                        // List of uploaded part ids
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
    ASSERT(this->blockBuffer == NULL);

    // Allocate the part buffer
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->blockBuffer = bufNew(this->blockSize);
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
    ASSERT(this->blockBuffer != NULL);
    ASSERT(bufSize(this->blockBuffer) > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the block id list
        if (this->blockIdList == NULL)
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->blockIdList = strLstNew();
            }
            MEM_CONTEXT_END();
        }

        // Generate block id
        char blockId[9];
        ASSERT(sizeof(blockId) == encodeToStrSize(encodeBase64, 6) + 1);

        encodeToStr(encodeBase64, (unsigned char *)strPtr(strNewFmt("%06u", strLstSize(this->blockIdList))), 6, blockId);

        // Upload the part and add to part list
        HttpQuery *query = httpQueryNew();
        httpQueryAdd(query, AZURE_QUERY_COMP_STR, AZURE_QUERY_VALUE_BLOCK_STR);
        httpQueryAdd(query, AZURE_QUERY_BLOCK_ID_STR, STR(blockId));

        storageAzureRequestP(
            this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name, .query = query, .body = this->blockBuffer);

        strLstAddZ(this->blockIdList, blockId);
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
    ASSERT(this->blockBuffer != NULL);

    size_t bytesTotal = 0;

    // Continue until the write buffer has been exhausted
    do
    {
        // Copy as many bytes as possible into the part buffer
        size_t bytesNext = bufRemains(this->blockBuffer) > bufUsed(buffer) - bytesTotal ?
            bufUsed(buffer) - bytesTotal : bufRemains(this->blockBuffer);
        bufCatSub(this->blockBuffer, buffer, bytesTotal, bytesNext);
        bytesTotal += bytesNext;

        // If the part buffer is full then write it
        if (bufRemains(this->blockBuffer) == 0)
        {
            storageWriteAzurePart(this);
            bufUsedZero(this->blockBuffer);
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
    if (this->blockBuffer != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // If a multi-block upload was started we need to finish that way
            if (this->blockIdList != NULL)
            {
                // If there is anything left in the block buffer then write it
                if (bufUsed(this->blockBuffer) > 0)
                    storageWriteAzurePart(this);

                // Generate the xml block list
                XmlDocument *blockXml = xmlDocumentNew(AZURE_XML_TAG_BLOCK_LIST_STR);

                for (unsigned int blockIdx = 0; blockIdx < strLstSize(this->blockIdList); blockIdx++)
                {
                    xmlNodeContentSet(
                        xmlNodeAdd(xmlDocumentRoot(blockXml), AZURE_XML_TAG_UNCOMMITTED_STR),
                        strLstGet(this->blockIdList, blockIdx));
                }

                // Finalize the multi-block upload
                storageAzureRequestP(
                    this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name,
                    .query = httpQueryAdd(httpQueryNew(), AZURE_QUERY_COMP_STR, AZURE_QUERY_VALUE_BLOCK_LIST_STR),
                    .body = xmlDocumentBuf(blockXml));
            }
            // Else upload all the data in a single block
            else
            {
                storageAzureRequestP(
                    this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name,
                    httpHeaderAdd(httpHeaderNew(NULL), AZURE_HEADER_BLOB_TYPE_STR, AZURE_HEADER_VALUE_BLOCK_BLOB_STR),
                    .body = this->blockBuffer);
            }

            bufFree(this->blockBuffer);
            this->blockBuffer = NULL;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteAzureNew(StorageAzure *storage, const String *name, size_t blockSize)
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
            .blockSize = blockSize,

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
