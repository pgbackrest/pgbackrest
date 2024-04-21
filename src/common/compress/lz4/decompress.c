/***********************************************************************************************************************************
LZ4 Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBLZ4

#include <lz4frame.h>
#include <stdio.h>

#include "common/compress/lz4/common.h"
#include "common/compress/lz4/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Lz4Decompress
{
    LZ4F_decompressionContext_t context;                            // LZ4 decompression context
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    size_t inputOffset;                                             // Current offset from the start of the buffer
    bool frameDone;                                                 // Has the current frame completed?
    bool done;                                                      // Is decompression done?
} Lz4Decompress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static void
lz4DecompressToLog(const Lz4Decompress *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{inputSame: %s, inputOffset: %zu, frameDone %s, done: %s}", cvtBoolToConstZ(this->inputSame),
        this->inputOffset, cvtBoolToConstZ(this->frameDone), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_LZ4_DECOMPRESS_TYPE                                                                                           \
    Lz4Decompress *
#define FUNCTION_LOG_LZ4_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_OBJECT_FORMAT(value, lz4DecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free decompression context
***********************************************************************************************************************************/
static void
lz4DecompressFreeResource(THIS_VOID)
{
    THIS(Lz4Decompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_DECOMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    LZ4F_freeDecompressionContext(this->context);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
lz4DecompressProcess(THIS_VOID, const Buffer *const compressed, Buffer *const decompressed)
{
    THIS(Lz4Decompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, decompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->context != NULL);
    ASSERT(decompressed != NULL);

    // When there is no more input then decompression is done
    if (compressed == NULL)
    {
        // If the current frame being decompressed was not completed then error
        if (!this->frameDone)
            THROW(FormatError, "unexpected eof in compressed data");

        this->done = true;
    }
    else
    {
        // Decompress as much data as possible
        size_t srcSize = bufUsed(compressed) - this->inputOffset;
        size_t dstSize = bufRemains(decompressed);

        this->frameDone = lz4Error(
            LZ4F_decompress(
                this->context, bufRemainsPtr(decompressed), &dstSize, bufPtrConst(compressed) + this->inputOffset, &srcSize,
                NULL)) == 0;

        bufUsedInc(decompressed, dstSize);

        // If the compressed data was not fully processed then update the offset and set inputSame
        if (srcSize < bufUsed(compressed) - this->inputOffset)
        {
            this->inputOffset += srcSize;
            this->inputSame = true;
        }
        // Else all compressed data was processed
        else
        {
            this->inputOffset = 0;
            this->inputSame = false;
        }
    }

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

    FUNCTION_TEST_RETURN(BOOL, this->done);
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

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
lz4DecompressNew(const bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        (void)raw;                                                  // Not required for decompress
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(Lz4Decompress, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (Lz4Decompress){0};

        // Create lz4 context
        lz4Error(LZ4F_createDecompressionContext(&this->context, LZ4F_VERSION));

        // Set callback to ensure lz4 context is freed
        memContextCallbackSet(objMemContext(this), lz4DecompressFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            LZ4_DECOMPRESS_FILTER_TYPE, this, NULL, .done = lz4DecompressDone, .inOut = lz4DecompressProcess,
            .inputSame = lz4DecompressInputSame));
}

#endif // HAVE_LIBLZ4
