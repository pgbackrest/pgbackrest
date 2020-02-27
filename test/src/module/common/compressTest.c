/***********************************************************************************************************************************
Test Compression
***********************************************************************************************************************************/
#include "common/io/filter/group.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
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
    memContextFree(((GzCompress *)ioFilterDriver(compress))->memContext);

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
    memContextFree(((GzDecompress *)ioFilterDriver(decompress))->memContext);

    return decompressed;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("gzError"))
    {
        TEST_RESULT_INT(gzError(Z_OK), Z_OK, "check ok");
        TEST_RESULT_INT(gzError(Z_STREAM_END), Z_STREAM_END, "check stream end");
        TEST_ERROR(gzError(Z_NEED_DICT), AssertError, "zlib threw error: [2] need dictionary");
        TEST_ERROR(gzError(Z_ERRNO), AssertError, "zlib threw error: [-1] file error");
        TEST_ERROR(gzError(Z_STREAM_ERROR), FormatError, "zlib threw error: [-2] stream error");
        TEST_ERROR(gzError(Z_DATA_ERROR), FormatError, "zlib threw error: [-3] data error");
        TEST_ERROR(gzError(Z_MEM_ERROR), MemoryError, "zlib threw error: [-4] insufficient memory");
        TEST_ERROR(gzError(Z_BUF_ERROR), AssertError, "zlib threw error: [-5] no space in buffer");
        TEST_ERROR(gzError(Z_VERSION_ERROR), FormatError, "zlib threw error: [-6] incompatible version");
        TEST_ERROR(gzError(999), AssertError, "zlib threw error: [999] unknown error");
    }

    // *****************************************************************************************************************************
    if (testBegin("GzCompress and GzDecompress"))
    {
        const char *simpleData = "A simple string";
        Buffer *compressed = NULL;
        Buffer *decompressed = bufNewC(simpleData, strlen(simpleData));

        VariantList *compressParamList = varLstNew();
        varLstAdd(compressParamList, varNewUInt(3));
        varLstAdd(compressParamList, varNewBool(false));

        TEST_ASSIGN(
            compressed, testCompress(gzCompressNewVar(compressParamList), decompressed, 1024, 1024),
            "simple data - compress large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzCompressNew(3), decompressed, 1024, 1)), true,
            "simple data - compress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzCompressNew(3), decompressed, 1, 1024)), true,
            "simple data - compress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzCompressNew(3), decompressed, 1, 1)), true,
            "simple data - compress small in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzDecompressNew(), compressed, 1024, 1024)), true,
            "simple data - decompress large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzDecompressNew(), compressed, 1024, 1)), true,
            "simple data - decompress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzDecompressNew(), compressed, 1, 1024)), true,
            "simple data - decompress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzDecompressNew(), compressed, 1, 1)), true,
            "simple data - decompress small in/small out buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no compression data");

        TEST_ERROR(testDecompress(gzDecompressNew(), bufNew(0), 1, 1), FormatError, "unexpected eof in compressed data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on truncated compression data");

        Buffer *truncated = bufNew(0);
        bufCatSub(truncated, compressed, 0, bufUsed(compressed) - 1);

        TEST_RESULT_UINT(bufUsed(truncated), bufUsed(compressed) - 1, "check truncated buffer size");
        TEST_ERROR(testDecompress(gzDecompressNew(), truncated, 512, 512), FormatError, "unexpected eof in compressed data");

        // Compress a large zero input buffer into small output buffer
        // -------------------------------------------------------------------------------------------------------------------------
        decompressed = bufNew(1024 * 1024 - 1);
        memset(bufPtr(decompressed), 0, bufSize(decompressed));
        bufUsedSet(decompressed, bufSize(decompressed));

        TEST_ASSIGN(
            compressed, testCompress(gzCompressNew(3), decompressed, bufSize(decompressed), 1024),
            "zero data - compress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzDecompressNew(), compressed, bufSize(compressed), 1024 * 256)), true,
            "zero data - decompress large in/small out buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("gzDecompressToLog() and gzCompressToLog()"))
    {
        GzDecompress *decompress = (GzDecompress *)ioFilterDriver(gzDecompressNew());

        TEST_RESULT_STR_Z(gzDecompressToLog(decompress), "{inputSame: false, done: false, availIn: 0}", "format object");

        decompress->inputSame = true;
        decompress->done = true;
        TEST_RESULT_STR_Z(gzDecompressToLog(decompress), "{inputSame: true, done: true, availIn: 0}", "format object");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
