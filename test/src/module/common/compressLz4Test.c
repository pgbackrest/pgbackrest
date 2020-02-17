/***********************************************************************************************************************************
Test LZ4
***********************************************************************************************************************************/
#include "common/io/filter/group.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
#ifdef HAVE_LIBLZ4

static Buffer *
testCompress(IoFilter *compress, Buffer *decompressed, size_t inputSize, size_t outputSize)
{
    Buffer *compressed = bufNew(1024 * 1024);
    size_t inputTotal = 0;
    ioBufferSizeSet(outputSize);

    IoWrite *write = ioBufferWriteNew(compressed);
    ioFilterGroupAdd(ioWriteFilterGroup(write), compress);
    ioWriteOpen(write);

    // Compress input data
    while (inputTotal < bufSize(decompressed))
    {
        // Generate the input buffer based on input size.  This breaks the data up into chunks as it would be in a real scenario.
        Buffer *input = bufNewC(
            bufPtr(decompressed) + inputTotal,
            inputSize > bufSize(decompressed) - inputTotal ? bufSize(decompressed) - inputTotal : inputSize);

        ioWrite(write, input);

        inputTotal += bufUsed(input);
        bufFree(input);
    }

    ioWriteClose(write);
    memContextFree(((Lz4Compress *)ioFilterDriver(compress))->memContext);

    return compressed;
}

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static Buffer *
testDecompress(IoFilter *decompress, Buffer *compressed, size_t inputSize, size_t outputSize)
{
    Buffer *decompressed = bufNew(1024 * 1024);
    Buffer *output = bufNew(outputSize);
    ioBufferSizeSet(inputSize);

    IoRead *read = ioBufferReadNew(compressed);
    ioFilterGroupAdd(ioReadFilterGroup(read), decompress);
    ioReadOpen(read);

    while (!ioReadEof(read))
    {
        ioRead(read, output);
        bufCat(decompressed, output);
        bufUsedZero(output);
    }

    ioReadClose(read);
    bufFree(output);
    memContextFree(((Lz4Decompress *)ioFilterDriver(decompress))->memContext);

    return decompressed;
}

#endif // HAVE_LIBLZ4

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

#ifdef HAVE_LIBLZ4

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

#endif // HAVE_LIBLZ4

    // *****************************************************************************************************************************
    if (testBegin("lz4Error"))
    {
#ifdef HAVE_LIBLZ4

        TEST_RESULT_UINT(lz4Error(0), 0, "check success");
        TEST_ERROR(lz4Error((size_t)-2), FormatError, "lz4 error: [-2] ERROR_maxBlockSize_invalid");

#endif // HAVE_LIBLZ4
    }

    // *****************************************************************************************************************************
    if (testBegin("Lz4Compress and Lz4Decompress"))
    {
#ifdef HAVE_LIBLZ4
        TEST_TITLE("compress simple data");

        const char *simpleData = "A simple string";
        Buffer *compressed = NULL;
        Buffer *decompressed = bufNewC(simpleData, strlen(simpleData));

        VariantList *compressParamList = varLstNew();
        varLstAdd(compressParamList, varNewUInt(3));

        TEST_ASSIGN(
            compressed, testCompress(lz4CompressNewVar(compressParamList), decompressed, 1024, 256 * 1024 * 1024),
            "large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(lz4CompressNew(1), decompressed, 1024, 1)), true,
            "large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(lz4CompressNew(1), decompressed, 1, 1024)), true,
            "compress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(lz4CompressNew(1), decompressed, 1, 1)), true,
            "compress small in/small out buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compressed output can be decompressed with lz4 tool");

        storagePutP(storageNewWriteP(storageTest, STRDEF("test.lz4")), compressed);
        TEST_SYSTEM("lz4 -dc {[path]}/test.lz4 > {[path]}/test.out");
        TEST_RESULT_BOOL(bufEq(decompressed, storageGetP(storageNewReadP(storageTest, STRDEF("test.out")))), true, "check output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decompress simple data");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(lz4DecompressNew(), compressed, 1024, 1024)), true,
            "simple data - decompress large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(lz4DecompressNew(), compressed, 1024, 1)), true,
            "simple data - decompress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(lz4DecompressNew(), compressed, 1, 1024)), true,
            "simple data - decompress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(lz4DecompressNew(), compressed, 1, 1)), true,
            "simple data - decompress small in/small out buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no compression data");

        TEST_ERROR(testDecompress(lz4DecompressNew(), bufNew(0), 1, 1), FormatError, "unexpected eof in compressed data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on truncated compression data");

        Buffer *truncated = bufNew(0);
        bufCatSub(truncated, compressed, 0, bufUsed(compressed) - 1);

        TEST_RESULT_UINT(bufUsed(truncated), bufUsed(compressed) - 1, "check truncated buffer size");
        TEST_ERROR(testDecompress(lz4DecompressNew(), truncated, 512, 512), FormatError, "unexpected eof in compressed data");

        // Compress a large zero input buffer into small output buffer
        // -------------------------------------------------------------------------------------------------------------------------
        decompressed = bufNew(1024 * 1024 - 1);
        memset(bufPtr(decompressed), 0, bufSize(decompressed));
        bufUsedSet(decompressed, bufSize(decompressed));

        TEST_ASSIGN(
            compressed, testCompress(lz4CompressNew(1), decompressed, bufSize(decompressed), 1024),
            "zero data - compress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(lz4DecompressNew(), compressed, bufSize(compressed), 1024 * 256)), true,
            "zero data - decompress large in/small out buffer");

        // !!! THIS IS NOT TRUE IN ALL CASES
        // TEST_RESULT_BOOL(
        //     bufEq(compressed, testCompress(lz4CompressNew(9), decompressed, 1024, 1024)), false,
        //     "compress using different level -- should not be equal in size");

#endif // HAVE_LIBLZ4
    }

    // *****************************************************************************************************************************
    if (testBegin("lz4DecompressToLog() and lz4CompressToLog()"))
    {
#ifdef HAVE_LIBLZ4
        Lz4Compress *compress = (Lz4Compress *)ioFilterDriver(lz4CompressNew(7));

        compress->inputSame = true;
        compress->flushing = true;

        TEST_RESULT_STR_Z(lz4CompressToLog(compress), "{level: 7, first: true, inputSame: true, flushing: true}", "format object");

        // -------------------------------------------------------------------------------------------------------------------------
        Lz4Decompress *decompress = (Lz4Decompress *)ioFilterDriver(lz4DecompressNew());

        decompress->inputSame = true;
        decompress->done = true;
        decompress->inputOffset = 999;

        TEST_RESULT_STR_Z(
            lz4DecompressToLog(decompress), "{inputSame: true, inputOffset: 999, frameDone false, done: true}", "format object");

#endif // HAVE_LIBLZ4
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
