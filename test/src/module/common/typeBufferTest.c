/***********************************************************************************************************************************
Test Buffers
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("bufNew(), bufNewStr(), bufSize(), bufPtr(), and bufFree()"))
    {
        Buffer *buffer = NULL;

        TEST_ASSIGN(buffer, bufNew(256), "new buffer");
        TEST_RESULT_PTR(bufPtr(buffer), buffer->buffer, "buffer pointer");
        TEST_RESULT_INT(bufSize(buffer), 256, "buffer size");

        TEST_ASSIGN(buffer, bufNewStr(strNew("TEST-STR")), "new buffer from string");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TEST-STR", 8) == 0, true, "check buffer");

        TEST_RESULT_VOID(bufFree(buffer), "free buffer");
        TEST_RESULT_VOID(bufFree(bufNew(0)), "free empty buffer");
        TEST_RESULT_VOID(bufFree(NULL), "free null buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufResize()"))
    {
        Buffer *buffer = NULL;
        unsigned char *bufferPtr = NULL;

        TEST_ASSIGN(buffer, bufNew(0), "new zero buffer");
        TEST_RESULT_INT(bufSize(buffer), 0, "check size");
        TEST_RESULT_PTR(bufResize(buffer, 256), buffer, "resize buffer");
        TEST_RESULT_INT(bufSize(buffer), 256, "check size");

        // Load data
        TEST_ASSIGN(bufferPtr, bufPtr(buffer), "buffer pointer");

        for (unsigned int bufferIdx = 0; bufferIdx < bufSize(buffer); bufferIdx++)
            bufferPtr[bufferIdx] = (unsigned char)bufferIdx;

        // Increase buffer size
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 512)), "increase buffer size");
        TEST_RESULT_INT(bufSize(buffer), 512, "check size");

        // Test that no bytes have changed in the original data
        unsigned int sameTotal = 0;

        for (unsigned int bufferIdx = 0; bufferIdx < 256; bufferIdx++)
            sameTotal += bufferPtr[bufferIdx] == bufferIdx;

        TEST_RESULT_INT(sameTotal, 256, "original bytes match");

        // Decrease buffer size
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 128)), "decrease buffer size");
        TEST_RESULT_INT(bufSize(buffer), 128, "check size");

        // Test that no bytes have changed in the original data
        sameTotal = 0;

        for (unsigned int bufferIdx = 0; bufferIdx < bufSize(buffer); bufferIdx++)
            sameTotal += bufferPtr[bufferIdx] == bufferIdx;

        TEST_RESULT_INT(sameTotal, 128, "original bytes match");

        // Resize to zero buffer
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 0)), "decrease to zero");
        TEST_RESULT_INT(bufSize(buffer), 0, "check size");
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 0)), "decrease to zero again");
        TEST_RESULT_INT(bufSize(buffer), 0, "check size");
    }
}
