/***********************************************************************************************************************************
Storage Write Interface
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageWrite
{
    StorageWritePub pub;                                            // Publicly accessible variables
    void *driver;                                                   // Driver
};

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteOpen(THIS_VOID)
{
    THIS(StorageWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageWriteDriverInterface(this->driver)->open(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to a file
***********************************************************************************************************************************/
static void
storageWrite(THIS_VOID, const Buffer *const buffer)
{
    THIS(StorageWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageWriteDriverInterface(this->driver)->write(this->driver, buffer);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWriteClose(THIS_VOID)
{
    THIS(StorageWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageWriteDriverInterface(this->driver)->close(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
storageWriteFd(const THIS_VOID)
{
    THIS(const StorageWrite);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, storageWriteDriverInterface(this->driver)->fd(this->driver));
}

/***********************************************************************************************************************************
Seek in file
***********************************************************************************************************************************/
static void
storageWriteSeek(THIS_VOID, const uint64_t position)
{
    THIS(StorageWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, this);
        FUNCTION_LOG_PARAM(UINT64, position);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    storageWriteDriverInterface(this->driver)->seek(this->driver, position);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const IoWriteInterface storageIoWriteInterface =
{
    .close = storageWriteClose,
    .fd = storageWriteFd,
    .open = storageWriteOpen,
    .seek = storageWriteSeek,
    .write = storageWrite,
};

FN_EXTERN StorageWrite *
storageWriteNew(
    const Storage *const storage, const String *const name, const mode_t modeFile, const mode_t modePath, const String *const user,
    const String *const group, const time_t timeModified, const bool createPath, const bool syncFile, const bool syncPath,
    const bool atomic, const bool truncate, const bool compressible)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(TIME, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
        FUNCTION_LOG_PARAM(BOOL, truncate);
        FUNCTION_LOG_PARAM(BOOL, compressible);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageWrite)
        {
            .driver = storageInterfaceNewWriteP(
                storageDriver(storage), name, .modeFile = modeFile, .modePath = modePath, .user = user, .group = group,
                .timeModified = timeModified, .createPath = createPath, .syncFile = syncFile, .syncPath = syncPath,
                .atomic = atomic, .truncate = truncate, .compressible = compressible),
            .pub =
            {
                .type = storageType(storage),
                .name = strDup(name),
                .createPath = createPath,
                .atomic = atomic,
                .truncate = truncate,
                .syncPath = syncPath,
                .syncFile = syncFile,
                .user = strDup(user),
                .group = strDup(group),
                .modePath = modePath,
                .modeFile = modeFile,
                .timeModified = timeModified,
            },
        };

        ASSERT(storageWriteDriverInterface(this->driver)->close != NULL);
        ASSERT(storageWriteDriverInterface(this->driver)->write != NULL);

        // Remove fd method if it does not exist in the driver
        IoWriteInterface storageIoWriteInterfaceCopy = storageIoWriteInterface;

        if (storageWriteDriverInterface(this->driver)->fd == NULL)
            storageIoWriteInterfaceCopy.fd = NULL;

        // Remove open method if it does not exist in the driver (cloud drivers don't have open)
        if (storageWriteDriverInterface(this->driver)->open == NULL)
            storageIoWriteInterfaceCopy.open = NULL;

        // Remove seek method if it does not exist in the driver
        if (storageWriteDriverInterface(this->driver)->seek == NULL)
            storageIoWriteInterfaceCopy.seek = NULL;

        this->pub.io = ioWriteNew(this, storageIoWriteInterfaceCopy);

        // Set filter group when interface function exists
        if (storageWriteDriverInterface(this->driver)->filterGroup != NULL)
            storageWriteDriverInterface(this->driver)->filterGroup(this->driver, ioWriteFilterGroup(storageWriteIo(this)));
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}

/**********************************************************************************************************************************/
#define STORAGE_CHUNK_SIZE_MAX                                      (size_t)(1024 * 1024 * 1024)
#define STORAGE_CHUNK_INCR                                          (size_t)(8 * 1024 * 1024)

FN_EXTERN size_t
storageWriteChunkSize(
    const size_t chunkSizeDefault, const unsigned int splitDefault, const unsigned int splitMax, const unsigned int chunkIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, chunkSizeDefault);
        FUNCTION_TEST_PARAM(UINT, splitDefault);
        FUNCTION_TEST_PARAM(UINT, splitMax);
        FUNCTION_TEST_PARAM(UINT, chunkIdx);
    FUNCTION_TEST_END();

    ASSERT(chunkSizeDefault > 0);
    ASSERT(STORAGE_CHUNK_SIZE_MAX >= chunkSizeDefault);
    ASSERT(STORAGE_CHUNK_INCR > 0);
    ASSERT(splitMax > splitDefault);

    // If below the default split then return default chunk size
    if (chunkIdx < splitDefault)
    {
        FUNCTION_TEST_RETURN(SIZE, chunkSizeDefault);
    }
    // Else if above max split then return max chunk size
    else if (chunkIdx > splitMax)
        FUNCTION_TEST_RETURN(SIZE, STORAGE_CHUNK_SIZE_MAX);

    // Calculate ascending chunk size
    uint64_t result =
        (uint64_t)(STORAGE_CHUNK_SIZE_MAX - STORAGE_CHUNK_INCR) * (chunkIdx - splitDefault + 1) / (splitMax - splitDefault);

    // If ascending chunk size is less than default then return default
    if (result <= chunkSizeDefault)
    {
        result = chunkSizeDefault;
    }
    // Else if ascending chunk size is not evenly divisible by chunk increment then round up
    else if (result % STORAGE_CHUNK_INCR != 0)
        result += STORAGE_CHUNK_INCR - (result % STORAGE_CHUNK_INCR);

    // Chunk size should never exceed SIZE_MAX (might happen on 32-bit platforms)
    CHECK(AssertError, result <= SIZE_MAX, "chunk size exceeds SIZE_MAX");

    FUNCTION_TEST_RETURN(SIZE, (size_t)result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageWriteChunkBufferResize(const Buffer *const input, Buffer *const chunk, const size_t chunkSizeMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, input);
        FUNCTION_TEST_PARAM(BUFFER, chunk);
        FUNCTION_TEST_PARAM(SIZE, chunkSizeMax);
    FUNCTION_TEST_END();

    ASSERT(input != NULL);
    ASSERT(chunk != NULL);
    ASSERT(chunkSizeMax > 0);

    // Resize chunk buffer if it is less than max chunk size
    size_t chunkSize = bufSize(chunk);

    if (chunkSize < chunkSizeMax)
    {
        // If the input buffer is full there is very likely more data so increase size of the chunk buffer aggressively
        if (bufFull(input))
        {
            // If this is the first write set chunk size equal to double the input buffer size
            if (chunkSize == 0)
            {
                chunkSize = bufSize(input) * 2;
            }
            // Else double chunk size so as not to resize the chunk buffer too many times since prior data must be copied
            else
                chunkSize *= 2;
        }
        // Else no more writes are expected so allocate only what is needed
        else
            chunkSize += bufUsed(input) - bufRemains(chunk);

        // Chunk size cannot be larger than max chunk size
        if (chunkSize > chunkSizeMax)
            chunkSize = chunkSizeMax;

        // Resize the chunk buffer
        bufResize(chunk, chunkSize);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageWriteToLog(const StorageWrite *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(storageWriteType(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcFmt(
        debugLog, ", name: %s, modeFile: %04o, modePath: %04o, createPath: %s, syncFile: %s, syncPath: %s, atomic: %s}",
        strZ(storageWriteName(this)), storageWriteModeFile(this), storageWriteModePath(this),
        cvtBoolToConstZ(storageWriteCreatePath(this)), cvtBoolToConstZ(storageWriteSyncFile(this)),
        cvtBoolToConstZ(storageWriteSyncPath(this)), cvtBoolToConstZ(storageWriteAtomic(this)));
}
