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

        IoWrite *write = ioBufferWriteNew(pack);
        ioWriteOpen(write);

        PackWrite *packWrite = NULL;
        TEST_ASSIGN(packWrite, pckWriteNew(write), "new write");

        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{idLast: 0}", "log");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 1, 0750), "write mode");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{idLast: 1}", "log");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 2, 1911246845), "write timestamp");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 4, 0xFFFFFFFFFFFFFFFF), "write max u64");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 20, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 21, 77), "write 77");
        TEST_RESULT_VOID(pckWriteUInt32(packWrite, 22, 127), "write 127");
        TEST_RESULT_VOID(pckWriteInt64(packWrite, 23, -1), "write -1");
        TEST_RESULT_VOID(pckWriteInt32(packWrite, 24, -1), "write -1");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");

        TEST_RESULT_VOID(pckWriteFree(packWrite), "free");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "8ae803"                                                //  1, u64, 750
            "8afd9fad8f07"                                          //  2, u64, 1911246845
            "9affffffffffffffffff01"                                //  4, u64, 0xFFFFFFFFFFFFFFFF
            "7a07"                                                  // 20, u64, 1
            "8a4d"                                                  // 21, u64, 77
            "897f"                                                  // 22, u32, 127
            "45"                                                    // 23, i64, -1
            "44"                                                    // 24, i32, -1
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
        TEST_ERROR(pckReadUInt32(packRead, 4), FormatError, "field 4 is type 10 but expected 9");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 4), 0xFFFFFFFFFFFFFFFF, "read max u64");
        TEST_ERROR(pckReadUInt64(packRead, 19), FormatError, "field 19 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 19), true, "field 19 is null");
        TEST_RESULT_BOOL(pckReadNull(packRead, 20), false, "field 20 is not null");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 20), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt32(packRead, 22), 127, "read 127 (skip field 21)");
        TEST_RESULT_INT(pckReadInt64(packRead, 23), -1, "read -1");
        TEST_RESULT_INT(pckReadInt32(packRead, 24), -1, "read -1");

        TEST_ERROR(pckReadUInt64(packRead, 999), FormatError, "field 999 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 999), true, "field 999 is null");

        TEST_RESULT_VOID(pckReadFree(packRead), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid uint64");

        read = ioBufferReadNew(BUFSTRDEF("\255\255\255\255\255\255\255\255\255"));
        ioReadOpen(read);

        packRead = NULL;
        TEST_ASSIGN(packRead, pckReadNew(read), "new read");

        TEST_ERROR(pckReadUInt64(packRead, 1), AssertError, "assertion 'bufPtr(this->buffer)[0] < 0x80' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack pointer");

        pack = bufNew(0);

        write = ioBufferWriteNew(pack);
        ioWriteOpen(write);

        TEST_ASSIGN(packWrite, pckWriteNew(write), "new write");
        TEST_RESULT_VOID(pckWritePtr(packWrite, 1, "sample"), "write pointer");

        ioWriteClose(write);

        read = ioBufferReadNew(pack);
        ioReadOpen(read);

        TEST_ASSIGN(packRead, pckReadNew(read), "new read");
        TEST_RESULT_Z(pckReadPtr(packRead, 1), "sample", "read pointer");
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
