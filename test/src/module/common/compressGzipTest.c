/***********************************************************************************************************************************
Test Gzip
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
    memContextFree(((GzipCompress *)ioFilterDriver(compress))->memContext);

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
    memContextFree(((GzipDecompress *)ioFilterDriver(decompress))->memContext);

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
    if (testBegin("gzipError"))
    {
        TEST_RESULT_INT(gzipError(Z_OK), Z_OK, "check ok");
        TEST_RESULT_INT(gzipError(Z_STREAM_END), Z_STREAM_END, "check stream end");
        TEST_ERROR(gzipError(Z_NEED_DICT), AssertError, "zlib threw error: [2] need dictionary");
        TEST_ERROR(gzipError(Z_ERRNO), AssertError, "zlib threw error: [-1] file error");
        TEST_ERROR(gzipError(Z_STREAM_ERROR), FormatError, "zlib threw error: [-2] stream error");
        TEST_ERROR(gzipError(Z_DATA_ERROR), FormatError, "zlib threw error: [-3] data error");
        TEST_ERROR(gzipError(Z_MEM_ERROR), MemoryError, "zlib threw error: [-4] insufficient memory");
        TEST_ERROR(gzipError(Z_BUF_ERROR), AssertError, "zlib threw error: [-5] no space in buffer");
        TEST_ERROR(gzipError(Z_VERSION_ERROR), FormatError, "zlib threw error: [-6] incompatible version");
        TEST_ERROR(gzipError(999), AssertError, "zlib threw error: [999] unknown error");
    }

    // *****************************************************************************************************************************
    if (testBegin("gzipWindowBits"))
    {

        TEST_RESULT_INT(gzipWindowBits(true), -15, "raw window bits");
        TEST_RESULT_INT(gzipWindowBits(false), 31, "gzip window bits");
    }

    // *****************************************************************************************************************************
    if (testBegin("GzipCompress and GzipDecompress"))
    {
        const char *simpleData = "A simple string";
        Buffer *compressed = NULL;
        Buffer *decompressed = bufNewC(simpleData, strlen(simpleData));

        VariantList *compressParamList = varLstNew();
        varLstAdd(compressParamList, varNewUInt(3));
        varLstAdd(compressParamList, varNewBool(false));

        VariantList *decompressParamList = varLstNew();
        varLstAdd(decompressParamList, varNewBool(false));

        TEST_ASSIGN(
            compressed, testCompress(gzipCompressNewVar(compressParamList), decompressed, 1024, 1024),
            "simple data - compress large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzipCompressNew(3, false), decompressed, 1024, 1)), true,
            "simple data - compress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzipCompressNew(3, false), decompressed, 1, 1024)), true,
            "simple data - compress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(compressed, testCompress(gzipCompressNew(3, false), decompressed, 1, 1)), true,
            "simple data - compress small in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzipDecompressNewVar(decompressParamList), compressed, 1024, 1024)), true,
            "simple data - decompress large in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzipDecompressNew(false), compressed, 1024, 1)), true,
            "simple data - decompress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzipDecompressNew(false), compressed, 1, 1024)), true,
            "simple data - decompress small in/large out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzipDecompressNew(false), compressed, 1, 1)), true,
            "simple data - decompress small in/small out buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no compression data");

        TEST_ERROR(testDecompress(gzipDecompressNew(true), bufNew(0), 1, 1), FormatError, "unexpected eof in compressed data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on truncated compression data");

        Buffer *truncated = bufNew(0);
        bufCatSub(truncated, compressed, 0, bufUsed(compressed) - 1);

        TEST_RESULT_UINT(bufUsed(truncated), bufUsed(compressed) - 1, "check truncated buffer size");
        TEST_ERROR(testDecompress(gzipDecompressNew(false), truncated, 512, 512), FormatError, "unexpected eof in compressed data");

        // Compress a large zero input buffer into small output buffer
        // -------------------------------------------------------------------------------------------------------------------------
        decompressed = bufNew(1024 * 1024 - 1);
        memset(bufPtr(decompressed), 0, bufSize(decompressed));
        bufUsedSet(decompressed, bufSize(decompressed));

        TEST_ASSIGN(
            compressed, testCompress(gzipCompressNew(3, true), decompressed, bufSize(decompressed), 1024),
            "zero data - compress large in/small out buffer");

        TEST_RESULT_BOOL(
            bufEq(decompressed, testDecompress(gzipDecompressNew(true), compressed, bufSize(compressed), 1024 * 256)), true,
            "zero data - decompress large in/small out buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("gzipDecompressToLog() and gzipCompressToLog()"))
    {
        GzipDecompress *decompress = (GzipDecompress *)ioFilterDriver(gzipDecompressNew(false));

        TEST_RESULT_STR_Z(gzipDecompressToLog(decompress), "{inputSame: false, done: false, availIn: 0}", "format object");

        decompress->inputSame = true;
        decompress->done = true;
        TEST_RESULT_STR_Z(gzipDecompressToLog(decompress), "{inputSame: true, done: true, availIn: 0}", "format object");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
