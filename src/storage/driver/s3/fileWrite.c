/***********************************************************************************************************************************
S3 Storage File Write Driver
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/xml.h"
#include "storage/driver/s3/fileWrite.h"
#include "storage/fileWrite.intern.h"

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
struct StorageDriverS3FileWrite
{
    MemContext *memContext;
    StorageDriverS3 *storage;
    StorageFileWrite *interface;
    IoWrite *io;

    const String *path;
    const String *name;
    size_t partSize;
    Buffer *partBuffer;
    const String *uploadId;
    StringList *uploadPartList;
};

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageDriverS3FileWrite *
storageDriverS3FileWriteNew(StorageDriverS3 *storage, const String *name, size_t partSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageDriverS3FileWrite *this = NULL;

    // Create the file
    MEM_CONTEXT_NEW_BEGIN("StorageDriverS3FileWrite")
    {
        this = memNew(sizeof(StorageDriverS3FileWrite));
        this->memContext = MEM_CONTEXT_NEW();
        this->storage = storage;

        this->interface = storageFileWriteNewP(
            STORAGE_DRIVER_S3_TYPE_STR, this, .atomic = (StorageFileWriteInterfaceAtomic)storageDriverS3FileWriteAtomic,
            .createPath = (StorageFileWriteInterfaceCreatePath)storageDriverS3FileWriteCreatePath,
            .io = (StorageFileWriteInterfaceIo)storageDriverS3FileWriteIo,
            .modeFile = (StorageFileWriteInterfaceModeFile)storageDriverS3FileWriteModeFile,
            .modePath = (StorageFileWriteInterfaceModePath)storageDriverS3FileWriteModePath,
            .name = (StorageFileWriteInterfaceName)storageDriverS3FileWriteName,
            .syncFile = (StorageFileWriteInterfaceSyncFile)storageDriverS3FileWriteSyncFile,
            .syncPath = (StorageFileWriteInterfaceSyncPath)storageDriverS3FileWriteSyncPath);

        this->io = ioWriteNewP(
            this, .close = (IoWriteInterfaceClose)storageDriverS3FileWriteClose,
            .open = (IoWriteInterfaceOpen)storageDriverS3FileWriteOpen,
            .write = (IoWriteInterfaceWrite)storageDriverS3FileWrite);

        this->path = strPath(name);
        this->name = strDup(name);
        this->partSize = partSize;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_S3_FILE_WRITE, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageDriverS3FileWriteOpen(StorageDriverS3FileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
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
storageDriverS3FileWritePart(StorageDriverS3FileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);
    ASSERT(bufSize(this->partBuffer) > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the upload id if we have not already
        if (this->uploadId == NULL)
        {
            // Initiate mult-part upload
            XmlNode *xmlRoot = xmlDocumentRoot(
                xmlDocumentNewBuf(
                    storageDriverS3Request(
                        this->storage, HTTP_VERB_POST_STR, this->name,
                        httpQueryAdd(httpQueryNew(), S3_QUERY_UPLOADS_STR, EMPTY_STR), NULL, true, false).response));

            // Store the upload id
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->uploadId = xmlNodeContent(xmlNodeChild(xmlRoot, S3_XML_TAG_UPLOAD_ID_STR, true));
                this->uploadPartList = strLstNew();
            }
            MEM_CONTEXT_END();
        }

        // Upload the part and add etag to part list
        HttpQuery *query = httpQueryNew();
        httpQueryAdd(query, S3_QUERY_UPLOAD_ID_STR, this->uploadId);
        httpQueryAdd(query, S3_QUERY_PART_NUMBER_STR, strNewFmt("%u", strLstSize(this->uploadPartList) + 1));

        strLstAdd(
            this->uploadPartList,
            httpHeaderGet(
                storageDriverS3Request(
                    this->storage, HTTP_VERB_PUT_STR, this->name, query, this->partBuffer, true, false).responseHeader,
                HTTP_HEADER_ETAG_STR));

        ASSERT(strLstGet(this->uploadPartList, strLstSize(this->uploadPartList) - 1) != NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to internal buffer
***********************************************************************************************************************************/
void
storageDriverS3FileWrite(StorageDriverS3FileWrite *this, const Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->partBuffer != NULL);

    size_t bytesTotal = 0;

    // Continue until the write buffer has been exhausted
    do
    {
        // Copy an many bytes as possible into the part buffer
        size_t bytesNext = bufRemains(this->partBuffer) > bufUsed(buffer) - bytesTotal ?
            bufUsed(buffer) - bytesTotal : bufRemains(this->partBuffer);
        bufCatSub(this->partBuffer, buffer, bytesTotal, bytesNext);
        bytesTotal += bytesNext;

        // If the part buffer is full then write it
        if (bufRemains(this->partBuffer) == 0)
        {
            storageDriverS3FileWritePart(this);
            bufUsedZero(this->partBuffer);
        }
    }
    while (bytesTotal != bufUsed(buffer));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageDriverS3FileWriteClose(StorageDriverS3FileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
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
                if (bufUsed(this->partBuffer) > 0)
                    storageDriverS3FileWritePart(this);

                // Generate the xml part list
                XmlDocument *partList = xmlDocumentNew(S3_XML_TAG_COMPLETE_MULTIPART_UPLOAD_STR);

                for (unsigned int partIdx = 0; partIdx < strLstSize(this->uploadPartList); partIdx++)
                {
                    XmlNode *partNode = xmlNodeAdd(xmlDocumentRoot(partList), S3_XML_TAG_PART_STR);
                    xmlNodeContentSet(xmlNodeAdd(partNode, S3_XML_TAG_PART_NUMBER_STR), strNewFmt("%u", partIdx + 1));
                    xmlNodeContentSet(xmlNodeAdd(partNode, S3_XML_TAG_ETAG_STR), strLstGet(this->uploadPartList, partIdx));
                }

                // Finalize the multi-part upload
                storageDriverS3Request(
                    this->storage, HTTP_VERB_POST_STR, this->name,
                    httpQueryAdd(httpQueryNew(), S3_QUERY_UPLOAD_ID_STR, this->uploadId), xmlDocumentBuf(partList), false, false);
            }
            // Else upload all the data in a single put
            else
                storageDriverS3Request(this->storage, HTTP_VERB_PUT_STR, this->name, NULL, this->partBuffer, false, false);

            bufFree(this->partBuffer);
            this->partBuffer = NULL;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
S3 operations are always atomic, so return true
***********************************************************************************************************************************/
bool
storageDriverS3FileWriteAtomic(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(true);
}

/***********************************************************************************************************************************
S3 paths are always implicitly created
***********************************************************************************************************************************/
bool
storageDriverS3FileWriteCreatePath(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(true);
}

/***********************************************************************************************************************************
Get interface
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverS3FileWriteInterface(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}

/***********************************************************************************************************************************
Get I/O interface
***********************************************************************************************************************************/
IoWrite *
storageDriverS3FileWriteIo(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
S3 does not support Posix-style mode
***********************************************************************************************************************************/
mode_t
storageDriverS3FileWriteModeFile(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(0);
}

/***********************************************************************************************************************************
S3 does not support Posix-style mode
***********************************************************************************************************************************/
mode_t
storageDriverS3FileWriteModePath(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(0);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageDriverS3FileWriteName(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->name);
}

/***********************************************************************************************************************************
S3 operations are always atomic, so return true
***********************************************************************************************************************************/
bool
storageDriverS3FileWriteSyncFile(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(true);
}

/***********************************************************************************************************************************
S3 operations are always atomic, so return true
***********************************************************************************************************************************/
bool
storageDriverS3FileWriteSyncPath(const StorageDriverS3FileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    (void)this;

    FUNCTION_TEST_RETURN(true);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageDriverS3FileWriteFree(StorageDriverS3FileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_WRITE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
