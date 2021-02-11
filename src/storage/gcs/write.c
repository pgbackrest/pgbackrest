/***********************************************************************************************************************************
GCS Storage File Write
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

    HttpRequest *request;                                           // Async block upload request
    size_t blockSize;                                               // Size of blocks for multi-block upload
    Buffer *blockBuffer;                                            // Block buffer (stores data until blockSize is reached)
    // StringList *blockIdList;                                        // List of uploaded block ids
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
    ASSERT(this->blockBuffer == NULL);

    // Allocate the block buffer
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->blockBuffer = bufNew(this->blockSize);
    }
    MEM_CONTEXT_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Flush bytes to upload block
***********************************************************************************************************************************/
// static void
// storageWriteGcsBlock(StorageWriteGcs *this)
// {
//     FUNCTION_LOG_BEGIN(logLevelTrace);
//         FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
//     FUNCTION_LOG_END();
//
//     ASSERT(this != NULL);
//
//     // If there is an outstanding async request then wait for the response. Since the part id has already been stored there is
//     // nothing to do except make sure the request did not error.
//     if (this->request != NULL)
//     {
//         storageGcsResponseP(this->request);
//         httpRequestFree(this->request);
//         this->request = NULL;
//     }
//
//     FUNCTION_LOG_RETURN_VOID();
// }
//
// static void
// storageWriteGcsBlockAsync(StorageWriteGcs *this)
// {
//     FUNCTION_LOG_BEGIN(logLevelTrace);
//         FUNCTION_LOG_PARAM(STORAGE_WRITE_GCS, this);
//     FUNCTION_LOG_END();
//
//     ASSERT(this != NULL);
//     ASSERT(this->blockBuffer != NULL);
//     ASSERT(bufSize(this->blockBuffer) > 0);
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         // Complete prior async request, if any
//         storageWriteGcsBlock(this);
//
//         // Create the block id list
//         if (this->blockIdList == NULL)
//         {
//             MEM_CONTEXT_BEGIN(this->memContext)
//             {
//                 this->blockIdList = strLstNew();
//             }
//             MEM_CONTEXT_END();
//         }
//
//         // Generate block id. Combine the block number with the provided file id to create a (hopefully) unique block id that won't
//         // overlap with any other process. This is to prevent another process from overwriting our blocks. If two processes are
//         // writing against the same file then there may be problems anyway but we need to at least ensure the result is consistent,
//         // i.e. we get all of one file or all of the other depending on who writes last.
//         const String *blockId = strNewFmt("%016" PRIX64 "x%07u", this->fileId, strLstSize(this->blockIdList));
//
//         // Upload the block and add to block list
//         HttpQuery *query = httpQueryNewP();
//         httpQueryAdd(query, GCS_QUERY_COMP_STR, GCS_QUERY_VALUE_BLOCK_STR);
//         httpQueryAdd(query, GCS_QUERY_BLOCK_ID_STR, blockId);
//
//         MEM_CONTEXT_BEGIN(this->memContext)
//         {
//             this->request = storageGcsRequestAsyncP(
//                 this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name, .query = query, .content = this->blockBuffer);
//         }
//         MEM_CONTEXT_END();
//
//         strLstAdd(this->blockIdList, blockId);
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN_VOID();
// }

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
    ASSERT(this->blockBuffer != NULL);

    // size_t bytesTotal = 0;
    //
    // // Continue until the write buffer has been exhausted
    // do
    // {
    //     // Copy as many bytes as possible into the block buffer
    //     size_t bytesNext = bufRemains(this->blockBuffer) > bufUsed(buffer) - bytesTotal ?
    //         bufUsed(buffer) - bytesTotal : bufRemains(this->blockBuffer);
    //     bufCatSub(this->blockBuffer, buffer, bytesTotal, bytesNext);
    //     bytesTotal += bytesNext;
    //
    //     // If the block buffer is full then write it
    //     if (bufRemains(this->blockBuffer) == 0)
    //     {
    //         storageWriteGcsBlockAsync(this);
    //         bufUsedZero(this->blockBuffer);
    //     }
    // }
    // while (bytesTotal != bufUsed(buffer));

    THROW(AssertError, "NOT YET IMPLEMENTED");

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

    // // Close if the file has not already been closed
    // if (this->blockBuffer != NULL)
    // {
    //     MEM_CONTEXT_TEMP_BEGIN()
    //     {
    //         // If a multi-block upload was started we need to finish that way
    //         if (this->blockIdList != NULL)
    //         {
    //             // If there is anything left in the block buffer then write it
    //             if (!bufEmpty(this->blockBuffer))
    //                 storageWriteGcsBlockAsync(this);
    //
    //             // Complete prior async request, if any
    //             storageWriteGcsBlock(this);
    //
    //             // Generate the xml block list
    //             XmlDocument *blockXml = xmlDocumentNew(GCS_XML_TAG_BLOCK_LIST_STR);
    //
    //             for (unsigned int blockIdx = 0; blockIdx < strLstSize(this->blockIdList); blockIdx++)
    //             {
    //                 xmlNodeContentSet(
    //                     xmlNodeAdd(xmlDocumentRoot(blockXml), GCS_XML_TAG_UNCOMMITTED_STR),
    //                     strLstGet(this->blockIdList, blockIdx));
    //             }
    //
    //             // Finalize the multi-block upload
    //             storageGcsRequestP(
    //                 this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name,
    //                 .query = httpQueryAdd(httpQueryNewP(), GCS_QUERY_COMP_STR, GCS_QUERY_VALUE_BLOCK_LIST_STR),
    //                 .content = xmlDocumentBuf(blockXml));
    //         }
    //         // Else upload all the data in a single block
    //         else
    //         {
    //             storageGcsRequestP(
    //                 this->storage, HTTP_VERB_PUT_STR, .uri = this->interface.name,
    //                 httpHeaderAdd(httpHeaderNew(NULL), GCS_HEADER_BLOB_TYPE_STR, GCS_HEADER_VALUE_BLOCK_BLOB_STR),
    //                 .content = this->blockBuffer);
    //         }
    //
    //         bufFree(this->blockBuffer);
    //         this->blockBuffer = NULL;
    //     }
    //     MEM_CONTEXT_TEMP_END();
    // }

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteGcsNew(StorageGcs *storage, const String *name, size_t blockSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, blockSize);
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
            .blockSize = blockSize,

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
