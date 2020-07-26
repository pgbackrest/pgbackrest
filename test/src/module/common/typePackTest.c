/***********************************************************************************************************************************
Test Packs
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
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
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 4, 0xFFFFFFFFFFFFFFFF), "write max u64");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 9, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 10, 77), "write 77");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 11, 127), "write 127");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "03e803"                                                //  1, u64, 750
            "09fd9fad8f07"                                          //  2, u64, 1911246845
            "33ffffffffffffffffff01"                                //  4, u64, 0xFFFFFFFFFFFFFFFF
            "810101"                                                //  9, u64, 1
            "014d"                                                  // 10, u64, 77
            "017f"                                                  // 11, u64, 127
            "00",                                                   // end
            "check pack");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read pack");

        IoRead *read = ioBufferReadNew(pack);
        ioReadOpen(read);

        PackRead *packRead = NULL;
        TEST_ASSIGN(packRead, pckReadNew(read), "new read");

        TEST_RESULT_UINT(pckReadUInt64(packRead, 1), 0750, "read mode");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 2), 1911246845, "read timestamp");
        TEST_ERROR(pckReadUInt64(packRead, 2), FormatError, "field 2 was already read");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 4), 0xFFFFFFFFFFFFFFFF, "read max u64");
        TEST_ERROR(pckReadUInt64(packRead, 8), FormatError, "field 8 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 8), true, "field is null");
        TEST_RESULT_BOOL(pckReadNull(packRead, 9), false, "field is not null");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 9), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 11), 127, "read 0 (skip field 10)");

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
