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
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 1, 0750), "write mode");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{depth: 1, idLast: 1}", "log");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 2, 1911246845), "write timestamp");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 7, 0xFFFFFFFFFFFFFFFF), "write max u64");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 10, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 11, 77), "write 77");
        TEST_RESULT_VOID(pckWriteUInt32(packWrite, 12, 127), "write 127");
        TEST_RESULT_VOID(pckWriteInt64(packWrite, 13, -1), "write -1");
        TEST_RESULT_VOID(pckWriteInt32(packWrite, 14, -1), "write -1");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 15, true), "write true");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 20, false), "write false");
        TEST_RESULT_VOID(pckWriteObjBegin(packWrite, 28), "write obj begin");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 1, true), "write true");
        TEST_RESULT_VOID(pckWriteBool(packWrite, 2, false), "write false");
        TEST_RESULT_VOID(pckWriteObjEnd(packWrite), "write obj end");
        TEST_RESULT_VOID(pckWriteArrayBegin(packWrite, 37), "write array begin");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 1, 0), "write 0");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 2, 1), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64(packWrite, 3, 2), "write 2");
        TEST_RESULT_VOID(pckWriteArrayEnd(packWrite), "write array end");
        TEST_RESULT_VOID(pckWriteStr(packWrite, 38, STRDEF("sample")), "write string");
        TEST_RESULT_VOID(pckWriteStr(packWrite, 39, STRDEF("enoughtoincreasebuffer")), "write string");
        TEST_RESULT_VOID(pckWriteStrZ(packWrite, 0, ""), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrZ(packWrite, 41, "small"), "write string");
        TEST_RESULT_VOID(pckWriteStr(packWrite, 0, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrZ(packWrite, 43, NULL), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrZ(packWrite, 0, NULL), "write NULL string");
        TEST_RESULT_VOID(pckWriteStr(packWrite, 0, STRDEF("")), "write zero-length string");
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
            TEST_RESULT_UINT(pckReadUInt64(packRead, pckReadId(packRead)), value, "read %u", value);
            value++;
        }

        TEST_RESULT_VOID(pckReadArrayEnd(packRead), "read array end");

        TEST_RESULT_STR_Z(pckReadStr(packRead, 39), "enoughtoincreasebuffer", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStr(packRead, 41), "small", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStr(packRead, 0), "", "zero length (skipped prior)");
        TEST_RESULT_STR(pckReadStrNull(packRead, 43), NULL, "read NULL string");
        TEST_RESULT_STR(pckReadStrNull(packRead, 0), NULL, "read NULL string");
        TEST_RESULT_STR_Z(pckReadStrNull(packRead, 0), "", "read empty string");

        TEST_ERROR(pckReadUInt64(packRead, 999), FormatError, "field 999 does not exist");
        TEST_RESULT_BOOL(pckReadNull(packRead, 999), true, "field 999 is null");

        TEST_RESULT_VOID(pckReadEnd(packRead), "end");
        TEST_RESULT_VOID(pckReadFree(packRead), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid uint64");

        TEST_ASSIGN(packRead, pckReadNewBuf(BUFSTRDEF("\255\255\255\255\255\255\255\255\255")), "new read");
        TEST_ERROR(pckReadUInt64(packRead, 1), AssertError, "assertion 'bufPtr(this->buffer)[0] < 0x80' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack pointer");

        pack = bufNew(0);

        TEST_ASSIGN(packWrite, pckWriteNewBuf(pack), "new write");
        TEST_RESULT_VOID(pckWritePtr(packWrite, 1, "sample"), "write pointer");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "write end");

        TEST_ASSIGN(packRead, pckReadNewBuf(pack), "new read");
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
