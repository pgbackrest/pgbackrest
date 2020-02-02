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
STRING_EXTERN(GZIP_DECOMPRESS_FILTER_TYPE_STR,                      GZIP_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define GZIP_DECOMPRESS_TYPE                                        GzipDecompress
#define GZIP_DECOMPRESS_PREFIX                                      gzipDecompress

typedef struct GzipDecompress
{
    MemContext *memContext;                                         // Context to store data
    z_stream stream;                                                // Decompression stream state

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
        this->stream.avail_in);
}

#define FUNCTION_LOG_GZIP_DECOMPRESS_TYPE                                                                                          \
    GzipDecompress *
#define FUNCTION_LOG_GZIP_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, gzipDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free inflate stream
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(GZIP_DECOMPRESS, LOG, logLevelTrace)
{
    inflateEnd(&this->stream);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

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
    ASSERT(uncompressed != NULL);

    // There should never be a flush because in a valid compressed stream the end of data can be determined and done will be set.
    // If a flush is received it means the compressed stream terminated early, e.g. a zero-length or truncated file.
    if (compressed == NULL)
        THROW(FormatError, "unexpected eof in compressed data");

    if (!this->inputSame)
    {
        this->stream.avail_in = (unsigned int)bufUsed(compressed);
        this->stream.next_in = bufPtr(compressed);
    }

    this->stream.avail_out = (unsigned int)bufRemains(uncompressed);
    this->stream.next_out = bufPtr(uncompressed) + bufUsed(uncompressed);

    this->result = gzipError(inflate(&this->stream, Z_NO_FLUSH));

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

        *driver = (GzipDecompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .stream = {.zalloc = NULL},
        };

        // Create gzip stream
        gzipError(driver->result = inflateInit2(&driver->stream, gzipWindowBits(raw)));

        // Set free callback to ensure gzip context is freed
        memContextCallbackSet(driver->memContext, gzipDecompressFreeResource, driver);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewBool(raw));

        // Create filter interface
        this = ioFilterNewP(
            GZIP_DECOMPRESS_FILTER_TYPE_STR, driver, paramList, .done = gzipDecompressDone, .inOut = gzipDecompressProcess,
            .inputSame = gzipDecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
gzipDecompressNewVar(const VariantList *paramList)
{
    return gzipDecompressNew(varBool(varLstGet(paramList, 0)));
}
