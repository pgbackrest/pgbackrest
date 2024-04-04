/***********************************************************************************************************************************
Test Buffers
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
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

        TEST_RESULT_PTR(bufPtr(buffer), buffer->pub.buffer, "buffer pointer");
        TEST_RESULT_UINT(bufSize(buffer), 256, "buffer size");
        TEST_RESULT_UINT(bufSizeAlloc(buffer), 256, "buffer allocation size");

        TEST_ASSIGN(buffer, bufNewC("TEST-STR", sizeof("TEST-STR") - 1), "new buffer from string");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "TEST-STR", 8) == 0, true, "check buffer");

        TEST_RESULT_UINT(bufSize(bufDup(bufNew(0))), 0, "duplicate empty buffer");

        TEST_RESULT_VOID(bufFree(buffer), "free buffer");
        TEST_RESULT_VOID(bufFree(bufNew(0)), "free empty buffer");

        TEST_RESULT_VOID(bufMove(NULL, memContextTop()), "move null buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        char cBuffer[] = "ABCD";

        TEST_ASSIGN(buffer, bufNewC(cBuffer, sizeof(cBuffer)), "create from c buffer");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), cBuffer, sizeof(cBuffer)) == 0, true, "check buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bufNewDecode()");

        TEST_RESULT_STR_Z(strNewBuf(bufNewDecode(encodingBase64, STRDEF("eno="))), "zz", "decode base64");
        TEST_RESULT_STR_Z(strNewBuf(bufNewDecode(encodingBase64, STRDEF(""))), "", "decode empty base64");
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

        TEST_ERROR(bufLimitSet(buffer, 64), AssertError, "assertion 'limit >= bufUsed(this)' failed");
        TEST_RESULT_VOID(bufUsedSet(buffer, 64), "set used");

        // Use limits to change size reporting
        TEST_RESULT_VOID(bufLimitSet(buffer, 64), "set limit");
        TEST_RESULT_UINT(bufSize(buffer), 64, "check limited size");
        TEST_RESULT_BOOL(buffer->pub.sizeLimit, true, "check limit flag");

        TEST_RESULT_VOID(bufLimitClear(buffer), "clear limit");
        TEST_RESULT_UINT(bufSize(buffer), 128, "check unlimited size");
        TEST_RESULT_BOOL(buffer->pub.sizeLimit, false, "check limit flag");

        TEST_RESULT_VOID(bufLimitSet(buffer, 96), "set limit");
        TEST_RESULT_UINT(bufSize(buffer), 96, "check limited size");
        TEST_RESULT_BOOL(buffer->pub.sizeLimit, true, "check limit flag");

        TEST_RESULT_VOID(bufLimitSet(buffer, 128), "set limit");
        TEST_RESULT_UINT(bufSize(buffer), 128, "check unlimited size");
        TEST_RESULT_BOOL(buffer->pub.sizeLimit, false, "check limit flag");

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
    if (testBegin("bufDup(), bufEq(), and bufFind()"))
    {
        TEST_RESULT_BOOL(bufEq(BUFSTRDEF("123"), bufDup(BUFSTRDEF("1234"))), false, "buffer sizes not equal");
        TEST_RESULT_BOOL(bufEq(BUFSTR(STRDEF("321")), BUFSTRDEF("123")), false, "buffer sizes equal");
        TEST_RESULT_BOOL(bufEq(bufDup(BUFSTRZ("123")), BUF("123", 3)), true, "buffers equal");

        const Buffer *haystack = BUFSTRDEF("findsomethinginhere");

        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("xxx")), NULL, "not found");
        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("find")), bufPtrConst(haystack), "found first");
        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("here")), bufPtrConst(haystack) + 15, "found last");
        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("thing")), bufPtrConst(haystack) + 8, "found middle");
        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("find"), .begin = bufPtrConst(haystack) + 1), NULL, "skipped not found");
        TEST_RESULT_PTR(bufFindP(haystack, BUFSTRDEF("findsomethinginhere2")), NULL, "needle longer than haystack");
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
        char logBuf[STACK_TRACE_PARAM_MAX];

        Buffer *buffer = bufNew(100);
        TEST_RESULT_VOID(FUNCTION_LOG_OBJECT_FORMAT(buffer, bufToLog, logBuf, sizeof(logBuf)), "bufToLog");
        TEST_RESULT_Z(logBuf, "{used: 0, size: 100}", "check log");

        bufLimitSet(buffer, 50);
        TEST_RESULT_VOID(FUNCTION_LOG_OBJECT_FORMAT(buffer, bufToLog, logBuf, sizeof(logBuf)), "bufToLog");
        TEST_RESULT_Z(logBuf, "{used: 0, size: 50, sizeAlloc: 100}", "check log");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
