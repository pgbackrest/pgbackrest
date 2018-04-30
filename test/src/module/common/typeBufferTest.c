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
    if (testBegin("bufNew(), bugNewC, bufNewStr(), bufMove(), bufSize(), bufPtr(), and bufFree()"))
    {
        Buffer *buffer = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(buffer, bufNew(256), "new buffer");
            bufMove(buffer, MEM_CONTEXT_OLD());
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(bufPtr(buffer), buffer->buffer, "buffer pointer");
        TEST_RESULT_INT(bufSize(buffer), 256, "buffer size");

        TEST_ASSIGN(buffer, bufNewStr(strNew("TEST-STR")), "new buffer from string");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TEST-STR", 8) == 0, true, "check buffer");

        TEST_RESULT_VOID(bufFree(buffer), "free buffer");
        TEST_RESULT_VOID(bufFree(bufNew(0)), "free empty buffer");
        TEST_RESULT_VOID(bufFree(NULL), "free null buffer");

        TEST_RESULT_VOID(bufMove(NULL, NULL), "move null buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        char cBuffer[] = "ABCD";

        TEST_ASSIGN(buffer, bufNewC(sizeof(cBuffer), cBuffer), "create from c buffer");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), cBuffer, sizeof(cBuffer)) == 0, true, "check buffer");
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
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 512)), "set to same size");
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

    // *****************************************************************************************************************************
    if (testBegin("bufEq()"))
    {
        TEST_RESULT_BOOL(bufEq(bufNewStr(strNew("123")), bufNewStr(strNew("1234"))), false, "buffer sizes not equal");
        TEST_RESULT_BOOL(bufEq(bufNewStr(strNew("321")), bufNewStr(strNew("123"))), false, "buffer sizes equal");
        TEST_RESULT_BOOL(bufEq(bufNewStr(strNew("123")), bufNewStr(strNew("123"))), true, "buffers equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufCat()"))
    {
        TEST_RESULT_STR(strPtr(strNewBuf(bufCat(bufNewStr(strNew("123")), NULL))), "123", "cat null buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(bufCat(bufNewStr(strNew("123")), bufNew(0)))), "123", "cat empty buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(bufCat(bufNewStr(strNew("123")), bufNewStr(strNew("ABC"))))), "123ABC", "cat buffer");
    }
}
