/***********************************************************************************************************************************
LZ4 Decompress
***********************************************************************************************************************************/
#ifndef WITHOUT_LZ4

#include <stdio.h>
#include <lz4frame.h>

#include "common/compress/lz4/common.h"
#include "common/compress/lz4/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define LZ4_DECOMPRESS_FILTER_TYPE                                  "lz4Decompress"
    STRING_STATIC(LZ4_DECOMPRESS_FILTER_TYPE_STR,                   LZ4_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Lz4Decompress
{
    MemContext *memContext;                                         // Context to store data
    LZ4F_decompressionContext_t context;                            // LZ4 decompression context
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Lz4Decompress *
lz4DecompressNew(bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    Lz4Decompress *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Lz4Decompress")
    {
        // Allocate state and set context
        this = memNew(sizeof(Lz4Decompress));
        this->memContext = MEM_CONTEXT_NEW();

        // Create lz4 stream
        lz4Error(LZ4F_createDecompressionContext(&this->context, LZ4F_VERSION));

        // Set free callback to ensure lz4 context is freed
        memContextCallback(this->memContext, (MemContextCallback)lz4DecompressFree, this);

        // Create filter interface
        this->filter = ioFilterNewP(
            LZ4_DECOMPRESS_FILTER_TYPE_STR, this, .done = (IoFilterInterfaceDone)lz4DecompressDone,
            .inOut = (IoFilterInterfaceProcessInOut)lz4DecompressProcess,
            .inputSame = (IoFilterInterfaceInputSame)lz4DecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(LZ4_DECOMPRESS, this);
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
void
lz4DecompressProcess(Lz4Decompress *this, const Buffer *compressed, Buffer *uncompressed)
{
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
bool
lz4DecompressDone(const Lz4Decompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
lz4DecompressFilter(const Lz4Decompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filter);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
bool
lz4DecompressInputSame(const Lz4Decompress *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
lz4DecompressToLog(const Lz4Decompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
lz4DecompressFree(Lz4Decompress *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        LZ4F_freeDecompressionContext(this->context);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

#endif // WITH_LZ4
