/***********************************************************************************************************************************
LZ4 Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBLZ4

#include <stdio.h>
#include <lz4frame.h>

#include "common/compress/lz4/common.h"
#include "common/compress/lz4/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define LZ4_DECOMPRESS_FILTER_TYPE                                  "lz4Decompress"
    STRING_STATIC(LZ4_DECOMPRESS_FILTER_TYPE_STR,                   LZ4_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define LZ4_DECOMPRESS_TYPE                                         Lz4Decompress
#define LZ4_DECOMPRESS_PREFIX                                       lz4Decompress

struct Lz4Decompress
{
    MemContext *memContext;                                         // Context to store data
    LZ4F_decompressionContext_t context;                            // LZ4 decompression context
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
};

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
lz4DecompressToLog(const Lz4Decompress *this)
{
    return strNewFmt("{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_LZ4_DECOMPRESS_TYPE                                                                                           \
    Lz4Decompress *
#define FUNCTION_LOG_LZ4_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, lz4DecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free decompression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(LZ4_DECOMPRESS, LOG, logLevelTrace)
{
    LZ4F_freeDecompressionContext(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
lz4DecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *uncompressed)
{
    THIS(Lz4Decompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(uncompressed != NULL);

    // if (!this->inputSame)
    // {
    //     this->stream->avail_in = (unsigned int)bufUsed(compressed);
    //     this->stream->next_in = bufPtr(compressed);
    // }
    //
    // this->stream->avail_out = (unsigned int)bufRemains(uncompressed);
    // this->stream->next_out = bufPtr(uncompressed) + bufUsed(uncompressed);
    //
    // this->result = lz4Error(inflate(this->stream, Z_NO_FLUSH));
    //
    // // Set buffer used space
    // bufUsedSet(uncompressed, bufSize(uncompressed) - (size_t)this->stream->avail_out);
    //
    // // Is decompression done?
    // this->done = this->result == Z_STREAM_END;
    //
    // // Is the same input expected on the next call?
    // this->inputSame = this->done ? false : this->stream->avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
static bool
lz4DecompressDone(const THIS_VOID)
{
    THIS(const Lz4Decompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
lz4DecompressInputSame(const THIS_VOID)
{
    THIS(const Lz4Decompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
lz4DecompressNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Decompress")
    {
        Lz4Decompress *driver = memNew(sizeof(Lz4Decompress));

        *driver = (Lz4Decompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        // Create lz4 stream
        lz4Error(LZ4F_createDecompressionContext(&driver->context, LZ4F_VERSION));

        // Set free callback to ensure lz4 context is freed
        memContextCallbackSet(driver->memContext, lz4DecompressFreeResource, driver);

        // Create filter interface
        this = ioFilterNewP(
            LZ4_DECOMPRESS_FILTER_TYPE_STR, this, NULL, .done = lz4DecompressDone, .inOut = lz4DecompressProcess,
            .inputSame = lz4DecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBLZ4
