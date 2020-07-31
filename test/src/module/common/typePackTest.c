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

        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{depth: 1, idLast: 0}", "log");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 0750), "write mode");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{depth: 1, idLast: 1}", "log");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 1911246845), "write timestamp");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 0xFFFFFFFFFFFFFFFF, .id = 7), "write max u64");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 1, .id = 10), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 77), "write 77");
        TEST_RESULT_VOID(pckWriteUInt32P(packWrite, 127, .id = 12), "write 127");
        TEST_RESULT_VOID(pckWriteInt64P(packWrite, -1, .id = 13), "write -1");
        TEST_RESULT_VOID(pckWriteInt32P(packWrite, -1), "write -1");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, true), "write true");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false, .id = 20), "write false");
        TEST_RESULT_VOID(pckWriteObjBeginP(packWrite, .id = 28), "write obj begin");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, true), "write true");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false), "write false");
        TEST_RESULT_VOID(pckWriteObjEnd(packWrite), "write obj end");
        TEST_RESULT_VOID(pckWriteArrayBeginP(packWrite, .id = 37), "write array begin");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 0), "write 0");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 2), "write 2");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 3), "write 3");
        TEST_RESULT_VOID(pckWriteArrayEnd(packWrite), "write array end");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("sample"), .id = 38), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("enoughtoincreasebuffer")), "write string");
        TEST_RESULT_VOID(pckWriteStrZP(packWrite, ""), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrZP(packWrite, "small", .id = 41), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrZP(packWrite, NULL, .id = 43), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrZP(packWrite, NULL), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");

        TEST_RESULT_VOID(pckWriteFree(packWrite), "free");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "8ae803"                                                //  1,  u64, 750
            "8afd9fad8f07"                                          //  2,  u64, 1911246845
            "ca01ffffffffffffffffff01"                              //  7,  u64, 0xFFFFFFFFFFFFFFFF
            "6a01"                                                  // 10,  u64, 1
            "8a4d"                                                  // 11,  u64, 77
            "897f"                                                  // 12,  u32, 127
            "45"                                                    // 13,  i64, -1
            "44"                                                    // 14,  i32, -1
            "83"                                                    // 15, bool, true
            "4301"                                                  // 20, bool, false
            "76"                                                    // 28, obj begin
                "83"                                                //      1, bool
                "03"                                                //      2, bool
                "00"                                                //         obj end
            "8101"                                                  // 37, array begin
                "0a"                                                //      1,  u64, 0
                "4a"                                                //      2,  u64, 1
                "8a02"                                              //      3,  u64, 2
                "8a03"                                              //      4,  u64, 3
                "00"                                                //         array end
            "880673616d706c65"                                      // 38,  str, sample
            "8816656e6f756768746f696e637265617365627566666572"      // 39,  str, enoughtoincreasebuffer
            "08"                                                    // 40,  str, zero length
            "8805736d616c6c"                                        // 41,  str, small
            "08"                                                    // 42,  str, zero length
            "28"                                                    // 45,  str, zero length
            "00",                                                   // end
            "check pack");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read pack");

        IoRead *read = ioBufferReadNew(pack);
        ioReadOpen(read);

        PackRead *packRead = NULL;
        TEST_ASSIGN(packRead, pckReadNew(read), "new read");

        TEST_RESULT_UINT(pckReadUInt64P(packRead), 0750, "read mode");
        TEST_RESULT_UINT(pckReadUInt64P(packRead), 1911246845, "read timestamp");
        TEST_ERROR(pckReadUInt64P(packRead, .id = 2), FormatError, "field 2 was already read");
        TEST_ERROR(pckReadUInt32P(packRead, .id = 7), FormatError, "field 7 is type 'uint64' but expected 'uint32'");
        TEST_RESULT_UINT(pckReadUInt64P(packRead, .id = 7), 0xFFFFFFFFFFFFFFFF, "read max u64");
        TEST_ERROR(pckReadUInt64P(packRead, 9), FormatError, "field 9 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 9), true, "field 9 is null");
        TEST_RESULT_BOOL(pckReadNull(packRead, 10), false, "field 10 is not null");
        TEST_RESULT_UINT(pckReadUInt64P(packRead, .id = 10), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt32P(packRead, .id = 12), 127, "read 127 (skip field 11)");
        TEST_RESULT_INT(pckReadInt64P(packRead), -1, "read -1");
        TEST_RESULT_INT(pckReadInt32P(packRead, 14), -1, "read -1");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 15), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 20), false, "read false");

        TEST_ERROR(pckReadObjEnd(packRead), FormatError, "not in object");
        TEST_RESULT_VOID(pckReadObjBeginP(packRead, 28), "read object begin");
        TEST_ERROR(pckReadArrayEnd(packRead), FormatError, "not in array");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), false, "read false");
        TEST_ERROR(pckReadBoolP(packRead), FormatError, "field 3 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 4), true, "field 3 is null");
        TEST_RESULT_VOID(pckReadObjEnd(packRead), "read object end");

        TEST_ERROR(pckReadArrayEnd(packRead), FormatError, "not in array");
        TEST_RESULT_VOID(pckReadArrayBeginP(packRead, .id = 37), "read array begin");
        TEST_ERROR(pckReadObjEnd(packRead), FormatError, "not in object");

        unsigned int value = 0;

        while (pckReadNext(packRead))
        {
            TEST_RESULT_UINT(pckReadUInt64P(packRead, .id = pckReadId(packRead)), value, "read %u", value);
            value++;
        }

        TEST_RESULT_VOID(pckReadArrayEnd(packRead), "read array end");

        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 39), "enoughtoincreasebuffer", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 41), "small", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStrP(packRead), "", "zero length (skipped prior)");
        TEST_RESULT_STR(pckReadStrNullP(packRead, 43), NULL, "read NULL string");
        TEST_RESULT_STR(pckReadStrNullP(packRead, 0), NULL, "read NULL string");
        TEST_RESULT_STR_Z(pckReadStrNullP(packRead, 0), "", "read empty string");

        TEST_ERROR(pckReadUInt64P(packRead, .id = 999), FormatError, "field 999 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 999), true, "field 999 is null");

        TEST_RESULT_VOID(pckReadEnd(packRead), "end");
        TEST_RESULT_VOID(pckReadFree(packRead), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid uint64");

        TEST_ASSIGN(packRead, pckReadNewBuf(BUFSTRDEF("\255\255\255\255\255\255\255\255\255")), "new read");
        TEST_ERROR(pckReadUInt64P(packRead), AssertError, "assertion 'bufPtr(this->buffer)[0] < 0x80' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack pointer");

        pack = bufNew(0);

        TEST_ASSIGN(packWrite, pckWriteNewBuf(pack), "new write");
        TEST_RESULT_VOID(pckWritePtrP(packWrite, "sample"), "write pointer");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "write end");

        TEST_ASSIGN(packRead, pckReadNewBuf(pack), "new read");
        TEST_RESULT_Z(pckReadPtrP(packRead, .id = 1), "sample", "read pointer");
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
