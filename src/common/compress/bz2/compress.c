/***********************************************************************************************************************************
BZ2 Compress
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <bzlib.h>

#include "common/compress/bz2/common.h"
#include "common/compress/bz2/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(BZ2_COMPRESS_FILTER_TYPE_STR,                         BZ2_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define BZ2_COMPRESS_TYPE                                           Bz2Compress
#define BZ2_COMPRESS_PREFIX                                         bz2Compress

typedef struct Bz2Compress
{
    MemContext *memContext;                                         // Context to store data
    bz_stream stream;                                               // Compression stream

    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
	bool done;
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
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(BZ2_COMPRESS, LOG, logLevelTrace)
{
    BZ2_bzCompressEnd(&this->stream);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

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
	bz2Error(BZ2_bzCompress(&this->stream, this->flushing ? BZ_FINISH : BZ_RUN));

	// Set buffer used space
	bufUsedSet(compressed, bufSize(compressed) - (size_t)this->stream.avail_out);

	// Is compression done?
	if (this->flushing && this->stream.avail_out > 0)
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

    ASSERT(level >= 0);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Bz2Compress")
    {
        Bz2Compress *driver = memNew(sizeof(Bz2Compress));

        *driver = (Bz2Compress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .stream = {.bzalloc = NULL},
        };

        // Initialize context
        bz2Error(BZ2_bzCompressInit(&driver->stream, level, 0, 0));

        // Set callback to ensure bz2 stream is freed
        memContextCallbackSet(driver->memContext, bz2CompressFreeResource, driver);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewInt(level));

        // Create filter interface
        this = ioFilterNewP(
            BZ2_COMPRESS_FILTER_TYPE_STR, driver, paramList, .done = bz2CompressDone, .inOut = bz2CompressProcess,
            .inputSame = bz2CompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
