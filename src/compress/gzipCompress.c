/***********************************************************************************************************************************
Gzip Compress
***********************************************************************************************************************************/
#include <stdio.h>
#include <zlib.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "compress/gzip.h"
#include "compress/gzipCompress.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define GZIP_COMPRESS_FILTER_TYPE                                   "gzipCompress"
    STRING_STATIC(GZIP_COMPRESS_FILTER_TYPE_STR,                    GZIP_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct GzipCompress
{
    MemContext *memContext;                                         // Context to store data
    z_stream *stream;                                               // Compression stream state
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flush;                                                     // Is input complete and flushing in progress?
    bool done;                                                      // Is compression done?
};

/***********************************************************************************************************************************
Compression constants
***********************************************************************************************************************************/
#define MEM_LEVEL                                                   9

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
GzipCompress *
gzipCompressNew(int level, bool raw)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, level);
        FUNCTION_DEBUG_PARAM(BOOL, raw);

        FUNCTION_DEBUG_ASSERT(level >= -1 && level <= 9);
    FUNCTION_DEBUG_END();

    GzipCompress *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("GzipCompress")
    {
        // Allocate state and set context
        this = memNew(sizeof(GzipCompress));
        this->memContext = MEM_CONTEXT_NEW();

        // Create gzip stream
        this->stream = memNew(sizeof(z_stream));
        gzipError(deflateInit2(this->stream, level, Z_DEFLATED, gzipWindowBits(raw), MEM_LEVEL, Z_DEFAULT_STRATEGY));

        // Set free callback to ensure gzip context is freed
        memContextCallback(this->memContext, (MemContextCallback)gzipCompressFree, this);

        // Create filter interface
        this->filter = ioFilterNewP(
            GZIP_COMPRESS_FILTER_TYPE_STR, this, .done = (IoFilterInterfaceDone)gzipCompressDone,
            .inOut = (IoFilterInterfaceProcessInOut)gzipCompressProcess,
            .inputSame = (IoFilterInterfaceInputSame)gzipCompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(GZIP_COMPRESS, this);
}

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
void
gzipCompressProcess(GzipCompress *this, const Buffer *uncompressed, Buffer *compressed)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(GZIP_COMPRESS, this);
        FUNCTION_DEBUG_PARAM(BUFFER, uncompressed);
        FUNCTION_DEBUG_PARAM(BUFFER, compressed);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(!this->done);
        FUNCTION_DEBUG_ASSERT(this->stream != NULL);
        FUNCTION_DEBUG_ASSERT(compressed != NULL);
        FUNCTION_DEBUG_ASSERT(!this->flush || uncompressed == NULL);
        FUNCTION_DEBUG_ASSERT(this->flush || (!this->inputSame || this->stream->avail_in != 0));
    FUNCTION_DEBUG_END();

    // Flushing
    if (uncompressed == NULL)
    {
        this->stream->avail_in = 0;
        this->flush = true;
    }
    // More input
    else
    {
        // Is new input allowed?
        if (!this->inputSame)
        {
            this->stream->avail_in = (unsigned int)bufUsed(uncompressed);
            this->stream->next_in = bufPtr(uncompressed);
        }
    }

    // Initialize compressed output buffer
    this->stream->avail_out = (unsigned int)bufRemains(compressed);
    this->stream->next_out = bufPtr(compressed) + bufUsed(compressed);

    // Perform compression
    gzipError(deflate(this->stream, this->flush ? Z_FINISH : Z_NO_FLUSH));

    // Set buffer used space
    bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream->avail_out);

    // Is compression done?
    if (this->flush && this->stream->avail_out > 0)
        this->done = true;

    // Can more input be provided on the next call?
    this->inputSame = this->flush ? !this->done : this->stream->avail_in != 0;

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
bool
gzipCompressDone(const GzipCompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_COMPRESS, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->done);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
gzipCompressFilter(const GzipCompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_COMPRESS, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_FILTER, this->filter);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
bool
gzipCompressInputSame(const GzipCompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_COMPRESS, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->inputSame);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
gzipCompressToLog(const GzipCompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, flushing: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        cvtBoolToConstZ(this->done), this->stream->avail_in);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
gzipCompressFree(GzipCompress *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(GZIP_COMPRESS, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        deflateEnd(this->stream);
        this->stream = NULL;

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
