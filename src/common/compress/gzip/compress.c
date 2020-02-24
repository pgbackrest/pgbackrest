/***********************************************************************************************************************************
Gzip Compress
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <zlib.h>

#include "common/compress/gzip/common.h"
#include "common/compress/gzip/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(GZIP_COMPRESS_FILTER_TYPE_STR,                        GZIP_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define GZIP_COMPRESS_TYPE                                          GzipCompress
#define GZIP_COMPRESS_PREFIX                                        gzipCompress

typedef struct GzipCompress
{
    MemContext *memContext;                                         // Context to store data
    z_stream stream;                                                // Compression stream state

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flush;                                                     // Is input complete and flushing in progress?
    bool done;                                                      // Is compression done?
} GzipCompress;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
gzipCompressToLog(const GzipCompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, flushing: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        cvtBoolToConstZ(this->done), this->stream.avail_in);
}

#define FUNCTION_LOG_GZIP_COMPRESS_TYPE                                                                                            \
    GzipCompress *
#define FUNCTION_LOG_GZIP_COMPRESS_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, gzipCompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Compression constants
***********************************************************************************************************************************/
#define MEM_LEVEL                                                   9

/***********************************************************************************************************************************
Free deflate stream
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(GZIP_COMPRESS, LOG, logLevelTrace)
{
    deflateEnd(&this->stream);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
static void
gzipCompressProcess(THIS_VOID, const Buffer *uncompressed, Buffer *compressed)
{
    THIS(GzipCompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZIP_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->done);
    ASSERT(compressed != NULL);
    ASSERT(!this->flush || uncompressed == NULL);
    ASSERT(this->flush || (!this->inputSame || this->stream.avail_in != 0));

    // Flushing
    if (uncompressed == NULL)
    {
        this->stream.avail_in = 0;
        this->flush = true;
    }
    // More input
    else
    {
        // Is new input allowed?
        if (!this->inputSame)
        {
            this->stream.avail_in = (unsigned int)bufUsed(uncompressed);
            this->stream.next_in = bufPtr(uncompressed);
        }
    }

    // Initialize compressed output buffer
    this->stream.avail_out = (unsigned int)bufRemains(compressed);
    this->stream.next_out = bufPtr(compressed) + bufUsed(compressed);

    // Perform compression
    gzipError(deflate(&this->stream, this->flush ? Z_FINISH : Z_NO_FLUSH));

    // Set buffer used space
    bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream.avail_out);

    // Is compression done?
    if (this->flush && this->stream.avail_out > 0)
        this->done = true;

    // Can more input be provided on the next call?
    this->inputSame = this->flush ? !this->done : this->stream.avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
gzipCompressDone(const THIS_VOID)
{
    THIS(const GzipCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
gzipCompressInputSame(const THIS_VOID)
{
    THIS(const GzipCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
gzipCompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level >= -1 && level <= 9);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("GzipCompress")
    {
        GzipCompress *driver = memNew(sizeof(GzipCompress));

        *driver = (GzipCompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .stream = {.zalloc = NULL},
        };

        // Create gzip stream
        gzipError(deflateInit2(&driver->stream, level, Z_DEFLATED, WANT_GZIP | WINDOW_BITS, MEM_LEVEL, Z_DEFAULT_STRATEGY));

        // Set free callback to ensure gzip context is freed
        memContextCallbackSet(driver->memContext, gzipCompressFreeResource, driver);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewInt(level));

        // Create filter interface
        this = ioFilterNewP(
            GZIP_COMPRESS_FILTER_TYPE_STR, driver, paramList, .done = gzipCompressDone, .inOut = gzipCompressProcess,
            .inputSame = gzipCompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
