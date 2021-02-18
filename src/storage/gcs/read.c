/***********************************************************************************************************************************
GCS Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "storage/gcs/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_READ_GCS_TYPE                                       StorageReadGcs
#define STORAGE_READ_GCS_PREFIX                                     storageReadGcs

typedef struct StorageReadGcs
{
    MemContext *memContext;                                         // Object mem context
    StorageReadInterface interface;                                 // Interface
    StorageGcs *storage;                                            // Storage that created this object

    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadGcs;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_GCS_TYPE                                                                                         \
    StorageReadGcs *
#define FUNCTION_LOG_STORAGE_READ_GCS_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(value, "StorageReadGcs", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadGcsOpen(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse == NULL);

    bool result = false;

    // Request the file
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->httpResponse = storageGcsRequestP(
            this->storage, HTTP_VERB_GET_STR, .object = this->interface.name, .allowMissing = true, .contentIo = true,
            .query = httpQueryAdd(httpQueryNewP(), STRDEF("alt"), STRDEF("media")));
    }
    MEM_CONTEXT_END();

    if (httpResponseCodeOk(this->httpResponse))
    {
        result = true;
    }
    // Else error unless ignore missing
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, "unable to open '%s': No such file or directory", strZ(this->interface.name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadGcs(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpResponseIoRead(this->httpResponse), buffer));
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadGcsEof(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpResponseIoRead(this->httpResponse)));
}

/**********************************************************************************************************************************/
StorageRead *
storageReadGcsNew(StorageGcs *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageReadGcs")
    {
        StorageReadGcs *driver = memNew(sizeof(StorageReadGcs));

        *driver = (StorageReadGcs)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_GCS_TYPE_STR,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,

                .ioInterface = (IoReadInterface)
                {
                    .eof = storageReadGcsEof,
                    .open = storageReadGcsOpen,
                    .read = storageReadGcs,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
