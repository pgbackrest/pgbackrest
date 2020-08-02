/***********************************************************************************************************************************
Test Packs
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"

#include "common/harnessPack.h"

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

        ioBufferSizeSet(3);

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
        TEST_RESULT_VOID(pckWriteStrP(packWrite, EMPTY_STR), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("small"), .id = 41), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, NULL, .id = 43, .defaultNull = true), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, NULL, .defaultNull = true), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteUInt32P(packWrite, 0, .defaultNull = true), "write default 0");
        TEST_RESULT_VOID(pckWriteUInt32P(packWrite, 0, .defaultNull = true, .defaultValue = 1), "write 0");
        TEST_RESULT_VOID(pckWriteArrayBeginP(packWrite), "write array begin");
        TEST_RESULT_VOID(pckWriteObjBeginP(packWrite), "write obj begin");
        TEST_RESULT_VOID(pckWriteInt32P(packWrite, 555), "write 555");
        TEST_RESULT_VOID(pckWriteInt32P(packWrite, 777, .id = 3), "write 777");
        TEST_RESULT_VOID(pckWriteInt64P(packWrite, 0, .defaultNull = true), "write 0");
        TEST_RESULT_VOID(pckWriteInt64P(packWrite, 1, .defaultNull = true), "write 1");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 0, .defaultNull = true), "write 0");
        TEST_RESULT_VOID(pckWriteUInt64P(packWrite, 1, .defaultNull = true), "write 1");
        TEST_RESULT_VOID(pckWriteObjEnd(packWrite), "write obj end");
        TEST_RESULT_VOID(pckWriteNull(packWrite), "write null");
        TEST_RESULT_VOID(
            pckWriteStrP(packWrite, STRDEF("A"), .defaultNull = true, .defaultValue = STRDEF("")), "write A");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 0, .defaultNull = true), "write null");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 33, .defaultNull = true), "write 33");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 66, .id = 6), "write 66");
        TEST_RESULT_VOID(pckWriteInt32P(packWrite, 1, .defaultNull = true, .defaultValue = 1), "write default 1");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false, .defaultNull = true), "write default false");
        TEST_RESULT_VOID(pckWriteArrayEnd(packWrite), "write array end");

        const unsigned char bin[] = {0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
        TEST_RESULT_VOID(pckWriteBinP(packWrite, BUF(bin, sizeof(bin))), "write bin");
        TEST_RESULT_VOID(pckWriteBinP(packWrite, NULL, .defaultNull = true), "write bin NULL default");
        TEST_RESULT_VOID(pckWriteBinP(packWrite, bufNew(0)), "write bin zero length");

        TEST_RESULT_VOID(pckWriteEnd(packWrite), "end");
        TEST_RESULT_VOID(pckWriteFree(packWrite), "free");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            hrnPackBufToStr(pack),
               "1:uint64:488"
             ", 2:uint64:1911246845"
             ", 7:uint64:18446744073709551615"
            ", 10:uint64:1"
            ", 11:uint64:77"
            ", 12:uint32:127"
            ", 13:int64:-1"
            ", 14:int32:-1"
            ", 15:bool:true"
            ", 20:bool:false"
            ", 28:obj:"
            "{"
                  "1:bool:true"
                ", 2:bool:false"
            "}"
            ", 37:array:"
            "["
                  "1:uint64:0"
                ", 2:uint64:1"
                ", 3:uint64:2"
                ", 4:uint64:3"
            "]"
            ", 38:str:sample"
            ", 39:str:enoughtoincreasebuffer"
            ", 40:str:"
            ", 41:str:small"
            ", 42:str:"
            ", 45:str:"
            ", 47:uint32:0"
            ", 48:array:"
            "["
                  "1:obj:"
                "{"
                      "1:int32:555"
                    ", 3:int32:777"
                    ", 5:int64:1"
                    ", 7:uint64:1"
                "}"
                ", 3:str:A"
                ", 5:time:33"
                ", 6:time:66"
            "]"
            ", 49:bin:050403020100"
            ", 51:bin:",
            "check pack string");

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "b8e803"                                                //  1,  u64, 750
            "b8fd9fad8f07"                                          //  2,  u64, 1911246845
            "bc01ffffffffffffffffff01"                              //  7,  u64, 0xFFFFFFFFFFFFFFFF
            "b601"                                                  // 10,  u64, 1
            "b84d"                                                  // 11,  u64, 77
            "a87f"                                                  // 12,  u32, 127
            "54"                                                    // 13,  i64, -1
            "44"                                                    // 14,  i32, -1
            "38"                                                    // 15, bool, true
            "3401"                                                  // 20, bool, false
            "67"                                                    // 28, obj begin
                "38"                                                //      1, bool
                "30"                                                //      2, bool
                "00"                                                //     obj end
            "1801"                                                  // 37, array begin
                "b0"                                                //      1,  u64, 0
                "b4"                                                //      2,  u64, 1
                "b802"                                              //      3,  u64, 2
                "b803"                                              //      4,  u64, 3
                "00"                                                //     array end
            "880673616d706c65"                                      // 38,  str, sample
            "8816656e6f756768746f696e637265617365627566666572"      // 39,  str, enoughtoincreasebuffer
            "80"                                                    // 40,  str, zero length
            "8805736d616c6c"                                        // 41,  str, small
            "80"                                                    // 42,  str, zero length
            "82"                                                    // 45,  str, zero length
            "a1"                                                    // 47,  u32, 0
            "10"                                                    // 48, array begin
                "60"                                                //      1, obj begin
                    "48d608"                                        //           1, int32, 555
                    "49920c"                                        //           3, int32, 777
                    "5902"                                          //           5, int64, 1
                    "b5"                                            //           7, uint64, 1
                    "00"                                            //         obj end
                "890141"                                            //      3,  str, A
                "9942"                                              //      5, time, 33
                "988401"                                            //      6, time, 66
                "00"                                                //     array end
            "2806050403020100"                                      // 49,  bin, 0x050403020100
            "21"                                                    // 51,  bin, zero length
            "00",                                                   // end
            "check pack hex");

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
        TEST_ERROR(pckReadUInt64P(packRead, .id = 9), FormatError, "field 9 does not exist");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 9), true, "field 9 is null");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 10), false, "field 10 is not null");
        TEST_RESULT_UINT(pckReadUInt64P(packRead, .id = 10), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt32P(packRead, .id = 12, .defaultNull = true), 127, "read 127 (skip field 11)");
        TEST_RESULT_INT(pckReadInt64P(packRead), -1, "read -1");
        TEST_RESULT_INT(pckReadInt32P(packRead, .id = 14), -1, "read -1");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 15), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 20), false, "read false");

        TEST_ERROR(pckReadObjEnd(packRead), FormatError, "not in object");
        TEST_RESULT_VOID(pckReadObjBeginP(packRead, .id = 28), "read object begin");
        TEST_ERROR(pckReadArrayEnd(packRead), FormatError, "not in array");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .defaultNull = true), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), false, "read false");
        TEST_ERROR(pckReadBoolP(packRead), FormatError, "field 3 does not exist");
        TEST_RESULT_BOOL(pckReadNullP(packRead), true, "field 3 is null");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 4), true, "field 4 is null");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .defaultNull = true), false, "read default false");
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
        TEST_RESULT_STR(pckReadStrP(packRead, .id = 43, .defaultNull = true), NULL, "read NULL string");
        TEST_RESULT_STR(pckReadStrP(packRead, .defaultNull = true), NULL, "read NULL string");
        TEST_RESULT_STR_Z(pckReadStrP(packRead, .defaultNull = true), "", "read empty string");

        TEST_RESULT_UINT(pckReadUInt32P(packRead, .defaultNull = true), 0, "read default 0");
        TEST_RESULT_UINT(pckReadUInt32P(packRead, .id = 47), 0, "read 0");

        TEST_RESULT_VOID(pckReadArrayBeginP(packRead), "read array begin");
        TEST_RESULT_VOID(pckReadObjBeginP(packRead), "read object begin");
        TEST_RESULT_INT(pckReadInt32P(packRead), 555, "read 0");
        TEST_RESULT_INT(pckReadInt32P(packRead, .id = 3), 777, "read 0");
        TEST_RESULT_INT(pckReadInt64P(packRead, .defaultNull = true, .defaultValue = 44), 44, "read default 44");
        TEST_RESULT_INT(pckReadInt64P(packRead, .defaultNull = true, .defaultValue = 44), 1, "read 1");
        TEST_RESULT_UINT(pckReadUInt64P(packRead, .defaultNull = true, .defaultValue = 55), 55, "read default 55");
        TEST_RESULT_UINT(pckReadUInt64P(packRead, .defaultNull = true, .defaultValue = 55), 1, "read 1");
        TEST_RESULT_VOID(pckReadObjEnd(packRead), "read object end");
        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 3), "A", "read A");
        TEST_RESULT_INT(pckReadTimeP(packRead, .defaultNull = true, .defaultValue = 99), 99, "read default 99");
        TEST_RESULT_INT(pckReadTimeP(packRead, .id = 5, .defaultNull = true, .defaultValue = 44), 33, "read 33");
        TEST_RESULT_INT(pckReadInt32P(packRead, .id = 7, .defaultNull = true, .defaultValue = 1), 1, "read default 1");
        TEST_RESULT_VOID(pckReadArrayEnd(packRead), "read array end");

        TEST_RESULT_STR_Z(bufHex(pckReadBinP(packRead)), "050403020100", "read bin");
        TEST_RESULT_PTR(pckReadBinP(packRead, .defaultNull = true), NULL, "read bin null");
        TEST_RESULT_UINT(bufSize(pckReadBinP(packRead)), 0, "read bin zero length");

        TEST_ERROR(pckReadUInt64P(packRead, .id = 999), FormatError, "field 999 does not exist");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 999), true, "field 999 is null");

        TEST_RESULT_VOID(pckReadEnd(packRead), "end");
        TEST_RESULT_VOID(pckReadFree(packRead), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("EOF on short buffer");

        TEST_ASSIGN(packRead, pckReadNewBuf(BUFSTRDEF("\255")), "new read");
        TEST_ERROR(pckReadUInt64Internal(packRead), FormatError, "unexpected EOF");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid uint64");

        TEST_ASSIGN(packRead, pckReadNewBuf(BUFSTRDEF("\255\255\255\255\255\255\255\255\255\255")), "new read");
        TEST_ERROR(pckReadUInt64Internal(packRead), FormatError, "unterminated base-128 integer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack pointer");

        pack = bufNew(0);

        TEST_ASSIGN(packWrite, pckWriteNewBuf(pack), "new write");
        TEST_RESULT_VOID(pckWritePtrP(packWrite, NULL, .defaultNull = true), "write default pointer");
        TEST_RESULT_VOID(pckWritePtrP(packWrite, "sample"), "write pointer");
        TEST_RESULT_VOID(pckWriteEnd(packWrite), "write end");

        TEST_ASSIGN(packRead, pckReadNewBuf(pack), "new read");
        TEST_RESULT_Z(pckReadPtrP(packRead, .defaultNull = true), NULL, "read default pointer");
        TEST_RESULT_Z(pckReadPtrP(packRead, .id = 2), "sample", "read pointer");
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
