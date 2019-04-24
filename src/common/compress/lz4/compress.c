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
    Buffer *buffer;                                                 // For when the output buffer can't accept all compressed data
    bool first;                                                     // Is this the first call to process?

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
lz4CompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level >= -1 && level <= 9);

    Lz4Compress *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Compress")
    {
        // Allocate state and set context
        this = memNew(sizeof(Lz4Compress));
        this->memContext = MEM_CONTEXT_NEW();
        this->first = true;

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
Return a buffer where output will be written
***********************************************************************************************************************************/
static Buffer *
lz4CompressBuffer(Lz4Compress *this, size_t required, Buffer *output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
        FUNCTION_LOG_PARAM(SIZE, required);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    Buffer *result = output;

    if (required >= bufRemains(output) || lz4CompressInputSame(this))
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

        this->inputSame = true;
        result = this->buffer;
    }

    FUNCTION_LOG_RETURN(BUFFER, result);
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

    if (lz4CompressInputSame(this))
    {
        // !!! flush buffer here
    }
    else
    {
        // If first call to process then begin compression
        if (this->first)
        {
            Buffer *output = lz4CompressBuffer(this, 15, compressed);
            bufUsedInc(output, LZ4F_compressBegin(this->context, bufRemainsPtr(output), bufRemains(output), NULL));

            this->first = false;
        }

        // Normal processing call
        if (uncompressed != NULL)
        {
            Buffer *output = lz4CompressBuffer(this, LZ4F_compressBound(bufUsed(uncompressed), NULL), compressed);
            bufUsedInc(
                output, LZ4F_compressUpdate(this->context, bufRemainsPtr(output), bufRemains(output), bufPtr(uncompressed), NULL));
        }
        // Else flush remaining output
        else
        {
            this->flush = true;
        }
    }

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
        "{first: %s, inputSame: %s, done: %s, flushing: %s}", cvtBoolToConstZ(this->first), cvtBoolToConstZ(this->inputSame),
        cvtBoolToConstZ(this->done), cvtBoolToConstZ(this->done));
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
