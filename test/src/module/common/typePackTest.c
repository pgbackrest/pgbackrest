/***********************************************************************************************************************************
Test Packs
***********************************************************************************************************************************/
#include "common/io/bufferWrite.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("PackWrite and PackRead"))
    {
        TEST_TITLE("write pack");

        Buffer *pack = bufNew(0);
        PackWrite *packWrite = NULL;

        IoWrite *write = ioBufferWriteNew(pack);
        ioWriteOpen(write);

        TEST_ASSIGN(packWrite, pckWriteNew(write), "new write");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{idLast: 0}", "log");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 1, 0750), "write mode");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{idLast: 1}", "log");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 2, 1911246845), "write timestamp");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 4, 0xFFFFFFFFFFFFFFFF), "write max int");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 9, 1), "write 1");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "03e803"                                                // 1, u64, 750
            "09fd9fad8f07"                                          // 2, u64, 1911246845
            "33ffffffffffffffffff01"                                // 4, u64, 0xFFFFFFFFFFFFFFFF
            "810101"                                                // 9, u64, 1
            "00",                                                   // end
            "check pack");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read pack");

        TEST_RESULT_UINT(pckToUInt64(bufPtr(pack)), 0 << 5 | 1 << 1 | pckTypeUInt64, "pack to tag");
        // TEST_RESULT_UINT(pckToUInt64(bufPtr(pack) + 1), 0750, "pack to 0750");
        // TEST_RESULT_UINT(pckToUInt64(bufPtr(pack) + 3), 0, "pack to end");
    }

    // *****************************************************************************************************************************
    if (testBegin("pckToLog()"))
    {
        // TEST_TITLE("log for new pack write");
        //
        // PackWrite *packWrite = pckWriteNew(ioBufferWriteNew(bufNew()));
        // TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{size: 0, extra: 64}", "log");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
