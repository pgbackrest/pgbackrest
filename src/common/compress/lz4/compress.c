/***********************************************************************************************************************************
LZ4 Compress
***********************************************************************************************************************************/
#ifndef WITHOUT_LZ4

#include <stdio.h>
#include <lz4frame.h>

#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define LZ4_COMPRESS_FILTER_TYPE                                    "lz4Compress"
    STRING_STATIC(LZ4_COMPRESS_FILTER_TYPE_STR,                     LZ4_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Lz4Compress
{
    MemContext *memContext;                                         // Context to store data
    LZ4F_compressionContext_t context;                              // LZ4 compression context
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
Lz4Compress *
lz4CompressNew(int level, bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    ASSERT(level >= -1 && level <= 9);

    Lz4Compress *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Compress")
    {
        // Allocate state and set context
        this = memNew(sizeof(Lz4Compress));
        this->memContext = MEM_CONTEXT_NEW();

        // Create lz4 context
        lz4Error(LZ4F_createCompressionContext(&this->context, LZ4F_VERSION));

        // Set free callback to ensure lz4 context is freed
        memContextCallback(this->memContext, (MemContextCallback)lz4CompressFree, this);

        // Create filter interface
        this->filter = ioFilterNewP(
            LZ4_COMPRESS_FILTER_TYPE_STR, this, .done = (IoFilterInterfaceDone)lz4CompressDone,
            .inOut = (IoFilterInterfaceProcessInOut)lz4CompressProcess,
            .inputSame = (IoFilterInterfaceInputSame)lz4CompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(LZ4_COMPRESS, this);
}

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
void
lz4CompressProcess(Lz4Compress *this, const Buffer *uncompressed, Buffer *compressed)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->done);
    ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(!this->flush || uncompressed == NULL);
    // ASSERT(this->flush || (!this->inputSame || this->stream->avail_in != 0));

    // // Flushing
    // if (uncompressed == NULL)
    // {
    //     this->stream->avail_in = 0;
    //     this->flush = true;
    // }
    // // More input
    // else
    // {
    //     // Is new input allowed?
    //     if (!this->inputSame)
    //     {
    //         this->stream->avail_in = (unsigned int)bufUsed(uncompressed);
    //         this->stream->next_in = bufPtr(uncompressed);
    //     }
    // }
    //
    // // Initialize compressed output buffer
    // this->stream->avail_out = (unsigned int)bufRemains(compressed);
    // this->stream->next_out = bufPtr(compressed) + bufUsed(compressed);
    //
    // // Perform compression
    // lz4Error(deflate(this->stream, this->flush ? Z_FINISH : Z_NO_FLUSH));
    //
    // // Set buffer used space
    // bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream->avail_out);
    //
    // // Is compression done?
    // if (this->flush && this->stream->avail_out > 0)
    //     this->done = true;
    //
    // // Can more input be provided on the next call?
    // this->inputSame = this->flush ? !this->done : this->stream->avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
bool
lz4CompressDone(const Lz4Compress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
lz4CompressFilter(const Lz4Compress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filter);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
bool
lz4CompressInputSame(const Lz4Compress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
lz4CompressToLog(const Lz4Compress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, flushing: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        cvtBoolToConstZ(this->done));
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
lz4CompressFree(Lz4Compress *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        LZ4F_freeCompressionContext(this->context);
        this->context = NULL;

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

#endif // WITHOUT_LZ4
