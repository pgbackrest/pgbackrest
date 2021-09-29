/***********************************************************************************************************************************
BZ2 Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <bzlib.h>

#include "common/compress/bz2/common.h"
#include "common/compress/bz2/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Bz2Decompress
{
    bz_stream stream;                                               // Decompression stream state

    int result;                                                     // Result of last operation
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool done;                                                      // Is decompression done?
} Bz2Decompress;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
bz2DecompressToLog(const Bz2Decompress *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s, avail_in: %u}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done),
        this->stream.avail_in);
}

#define FUNCTION_LOG_BZ2_DECOMPRESS_TYPE                                                                                            \
    Bz2Decompress *
#define FUNCTION_LOG_BZ2_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, bz2DecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free inflate stream
***********************************************************************************************************************************/
static void
bz2DecompressFreeResource(THIS_VOID)
{
    THIS(Bz2Decompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BZ2_DECOMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    BZ2_bzDecompressEnd(&this->stream);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
bz2DecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *uncompressed)
{
    THIS(Bz2Decompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BZ2_DECOMPRESS, this);
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

        // bzip2 does not accept const input buffers
        this->stream.next_in = (char *)UNCONSTIFY(unsigned char *, bufPtrConst(compressed));
    }

    this->stream.avail_out = (unsigned int)bufRemains(uncompressed);
    this->stream.next_out = (char *)bufPtr(uncompressed) + bufUsed(uncompressed);

    this->result = bz2Error(BZ2_bzDecompress(&this->stream));

    // Set buffer used space
    bufUsedSet(uncompressed, bufSize(uncompressed) - (size_t)this->stream.avail_out);

    // Is decompression done?
    this->done = this->result == BZ_STREAM_END;

    // Is the same input expected on the next call?
    this->inputSame = this->done ? false : this->stream.avail_in != 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
static bool
bz2DecompressDone(const THIS_VOID)
{
    THIS(const Bz2Decompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BZ2_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
bz2DecompressInputSame(const THIS_VOID)
{
    THIS(const Bz2Decompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BZ2_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
bz2DecompressNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(Bz2Decompress)
    {
        // Allocate state and set context
        Bz2Decompress *driver = OBJ_NEW_ALLOC();

        *driver = (Bz2Decompress)
        {
            .stream = {.bzalloc = NULL},
        };

        // Create bz2 stream
        bz2Error(driver->result = BZ2_bzDecompressInit(&driver->stream, 0, 0));

        // Set free callback to ensure bz2 context is freed
        memContextCallbackSet(objMemContext(driver), bz2DecompressFreeResource, driver);

        // Create filter interface
        this = ioFilterNewP(
            BZ2_DECOMPRESS_FILTER_TYPE, driver, NULL, .done = bz2DecompressDone, .inOut = bz2DecompressProcess,
            .inputSame = bz2DecompressInputSame);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
