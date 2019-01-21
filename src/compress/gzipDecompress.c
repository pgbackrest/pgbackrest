/***********************************************************************************************************************************
Gzip Decompress
***********************************************************************************************************************************/
#include <stdio.h>
#include <zlib.h>

#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "compress/gzip.h"
#include "compress/gzipDecompress.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define GZIP_DECOMPRESS_FILTER_TYPE                                 "gzipDecompress"
    STRING_STATIC(GZIP_DECOMPRESS_FILTER_TYPE_STR,                  GZIP_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct GzipDecompress
{
    MemContext *memContext;                                         // Context to store data
    z_stream *stream;                                               // Decompression stream state
    IoFilter *filter;                                               // Filter interface

    int result;                                                     // Result of last operation
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
GzipDecompress *
gzipDecompressNew(bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    GzipDecompress *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("GzipDecompress")
    {
        // Allocate state and set context
        this = memNew(sizeof(GzipDecompress));
        this->memContext = MEM_CONTEXT_NEW();

        // Create gzip stream
        this->stream = memNew(sizeof(z_stream));
        gzipError(this->result = inflateInit2(this->stream, gzipWindowBits(raw)));

        // Set free callback to ensure gzip context is freed
        memContextCallback(this->memContext, (MemContextCallback)gzipDecompressFree, this);

        // Create filter interface
        this->filter = ioFilterNewP(
            GZIP_DECOMPRESS_FILTER_TYPE_STR, this, .done = (IoFilterInterfaceDone)gzipDecompressDone,
            .inOut = (IoFilterInterfaceProcessInOut)gzipDecompressProcess,
            .inputSame = (IoFilterInterfaceInputSame)gzipDecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(GZIP_DECOMPRESS, this);
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
void
gzipDecompressProcess(GzipDecompress *this, const Buffer *compressed, Buffer *uncompressed)
{
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
gzipDecompressDone(const GzipDecompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
gzipDecompressFilter(const GzipDecompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_FILTER, this->filter);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
bool
gzipDecompressInputSame(const GzipDecompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(GZIP_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
gzipDecompressToLog(const GzipDecompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, availIn: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        this->stream->avail_in);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
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
