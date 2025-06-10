/***********************************************************************************************************************************
Gz Decompress

Based on the documentation at https://github.com/madler/zlib/blob/master/zlib.h
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <zlib.h>

#include "common/compress/common.h"
#include "common/compress/gz/common.h"
#include "common/compress/gz/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct GzDecompress
{
    z_stream stream;                                                // Decompression stream state

    int result;                                                     // Result of last operation
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
} GzDecompress;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
gzDecompressToLog(const GzDecompress *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{inputSame: %s, done: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        this->stream.avail_in);
}

#define FUNCTION_LOG_GZ_DECOMPRESS_TYPE                                                                                            \
    GzDecompress *
#define FUNCTION_LOG_GZ_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, gzDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free inflate stream
***********************************************************************************************************************************/
static void
gzDecompressFreeResource(THIS_VOID)
{
    THIS(GzDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZ_DECOMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    inflateEnd(&this->stream);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
gzDecompressProcess(THIS_VOID, const Buffer *const compressed, Buffer *const uncompressed)
{
    THIS(GzDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZ_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(uncompressed != NULL);

    // There should never be a flush because in a valid compressed stream the end of data can be determined and done will be set.
    // If a flush is received it means the compressed stream terminated early, e.g. a zero-length or truncated file.
    if (compressed == NULL)
        THROW(FormatError, "unexpected eof in compressed data");

    if (!this->inputSame)
    {
        this->stream.avail_in = (unsigned int)bufUsed(compressed);

        // Not all versions of zlib (and none by default) will accept const input buffers
        this->stream.next_in = bufPtrConst(compressed);
    }

    this->stream.avail_out = (unsigned int)bufRemains(uncompressed);
    this->stream.next_out = bufPtr(uncompressed) + bufUsed(uncompressed);

    this->result = gzError(inflate(&this->stream, Z_NO_FLUSH));

    // Set buffer used space
    bufUsedSet(uncompressed, bufSize(uncompressed) - (size_t)this->stream.avail_out);

    // Is decompression done?
    this->done = this->result == Z_STREAM_END;

    // Is the same input expected on the next call?
    this->inputSame = this->done ? false : this->stream.avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
static bool
gzDecompressDone(const THIS_VOID)
{
    THIS(const GzDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZ_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
gzDecompressInputSame(const THIS_VOID)
{
    THIS(const GzDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZ_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
gzDecompressNew(const bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(GzDecompress, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (GzDecompress)
        {
            .stream = {.zalloc = NULL},
        };

        // Create gz stream
        gzError(this->result = inflateInit2(&this->stream, (raw ? 0 : WANT_GZ) | WINDOW_BITS));

        // Set free callback to ensure gz context is freed
        memContextCallbackSet(objMemContext(this), gzDecompressFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            GZ_DECOMPRESS_FILTER_TYPE, this, decompressParamList(raw), .done = gzDecompressDone, .inOut = gzDecompressProcess,
            .inputSame = gzDecompressInputSame));
}
