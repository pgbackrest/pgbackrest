/***********************************************************************************************************************************
Gz Compress

Based on the documentation at https://github.com/madler/zlib/blob/master/zlib.h
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <zlib.h>

#include "common/compress/common.h"
#include "common/compress/gz/common.h"
#include "common/compress/gz/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct GzCompress
{
    z_stream stream;                                                // Compression stream state

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
    bool done;                                                      // Is compression done?
} GzCompress;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
gzCompressToLog(const GzCompress *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{inputSame: %s, done: %s, flushing: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame),
        cvtBoolToConstZ(this->done), cvtBoolToConstZ(this->flushing), this->stream.avail_in);
}

#define FUNCTION_LOG_GZ_COMPRESS_TYPE                                                                                              \
    GzCompress *
#define FUNCTION_LOG_GZ_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_OBJECT_FORMAT(value, gzCompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Compression constants
***********************************************************************************************************************************/
#define MEM_LEVEL                                                   9

/***********************************************************************************************************************************
Free deflate stream
***********************************************************************************************************************************/
static void
gzCompressFreeResource(THIS_VOID)
{
    THIS(GzCompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZ_COMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    deflateEnd(&this->stream);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
static void
gzCompressProcess(THIS_VOID, const Buffer *const uncompressed, Buffer *const compressed)
{
    THIS(GzCompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZ_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->done);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);
    ASSERT(this->flushing || (!this->inputSame || this->stream.avail_in != 0));

    // Flushing
    if (uncompressed == NULL)
    {
        this->stream.avail_in = 0;
        this->flushing = true;
    }
    // More input
    else
    {
        // Is new input allowed?
        if (!this->inputSame)
        {
            this->stream.avail_in = (unsigned int)bufUsed(uncompressed);

            // Not all versions of zlib (and none by default) will accept const input buffers
            this->stream.next_in = bufPtrConst(uncompressed);
        }
    }

    // Initialize compressed output buffer
    this->stream.avail_out = (unsigned int)bufRemains(compressed);
    this->stream.next_out = bufPtr(compressed) + bufUsed(compressed);

    // Perform compression
    const int result = gzError(deflate(&this->stream, this->flushing ? Z_FINISH : Z_NO_FLUSH));

    // Set buffer used space
    bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream.avail_out);

    // Is compression done?
    if (this->flushing && result == Z_STREAM_END)
        this->done = true;

    // Can more input be provided on the next call?
    this->inputSame = this->flushing ? !this->done : this->stream.avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
gzCompressDone(const THIS_VOID)
{
    THIS(const GzCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZ_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
gzCompressInputSame(const THIS_VOID)
{
    THIS(const GzCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZ_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
gzCompressNew(const int level, const bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    ASSERT(level >= GZ_COMPRESS_LEVEL_MIN && level <= GZ_COMPRESS_LEVEL_MAX);

    OBJ_NEW_BEGIN(GzCompress, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (GzCompress)
        {
            .stream = {.zalloc = NULL},
        };

        // Create gz stream
        gzError(deflateInit2(&this->stream, level, Z_DEFLATED, (raw ? 0 : WANT_GZ) | WINDOW_BITS, MEM_LEVEL, Z_DEFAULT_STRATEGY));

        // Set free callback to ensure gz context is freed
        memContextCallbackSet(objMemContext(this), gzCompressFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            GZ_COMPRESS_FILTER_TYPE, this, compressParamList(level, raw), .done = gzCompressDone, .inOut = gzCompressProcess,
            .inputSame = gzCompressInputSame));
}
