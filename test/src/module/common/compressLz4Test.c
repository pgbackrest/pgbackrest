/***********************************************************************************************************************************
Test LZ4
***********************************************************************************************************************************/
#include "common/io/filter/group.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
// static Buffer *
// testCompress(Lz4Compress *compress, Buffer *decompressed, size_t inputSize, size_t outputSize)
// {
//     Buffer *compressed = bufNew(1024 * 1024);
//     size_t inputTotal = 0;
//     ioBufferSizeSet(outputSize);
//
//     IoFilterGroup *filterGroup = ioFilterGroupNew();
//     ioFilterGroupAdd(filterGroup, lz4CompressFilter(compress));
//     IoWrite *write = ioBufferWriteIo(ioBufferWriteNew(compressed));
//     ioWriteFilterGroupSet(write, filterGroup);
//     ioWriteOpen(write);
//
//     // Compress input data
//     while (inputTotal < bufSize(decompressed))
//     {
//         // Generate the input buffer based on input size.  This breaks the data up into chunks as it would be in a real scenario.
//         Buffer *input = bufNewC(
//             inputSize > bufSize(decompressed) - inputTotal ? bufSize(decompressed) - inputTotal : inputSize,
//             bufPtr(decompressed) + inputTotal);
//
//         ioWrite(write, input);
//
//         inputTotal += bufUsed(input);
//         bufFree(input);
//     }
//
//     ioWriteClose(write);
//     lz4CompressFree(compress);
//
//     return compressed;
// }

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
// static Buffer *
// testDecompress(Lz4Decompress *decompress, Buffer *compressed, size_t inputSize, size_t outputSize)
// {
//     Buffer *decompressed = bufNew(1024 * 1024);
//     Buffer *output = bufNew(outputSize);
//     ioBufferSizeSet(inputSize);
//
//     IoFilterGroup *filterGroup = ioFilterGroupNew();
//     ioFilterGroupAdd(filterGroup, lz4DecompressFilter(decompress));
//     IoRead *read = ioBufferReadIo(ioBufferReadNew(compressed));
//     ioReadFilterGroupSet(read, filterGroup);
//     ioReadOpen(read);
//
//     while (!ioReadEof(read))
//     {
//         ioRead(read, output);
//         bufCat(decompressed, output);
//         bufUsedZero(output);
//     }
//
//     ioReadClose(read);
//     bufFree(output);
//     lz4DecompressFree(decompress);
//
//     return decompressed;
// }

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

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

        // const char *simpleData = "A simple string";
        // Buffer *compressed = NULL;
        // Buffer *decompressed = bufNewC(strlen(simpleData), simpleData);
        //
        // TEST_ASSIGN(
        //     compressed, testCompress(lz4CompressNew(0), decompressed, 1024, 1024),
        //     "simple data - compress large in/large out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(compressed, testCompress(lz4CompressNew(0), decompressed, 1024, 1)), true,
        //     "simple data - compress large in/small out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(compressed, testCompress(lz4CompressNew(0), decompressed, 1, 1024)), true,
        //     "simple data - compress small in/large out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(compressed, testCompress(lz4CompressNew(0), decompressed, 1, 1)), true,
        //     "simple data - compress small in/small out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(decompressed, testDecompress(lz4DecompressNew(false), compressed, 1024, 1024)), true,
        //     "simple data - decompress large in/large out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(decompressed, testDecompress(lz4DecompressNew(false), compressed, 1024, 1)), true,
        //     "simple data - decompress large in/small out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(decompressed, testDecompress(lz4DecompressNew(false), compressed, 1, 1024)), true,
        //     "simple data - decompress small in/large out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(decompressed, testDecompress(lz4DecompressNew(false), compressed, 1, 1)), true,
        //     "simple data - decompress small in/small out buffer");
        //
        // // Compress a large zero input buffer into small output buffer
        // // -------------------------------------------------------------------------------------------------------------------------
        // decompressed = bufNew(1024 * 1024 - 1);
        // memset(bufPtr(decompressed), 0, bufSize(decompressed));
        // bufUsedSet(decompressed, bufSize(decompressed));
        //
        // TEST_ASSIGN(
        //     compressed, testCompress(lz4CompressNew(0), decompressed, bufSize(decompressed), 1024),
        //     "zero data - compress large in/small out buffer");
        //
        // TEST_RESULT_BOOL(
        //     bufEq(decompressed, testDecompress(lz4DecompressNew(true), compressed, bufSize(compressed), 1024 * 256)), true,
        //     "zero data - decompress large in/small out buffer");
        //
        // // -------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(lz4CompressFree(NULL), "free null decompress object");
        // TEST_RESULT_VOID(lz4DecompressFree(NULL), "free null decompress object");

#endif // HAVE_LIBLZ4
    }

    // *****************************************************************************************************************************
    if (testBegin("lz4DecompressToLog() and lz4CompressToLog()"))
    {
#ifdef HAVE_LIBLZ4

        // Lz4Decompress *decompress = lz4DecompressNew(false);
        //
        // TEST_RESULT_STR(strPtr(lz4DecompressToLog(decompress)), "{inputSame: false, done: false, availIn: 0}", "format object");
        //
        // decompress->inputSame = true;
        // decompress->done = true;
        // TEST_RESULT_STR(strPtr(lz4DecompressToLog(decompress)), "{inputSame: true, done: true, availIn: 0}", "format object");

#endif // HAVE_LIBLZ4
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
