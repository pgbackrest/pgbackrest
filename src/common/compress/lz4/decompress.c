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
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(LZ4_DECOMPRESS_FILTER_TYPE_STR,                       LZ4_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define LZ4_DECOMPRESS_TYPE                                         Lz4Decompress
#define LZ4_DECOMPRESS_PREFIX                                       lz4Decompress

typedef struct Lz4Decompress
{
    MemContext *memContext;                                         // Context to store data
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
static String *
lz4DecompressToLog(const Lz4Decompress *this)
{
    return strNewFmt(
        "{inputSame: %s, inputOffset: %zu, frameDone %s, done: %s}", cvtBoolToConstZ(this->inputSame), this->inputOffset,
        cvtBoolToConstZ(this->frameDone), cvtBoolToConstZ(this->done));
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
lz4DecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *decompressed)
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
                this->context, bufRemainsPtr(decompressed), &dstSize, bufPtr(compressed) + this->inputOffset, &srcSize, NULL)) == 0;

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

        // Create lz4 context
        lz4Error(LZ4F_createDecompressionContext(&driver->context, LZ4F_VERSION));

        // Set callback to ensure lz4 context is freed
        memContextCallbackSet(driver->memContext, lz4DecompressFreeResource, driver);

        // Create filter interface
        this = ioFilterNewP(
            LZ4_DECOMPRESS_FILTER_TYPE_STR, driver, NULL, .done = lz4DecompressDone, .inOut = lz4DecompressProcess,
            .inputSame = lz4DecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBLZ4
