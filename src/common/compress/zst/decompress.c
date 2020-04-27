/***********************************************************************************************************************************
ZST Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include "common/compress/zst/common.h"
#include "common/compress/zst/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(ZST_DECOMPRESS_FILTER_TYPE_STR,                       ZST_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define ZST_DECOMPRESS_TYPE                                         ZstDecompress
#define ZST_DECOMPRESS_PREFIX                                       zstDecompress

typedef struct ZstDecompress
{
    MemContext *memContext;                                         // Context to store data
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
} ZstDecompress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
zstDecompressToLog(const ZstDecompress *this)
{
    return strNewFmt("{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_ZST_DECOMPRESS_TYPE                                                                                           \
    ZstDecompress *
#define FUNCTION_LOG_ZST_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, zstDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free decompression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(ZST_DECOMPRESS, LOG, logLevelTrace)
{
    // !!!
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
zstDecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *decompressed)
{
    THIS(ZstDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ZST_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, decompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    // ASSERT(this->context != NULL);
    ASSERT(decompressed != NULL);

    // !!! IMPLEMENT

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
static bool
zstDecompressDone(const THIS_VOID)
{
    THIS(const ZstDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
zstDecompressInputSame(const THIS_VOID)
{
    THIS(const ZstDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
zstDecompressNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ZstDecompress")
    {
        ZstDecompress *driver = memNew(sizeof(ZstDecompress));

        *driver = (ZstDecompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        // Create zst context
        // !!!

        // Set callback to ensure zst context is freed
        memContextCallbackSet(driver->memContext, zstDecompressFreeResource, driver);

        // Create filter interface
        this = ioFilterNewP(
            ZST_DECOMPRESS_FILTER_TYPE_STR, driver, NULL, .done = zstDecompressDone, .inOut = zstDecompressProcess,
            .inputSame = zstDecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBZST
