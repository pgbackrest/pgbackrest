/***********************************************************************************************************************************
ZST Compress

Developed using the documentation in !!!
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include <zstd.h>

#include "common/compress/zst/common.h"
#include "common/compress/zst/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(ZST_COMPRESS_FILTER_TYPE_STR,                         ZST_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define ZST_COMPRESS_TYPE                                           ZstCompress
#define ZST_COMPRESS_PREFIX                                         zstCompress

typedef struct ZstCompress
{
    MemContext *memContext;                                         // Context to store data
    IoFilter *filter;                                               // Filter interface

    Buffer *buffer;                                                 // For when the output buffer can't accept all compressed data
    bool first;                                                     // Is this the first call to process?
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
} ZstCompress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
zstCompressToLog(const ZstCompress *this)
{
    return strNewFmt("{inputSame: %s, flushing: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->flushing));
}

#define FUNCTION_LOG_ZST_COMPRESS_TYPE                                                                                             \
    ZstCompress *
#define FUNCTION_LOG_ZST_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, zstCompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free compression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(ZST_COMPRESS, LOG, logLevelTrace)
{
    // !!!
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
static void
zstCompressProcess(THIS_VOID, const Buffer *uncompressed, Buffer *compressed)
{
    THIS(ZstCompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ZST_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!(this->flushing && !this->inputSame));
    // ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);

    bufCat(compressed, uncompressed);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
zstCompressDone(const THIS_VOID)
{
    THIS(const ZstCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->flushing && !this->inputSame);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
zstCompressInputSame(const THIS_VOID)
{
    THIS(const ZstCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
zstCompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level >= 0);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ZstCompress")
    {
        ZstCompress *driver = memNew(sizeof(ZstCompress));

        *driver = (ZstCompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        // Create zst context
        // !!! zstError(ZSTF_createCompressionContext(&driver->context, ZSTF_VERSION));

        // Set callback to ensure zst context is freed
        memContextCallbackSet(driver->memContext, zstCompressFreeResource, driver);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewInt(level));

        // Create filter interface
        this = ioFilterNewP(
            ZST_COMPRESS_FILTER_TYPE_STR, driver, paramList, .done = zstCompressDone, .inOut = zstCompressProcess,
            .inputSame = zstCompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBZST
