/***********************************************************************************************************************************
Gzip Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <zlib.h>

#include "common/compress/gzip/common.h"
#include "common/compress/gzip/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define GZIP_DECOMPRESS_FILTER_TYPE                                 "gzipDecompress"
    STRING_STATIC(GZIP_DECOMPRESS_FILTER_TYPE_STR,                  GZIP_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct GzipDecompress
{
    MemContext *memContext;                                         // Context to store data
    z_stream *stream;                                               // Decompression stream state

    int result;                                                     // Result of last operation
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
} GzipDecompress;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *
gzipDecompressToLog(const GzipDecompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        this->stream->avail_in);
}

#define FUNCTION_LOG_GZIP_DECOMPRESS_TYPE                                                                                          \
    GzipDecompress *
#define FUNCTION_LOG_GZIP_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, gzipDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
void
gzipDecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *uncompressed)
{
    THIS(GzipDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZIP_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->stream != NULL);
    ASSERT(compressed != NULL);
    ASSERT(uncompressed != NULL);

    if (!this->inputSame)
    {
        this->stream->avail_in = (unsigned int)bufUsed(compressed);
        this->stream->next_in = bufPtr(compressed);
    }

    this->stream->avail_out = (unsigned int)bufRemains(uncompressed);
    this->stream->next_out = bufPtr(uncompressed) + bufUsed(uncompressed);

    this->result = gzipError(inflate(this->stream, Z_NO_FLUSH));

    // Set buffer used space
    bufUsedSet(uncompressed, bufSize(uncompressed) - (size_t)this->stream->avail_out);

    // Is decompression done?
    this->done = this->result == Z_STREAM_END;

    // Is the same input expected on the next call?
    this->inputSame = this->done ? false : this->stream->avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
bool
gzipDecompressDone(const THIS_VOID)
{
    THIS(const GzipDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
bool
gzipDecompressInputSame(const THIS_VOID)
{
    THIS(const GzipDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
static void
gzipDecompressFree(GzipDecompress *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        inflateEnd(this->stream);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
gzipDecompressNew(bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("GzipDecompress")
    {
        // Allocate state and set context
        GzipDecompress *driver = memNew(sizeof(GzipDecompress));
        driver->memContext = MEM_CONTEXT_NEW();

        // Create gzip stream
        driver->stream = memNew(sizeof(z_stream));
        gzipError(driver->result = inflateInit2(driver->stream, gzipWindowBits(raw)));

        // Set free callback to ensure gzip context is freed
        memContextCallbackSet(driver->memContext, (MemContextCallback)gzipDecompressFree, driver);

        // Create filter interface
        this = ioFilterNewP(
            GZIP_DECOMPRESS_FILTER_TYPE_STR, driver, .done = gzipDecompressDone, .inOut = gzipDecompressProcess,
            .inputSame = gzipDecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
