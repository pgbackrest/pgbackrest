/***********************************************************************************************************************************
Test Buffers
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("bufNew*(), bufMove(), bufSize(), bufSizeAlloc(), bufPtr(), and bufFree()"))
    {
        Buffer *buffer = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(buffer, bufNew(256), "new buffer");
            bufMove(buffer, memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(bufPtr(buffer), buffer->buffer, "buffer pointer");
        TEST_RESULT_UINT(bufSize(buffer), 256, "buffer size");
        TEST_RESULT_UINT(bufSizeAlloc(buffer), 256, "buffer allocation size");

        TEST_ASSIGN(buffer, bufNewC("TEST-STR", sizeof("TEST-STR") - 1), "new buffer from string");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TEST-STR", 8) == 0, true, "check buffer");

        TEST_RESULT_VOID(bufFree(buffer), "free buffer");
        TEST_RESULT_VOID(bufFree(bufNew(0)), "free empty buffer");

        TEST_RESULT_VOID(bufMove(NULL, memContextTop()), "move null buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        char cBuffer[] = "ABCD";

        TEST_ASSIGN(buffer, bufNewC(cBuffer, sizeof(cBuffer)), "create from c buffer");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), cBuffer, sizeof(cBuffer)) == 0, true, "check buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bufNewDecode()");

        TEST_RESULT_STR_Z(strNewBuf(bufNewDecode(encodeBase64, STRDEF("eno="))), "zz", "decode base64");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufResize(), bufFull(), bufLimit*(), bufRemains*(), and bufUsed*()"))
    {
        Buffer *buffer = NULL;
        unsigned char *bufferPtr = NULL;

        TEST_ASSIGN(buffer, bufNew(0), "new zero buffer");
        TEST_RESULT_UINT(bufSize(buffer), 0, "check size");
        TEST_RESULT_PTR(bufResize(buffer, 256), buffer, "resize buffer");
        TEST_RESULT_UINT(bufSize(buffer), 256, "check size");
        TEST_RESULT_VOID(bufUsedSet(buffer, 256), "set used size");
        TEST_RESULT_BOOL(bufFull(buffer), true, "check buffer full");

        // Load data
        TEST_ASSIGN(bufferPtr, bufPtr(buffer), "buffer pointer");

        for (unsigned int bufferIdx = 0; bufferIdx < bufSize(buffer); bufferIdx++)
            bufferPtr[bufferIdx] = (unsigned char)bufferIdx;

        // Increase buffer size
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 512)), "increase buffer size");
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 512)), "set to same size");
        TEST_RESULT_UINT(bufSize(buffer), 512, "check size");
        TEST_RESULT_UINT(bufUsed(buffer), 256, "check used size");
        TEST_RESULT_BOOL(bufFull(buffer), false, "check buffer not full");
        TEST_RESULT_UINT(bufRemains(buffer), 256, "check remaining buffer space");
        TEST_RESULT_PTR(bufRemainsPtr(buffer), bufPtr(buffer) + 256, "check remaining buffer space pointer");
        TEST_RESULT_VOID(bufUsedInc(buffer, 256), "set used size");
        TEST_RESULT_BOOL(bufFull(buffer), true, "check buffer full");

        // Test that no bytes have changed in the original data
        unsigned int sameTotal = 0;

        for (unsigned int bufferIdx = 0; bufferIdx < 256; bufferIdx++)
            sameTotal += bufferPtr[bufferIdx] == bufferIdx;

        TEST_RESULT_INT(sameTotal, 256, "original bytes match");

        // Decrease buffer size
        TEST_ASSIGN(bufferPtr, bufPtr(bufResize(buffer, 128)), "decrease buffer size");
        TEST_RESULT_UINT(bufSize(buffer), 128, "check size");

        // Test that no bytes have changed in the original data
        sameTotal = 0;

        for (unsigned int bufferIdx = 0; bufferIdx < bufSize(buffer); bufferIdx++)
            sameTotal += bufferPtr[bufferIdx] == bufferIdx;

        TEST_RESULT_INT(sameTotal, 128, "original bytes match");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when used > new limit");

        TEST_ERROR(bufLimitSet(buffer, 64), AssertError, "assertion 'limit >= this->used' failed");
        TEST_RESULT_VOID(bufUsedSet(buffer, 64), "set used");

        // Use limits to change size reporting
        TEST_RESULT_VOID(bufLimitSet(buffer, 64), "set limit");
        TEST_RESULT_UINT(bufSize(buffer), 64, "    check limited size");
        TEST_RESULT_VOID(bufLimitClear(buffer), "    clear limit");
        TEST_RESULT_UINT(bufSize(buffer), 128, "    check unlimited size");

        // Resize to zero buffer
        TEST_RESULT_VOID(bufUsedZero(buffer), "set used to 0");
        TEST_RESULT_UINT(bufUsed(buffer), 0, "check used is zero");

        TEST_RESULT_VOID(bufLimitSet(buffer, 64), "set limit to make sure it gets reduced with the resize");
        TEST_RESULT_VOID(bufResize(buffer, 32), "decrease size to 32");
        TEST_RESULT_UINT(bufSize(buffer), 32, "check size");
        TEST_RESULT_VOID(bufLimitSet(buffer, 0), "set limit so that it won't need to be resized");
        TEST_RESULT_VOID(bufResize(buffer, 0), "decrease size to zero");
        TEST_RESULT_UINT(bufSize(buffer), 0, "check size");
        TEST_RESULT_VOID(bufResize(buffer, 0), "decrease size to zero again");
        TEST_RESULT_UINT(bufSize(buffer), 0, "check size");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufDup() and bufEq()"))
    {
        TEST_RESULT_BOOL(bufEq(BUFSTRDEF("123"), bufDup(BUFSTRDEF("1234"))), false, "buffer sizes not equal");
        TEST_RESULT_BOOL(bufEq(BUFSTR(STRDEF("321")), BUFSTRDEF("123")), false, "buffer sizes equal");
        TEST_RESULT_BOOL(bufEq(bufDup(BUFSTRZ("123")), BUF("123", 3)), true, "buffers equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufHex()"))
    {
        TEST_RESULT_STR_Z(bufHex(BUFSTRDEF("ABC-CBA")), "4142432d434241", "buffer to hex");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufCat*()"))
    {
        TEST_RESULT_STR_Z(strNewBuf(bufCat(bufNewC("123", 3), NULL)), "123", "cat null buffer");
        TEST_RESULT_STR_Z(strNewBuf(bufCat(bufNewC("123", 3), bufNew(0))), "123", "cat empty buffer");
        TEST_RESULT_STR_Z(strNewBuf(bufCat(bufNewC("123", 3), BUFSTRDEF("ABC"))), "123ABC", "cat buffer");

        TEST_RESULT_STR_Z(strNewBuf(bufCatSub(bufNewC("123", 3), NULL, 0, 0)), "123", "cat sub null buffer");
        TEST_RESULT_STR_Z(strNewBuf(bufCatSub(bufNewC("123", 3), bufNew(0), 0, 0)), "123", "cat sub empty buffer");
        TEST_RESULT_STR_Z(strNewBuf(bufCatSub(bufNewC("123", 3), BUFSTRDEF("ABC"), 1, 2)), "123BC", "cat sub buffer");

        Buffer *buffer = NULL;
        TEST_ASSIGN(buffer, bufNew(2), "new buffer with space");
        TEST_RESULT_STR_Z(strNewBuf(bufCat(buffer, BUFSTRDEF("AB"))), "AB", "cat buffer with space");
    }

    // *****************************************************************************************************************************
    if (testBegin("bufToLog()"))
    {
        Buffer *buffer = bufNew(100);
        TEST_RESULT_STR_Z(bufToLog(buffer), "{used: 0, size: 100}", "buf to log");

        bufLimitSet(buffer, 50);
        TEST_RESULT_STR_Z(bufToLog(buffer), "{used: 0, size: 50, sizeAlloc: 100}", "buf to log");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
