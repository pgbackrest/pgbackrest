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
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 7, 0xFFFFFFFFFFFFFFFF), "write max u64");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 10, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 11, 77), "write 77");
        TEST_RESULT_VOID(pckWriteUInt32(packWrite, 12, 127), "write 127");
        TEST_RESULT_VOID(pckWriteInt64(packWrite, 13, -1), "write -1");
        TEST_RESULT_VOID(pckWriteInt32(packWrite, 14, -1), "write -1");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 15, true), "write true");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 20, false), "write false");
        TEST_RESULT_VOID(pckWriteObj(packWrite, 28), "write obj");
        TEST_RESULT_VOID(pckWriteObj(packWrite, 37), "write obj");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");

        TEST_RESULT_VOID(pckWriteFree(packWrite), "free");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "8ae803"                                                //  1, u64, 750
            "8afd9fad8f07"                                          //  2, u64, 1911246845
            "ca01ffffffffffffffffff01"                              //  7, u64, 0xFFFFFFFFFFFFFFFF
            "6a01"                                                  // 10, u64, 1
            "8a4d"                                                  // 11, u64, 77
            "897f"                                                  // 12, u32, 127
            "45"                                                    // 13, i64, -1
            "44"                                                    // 14, i32, -1
            "83"                                                    // 15, bool, true
            "4301"                                                  // 20, bool, false
            "76"                                                    // 28,  obj
            "8601"                                                  // 37,  obj
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
        TEST_ERROR(pckReadUInt32(packRead, 7), FormatError, "field 7 is type 'uint64' but expected 'uint32'");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 7), 0xFFFFFFFFFFFFFFFF, "read max u64");
        TEST_ERROR(pckReadUInt64(packRead, 9), FormatError, "field 9 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 9), true, "field 9 is null");
        TEST_RESULT_BOOL(pckReadNull(packRead, 10), false, "field 10 is not null");
        TEST_RESULT_UINT(pckReadUInt64(packRead, 10), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt32(packRead, 12), 127, "read 127 (skip field 11)");
        TEST_RESULT_INT(pckReadInt64(packRead, 13), -1, "read -1");
        TEST_RESULT_INT(pckReadInt32(packRead, 14), -1, "read -1");
        TEST_RESULT_BOOL(pckReadBool(packRead, 15), true, "read true");
        TEST_RESULT_BOOL(pckReadBool(packRead, 20), false, "read false");
        TEST_RESULT_VOID(pckReadObj(packRead, 28), "read object");

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
