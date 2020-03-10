/***********************************************************************************************************************************
LZ4 Compress

Developed against version r131 using the documentation in https://github.com/lz4/lz4/blob/r131/lib/lz4frame.h.
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBLZ4

#include <stdio.h>
#include <lz4frame.h>
#include <string.h>

#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"

/***********************************************************************************************************************************
Older versions of lz4 do not define the max header size.  This seems to be the max for any version.
***********************************************************************************************************************************/
#ifndef LZ4F_HEADER_SIZE_MAX
    #define LZ4F_HEADER_SIZE_MAX                                    19
#endif

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(LZ4_COMPRESS_FILTER_TYPE_STR,                         LZ4_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define LZ4_COMPRESS_TYPE                                           Lz4Compress
#define LZ4_COMPRESS_PREFIX                                         lz4Compress

typedef struct Lz4Compress
{
    MemContext *memContext;                                         // Context to store data
    LZ4F_compressionContext_t context;                              // LZ4 compression context
    LZ4F_preferences_t prefs;                                       // Preferences -- just compress level set
    IoFilter *filter;                                               // Filter interface

    Buffer *buffer;                                                 // For when the output buffer can't accept all compressed data
    bool first;                                                     // Is this the first call to process?
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
} Lz4Compress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
lz4CompressToLog(const Lz4Compress *this)
{
    return strNewFmt(
        "{level: %d, first: %s, inputSame: %s, flushing: %s}", this->prefs.compressionLevel,
        cvtBoolToConstZ(this->first), cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->flushing));
}

#define FUNCTION_LOG_LZ4_COMPRESS_TYPE                                                                                             \
    Lz4Compress *
#define FUNCTION_LOG_LZ4_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, lz4CompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free compression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(LZ4_COMPRESS, LOG, logLevelTrace)
{
    LZ4F_freeCompressionContext(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
// Helper to return a buffer where output will be written.  If there is enough space in the provided output buffer then use it,
// otherwise allocate an internal buffer to hold the compressed data.  Once we start using the internal buffer we'll need to
// continue using it until it is completely flushed.
static Buffer *
lz4CompressBuffer(Lz4Compress *this, size_t required, Buffer *output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(SIZE, required);
        FUNCTION_TEST_PARAM(BUFFER, output);
    FUNCTION_TEST_END();

    Buffer *result = output;

    // Is an internal buffer required to hold the compressed data?
    if (bufUsed(this->buffer) > 0 || required >= bufRemains(output))
    {
        // Reallocate buffer if it is not large enough
        if (required >= bufRemains(this->buffer))
            bufResize(this->buffer, bufUsed(this->buffer) + required);

        result = this->buffer;
    }

    FUNCTION_TEST_RETURN(result);
}

// Helper to flush output data to compressed buffer
static void
lz4CompressFlush(Lz4Compress *this, Buffer *output, Buffer *compressed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(BUFFER, output);
        FUNCTION_TEST_PARAM(BUFFER, compressed);
    FUNCTION_TEST_END();

    // If the compressed buffer can hold all the output
    if (bufRemains(compressed) >= bufUsed(output))
    {
        bufCat(compressed, output);
        bufUsedZero(output);

        this->inputSame = false;
    }
    // Else flush as much as possible and set inputSame to flush more once the compressed buffer has been emptied
    else
    {
        size_t catSize = bufRemains(compressed);
        bufCatSub(compressed, output, 0, catSize);

        memmove(bufPtr(output), bufPtr(output) + catSize, bufUsed(output) - catSize);
        bufUsedSet(output, bufUsed(output) - catSize);

        this->inputSame = true;
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
lz4CompressProcess(THIS_VOID, const Buffer *uncompressed, Buffer *compressed)
{
    THIS(Lz4Compress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!(this->flushing && !this->inputSame));
    ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);

    // Flush overflow output to the compressed buffer
    if (this->inputSame)
    {
        lz4CompressFlush(this, this->buffer, compressed);
    }
    else
    {
        Buffer *output = NULL;

        // If first call to process then begin compression
        if (this->first)
        {
            output = lz4CompressBuffer(this, LZ4F_HEADER_SIZE_MAX, compressed);
            bufUsedInc(
                output, lz4Error(LZ4F_compressBegin(this->context, bufRemainsPtr(output), bufRemains(output), &this->prefs)));

            this->first = false;
        }

        // Normal processing call
        if (uncompressed != NULL)
        {
            output = lz4CompressBuffer(this, lz4Error(LZ4F_compressBound(bufUsed(uncompressed), &this->prefs)), compressed);

            bufUsedInc(
                output,
                lz4Error(
                    LZ4F_compressUpdate(
                        this->context, bufRemainsPtr(output), bufRemains(output), bufPtr(uncompressed), bufUsed(uncompressed),
                        NULL)));
        }
        // Else flush remaining output
        else
        {
            output = lz4CompressBuffer(this, lz4Error(LZ4F_compressBound(0, &this->prefs)), compressed);
            bufUsedInc(output, lz4Error(LZ4F_compressEnd(this->context, bufRemainsPtr(output), bufRemains(output), NULL)));

            this->flushing = true;
        }

        // If the output buffer was allocated locally it will need to be flushed to the compressed buffer
        if (output != compressed)
            lz4CompressFlush(this, output, compressed);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
lz4CompressDone(const THIS_VOID)
{
    THIS(const Lz4Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->flushing && !this->inputSame);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
lz4CompressInputSame(const THIS_VOID)
{
    THIS(const Lz4Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
lz4CompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level >= 0);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Compress")
    {
        Lz4Compress *driver = memNew(sizeof(Lz4Compress));

        *driver = (Lz4Compress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .prefs = {.compressionLevel = level, .frameInfo = {.contentChecksumFlag = LZ4F_contentChecksumEnabled}},
            .first = true,
            .buffer = bufNew(0),
        };

        // Create lz4 context
        lz4Error(LZ4F_createCompressionContext(&driver->context, LZ4F_VERSION));

        // Set callback to ensure lz4 context is freed
        memContextCallbackSet(driver->memContext, lz4CompressFreeResource, driver);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewInt(level));

        // Create filter interface
        this = ioFilterNewP(
            LZ4_COMPRESS_FILTER_TYPE_STR, driver, paramList, .done = lz4CompressDone, .inOut = lz4CompressProcess,
            .inputSame = lz4CompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBLZ4
