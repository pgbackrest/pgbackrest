/***********************************************************************************************************************************
LZ4 Compress
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
#define LZ4_COMPRESS_FILTER_TYPE                                    "lz4Compress"
    STRING_STATIC(LZ4_COMPRESS_FILTER_TYPE_STR,                     LZ4_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define LZ4_COMPRESS_TYPE                                           Lz4Compress
#define LZ4_COMPRESS_PREFIX                                         lz4Compress

struct Lz4Compress
{
    MemContext *memContext;                                         // Context to store data
    LZ4F_compressionContext_t context;                              // LZ4 compression context
    IoFilter *filter;                                               // Filter interface
    Buffer *buffer;                                                 // For when the output buffer can't accept all compressed data
    bool first;                                                     // Is this the first call to process?

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
};

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
lz4CompressToLog(const Lz4Compress *this)
{
    return strNewFmt(
        "{first: %s, inputSame: %s, flushing: %s}", cvtBoolToConstZ(this->first), cvtBoolToConstZ(this->inputSame),
        cvtBoolToConstZ(this->flushing));
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
// Helper to return a buffer where output will be written
static Buffer *
lz4CompressBuffer(Lz4Compress *this, size_t required, Buffer *output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(SIZE, required);
        FUNCTION_TEST_PARAM(BUFFER, output);
    FUNCTION_TEST_END();

    Buffer *result = output;

    if (required >= bufRemains(output) || (this->buffer != NULL && bufUsed(this->buffer) > 0))
    {
        // If buffer has not been allocated yet
        if (this->buffer == NULL)
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->buffer = bufNew(required);
            }
            MEM_CONTEXT_END();
        }
        // Else buffer is not large enough
        else if (required >= bufRemains(this->buffer))
            bufResize(this->buffer, bufUsed(this->buffer) + required);

        result = this->buffer;
    }

    FUNCTION_TEST_RETURN(result);
}

// Helper to copy data to compressed buffer
static void
lz4CompressCopy(Lz4Compress *this, Buffer *output, Buffer *compressed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(BUFFER, output);
        FUNCTION_TEST_PARAM(BUFFER, compressed);
    FUNCTION_TEST_END();

    if (bufRemains(compressed) >= bufUsed(output))
    {
        bufCat(compressed, output);
        bufUsedZero(output);

        this->inputSame = false;
    }
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

    if (this->inputSame)
    {
        lz4CompressCopy(this, this->buffer, compressed);
    }
    else
    {
        Buffer *output = NULL;

        // If first call to process then begin compression
        if (this->first)
        {
            output = lz4CompressBuffer(this, LZ4F_HEADER_SIZE_MAX, compressed);
            bufUsedInc(output, LZ4F_compressBegin(this->context, bufRemainsPtr(output), bufRemains(output), NULL));

            this->first = false;
        }

        // Normal processing call
        if (uncompressed != NULL)
        {
            output = lz4CompressBuffer(this, LZ4F_compressBound(bufUsed(uncompressed), NULL), compressed);
            bufUsedInc(
                output,
                LZ4F_compressUpdate(
                    this->context, bufRemainsPtr(output), bufRemains(output), bufPtr(uncompressed), bufUsed(uncompressed),
                    NULL));
        }
        // Else flush remaining output
        else
        {
            output = lz4CompressBuffer(this, LZ4F_compressBound(0, NULL), compressed);
            bufUsedInc(output, LZ4F_compressEnd(this->context, bufRemainsPtr(output), bufRemains(output), NULL));

            this->flushing = true;
        }

        // If the output buffer was allocated locally it will need to be copied to the compressed buffer
        if (output != compressed)
            lz4CompressCopy(this, output, compressed);
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

    ASSERT(level >= -1 && level <= 9);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Compress")
    {
        Lz4Compress *driver = memNew(sizeof(Lz4Compress));

        *driver = (Lz4Compress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .first = true,
        };

        // Create lz4 context
        lz4Error(LZ4F_createCompressionContext(&driver->context, LZ4F_VERSION));

        // Set free callback to ensure lz4 context is freed
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

IoFilter *
lz4CompressNewVar(const VariantList *paramList)
{
    return lz4CompressNew(varIntForce(varLstGet(paramList, 0)));
}

#endif // HAVE_LIBLZ4
