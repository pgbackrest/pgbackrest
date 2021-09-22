/***********************************************************************************************************************************
BZ2 Compress
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <bzlib.h>

#include "common/compress/bz2/common.h"
#include "common/compress/bz2/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Bz2Compress
{
    bz_stream stream;                                               // Compression stream

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
    bool done;                                                      // Is compression done?
} Bz2Compress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
bz2CompressToLog(const Bz2Compress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, flushing: %s, avail_in: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        cvtBoolToConstZ(this->flushing), this->stream.avail_in);
}

#define FUNCTION_LOG_BZ2_COMPRESS_TYPE                                                                                             \
    Bz2Compress *
#define FUNCTION_LOG_BZ2_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, bz2CompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free compression stream
***********************************************************************************************************************************/
static void
bz2CompressFreeResource(THIS_VOID)
{
    THIS(Bz2Compress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BZ2_COMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    BZ2_bzCompressEnd(&this->stream);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
static void
bz2CompressProcess(THIS_VOID, const Buffer *uncompressed, Buffer *compressed)
{
    THIS(Bz2Compress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BZ2_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->done);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);
    ASSERT(this->flushing || (!this->inputSame || this->stream.avail_in != 0));

    // If input is NULL then start flushing
    if (uncompressed == NULL)
    {
        this->stream.avail_in = 0;
        this->flushing = true;
    }
    // Else still have input data
    else
    {
        // Is new input allowed?
        if (!this->inputSame)
        {
            this->stream.avail_in = (unsigned int)bufUsed(uncompressed);

            // bzip2 does not accept const input buffers
            this->stream.next_in = (char *)UNCONSTIFY(unsigned char *, bufPtrConst(uncompressed));
        }
    }

    // Initialize compressed output buffer
    this->stream.avail_out = (unsigned int)bufRemains(compressed);
    this->stream.next_out = (char *)bufPtr(compressed) + bufUsed(compressed);

    // Perform compression, check for error
    int result = bz2Error(BZ2_bzCompress(&this->stream, this->flushing ? BZ_FINISH : BZ_RUN));

    // Set buffer used space
    bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream.avail_out);

    // Is compression done?
    if (this->flushing && result == BZ_STREAM_END)
        this->done = true;

    // Can more input be provided on the next call?
    this->inputSame = this->flushing ? !this->done : this->stream.avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
bz2CompressDone(const THIS_VOID)
{
    THIS(const Bz2Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BZ2_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
bz2CompressInputSame(const THIS_VOID)
{
    THIS(const Bz2Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BZ2_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
bz2CompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level > 0);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(Bz2Compress)
    {
        Bz2Compress *driver = OBJ_NEW_ALLOC();

        *driver = (Bz2Compress)
        {
            .stream = {.bzalloc = NULL},
        };

        // Initialize context
        bz2Error(BZ2_bzCompressInit(&driver->stream, level, 0, 0));

        // Set callback to ensure bz2 stream is freed
        memContextCallbackSet(objMemContext(driver), bz2CompressFreeResource, driver);

        // Create param list
        Buffer *const paramList = bufNew(PACK_EXTRA_MIN);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewBuf(paramList);

            pckWriteI32P(packWrite, level);
            pckWriteEndP(packWrite);
        }
        MEM_CONTEXT_TEMP_END();

        // Create filter interface
        this = ioFilterNewP(
            BZ2_COMPRESS_FILTER_TYPE, driver, paramList, .done = bz2CompressDone, .inOut = bz2CompressProcess,
            .inputSame = bz2CompressInputSame);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
