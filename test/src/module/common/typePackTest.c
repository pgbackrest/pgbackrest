/***********************************************************************************************************************************
Test Pack Type
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"

#include "common/harnessPack.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("PackWrite and PackRead"))
    {
        TEST_TITLE("type size");

        TEST_RESULT_UINT(sizeof(PackType), 4, "PackType");
        TEST_RESULT_UINT(sizeof(PackTypeMapData), 8, "PackTypeMapData");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write pack");

        Buffer *pack = bufNew(0);
        IoWrite *write = ioBufferWriteNewOpen(pack);
        PackWrite *packWrite = NULL;

        ioBufferSizeSet(3);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(packWrite, pckWriteMove(pckWriteNewIo(write), memContextPrior()), "move new write");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{depth: 0, idLast: 0}", "log");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 0750), "write mode");
        TEST_RESULT_STR_Z(pckWriteToLog(packWrite), "{depth: 0, idLast: 1}", "log");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 1911246845), "write timestamp");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 0xFFFFFFFFFFFFFFFF, .id = 7), "write max u64");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 1, .id = 10), "write 1");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 77), "write 77");
        TEST_RESULT_VOID(pckWriteU32P(packWrite, 127, .id = 12), "write 127");
        TEST_RESULT_VOID(pckWriteI64P(packWrite, -1, .id = 13), "write -1");
        TEST_RESULT_VOID(pckWriteI32P(packWrite, -1), "write -1");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, true), "write true");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false, .id = 20, .defaultWrite = true), "write false");
        TEST_RESULT_VOID(pckWriteObjBeginP(packWrite, .id = 28), "write obj begin");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, true), "write true");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false, .defaultWrite = true), "write false");
        TEST_RESULT_VOID(pckWriteStrIdP(packWrite, pckTypeTime, .id = 5), "write strid");
        TEST_RESULT_VOID(pckWriteStrIdP(packWrite, pckTypeTime, .defaultValue = pckTypeTime), "write default strid");
        TEST_RESULT_VOID(pckWriteModeP(packWrite, 0707, .id = 7), "write mode");
        TEST_RESULT_VOID(pckWriteModeP(packWrite, 0644, .defaultValue = 0644), "write default mode");
        TEST_RESULT_VOID(pckWriteObjEndP(packWrite), "write obj end");
        TEST_RESULT_VOID(pckWriteArrayBeginP(packWrite, .id = 37), "write array begin");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 0, .defaultWrite = true), "write 0");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 1), "write 1");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 2), "write 2");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 3), "write 3");
        TEST_RESULT_VOID(pckWriteArrayEndP(packWrite), "write array end");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("sample"), .id = 38), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("enoughtoincreasebuffer")), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, EMPTY_STR), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("small"), .id = 41), "write string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, NULL, .id = 43), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, NULL), "write NULL string");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("")), "write zero-length string");
        TEST_RESULT_VOID(pckWriteU32P(packWrite, 0), "write default 0");
        TEST_RESULT_VOID(pckWriteU32P(packWrite, 0, .defaultValue = 1), "write 0");
        TEST_RESULT_VOID(pckWriteArrayBeginP(packWrite), "write array begin");
        TEST_RESULT_VOID(pckWriteObjBeginP(packWrite), "write obj begin");
        TEST_RESULT_VOID(pckWriteI32P(packWrite, 555), "write 555");
        TEST_RESULT_VOID(pckWriteI32P(packWrite, 777, .id = 3), "write 777");
        TEST_RESULT_VOID(pckWriteI64P(packWrite, 0), "write 0");
        TEST_RESULT_VOID(pckWriteI64P(packWrite, 1), "write 1");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 0), "write 0");
        TEST_RESULT_VOID(pckWriteU64P(packWrite, 1), "write 1");
        TEST_RESULT_VOID(pckWriteObjEndP(packWrite), "write obj end");
        TEST_RESULT_VOID(pckWriteNull(packWrite), "write null");
        TEST_RESULT_VOID(
            pckWriteStrP(packWrite, STRDEF("A"), .defaultValue = STRDEF("")), "write A");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 0), "write null");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 33), "write 33");
        TEST_RESULT_VOID(pckWriteTimeP(packWrite, 66, .id = 6), "write 66");
        TEST_RESULT_VOID(pckWriteI32P(packWrite, 1, .defaultValue = 1), "write default 1");
        TEST_RESULT_VOID(pckWriteBoolP(packWrite, false), "write default false");
        TEST_RESULT_VOID(pckWriteArrayEndP(packWrite), "write array end");

        const unsigned char bin[] = {0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
        TEST_RESULT_VOID(pckWriteBinP(packWrite, BUF(bin, sizeof(bin))), "write bin");
        TEST_RESULT_VOID(pckWriteBinP(packWrite, NULL), "write bin NULL default");
        TEST_RESULT_VOID(pckWriteBinP(packWrite, bufNew(0)), "write bin zero length");

        // Write pack
        PackWrite *packSub = pckWriteNewP();
        pckWriteU64P(packSub, 345);
        pckWriteStrP(packSub, STRDEF("sub"), .id = 3);
        pckWriteEndP(packSub);

        TEST_RESULT_VOID(pckWritePackP(packWrite, pckWriteResult(packSub)), "write pack");
        TEST_RESULT_VOID(pckWritePackP(packWrite, NULL), "write null pack");

        // Write string list
        StringList *const strList = strLstNew();
        strLstAddZ(strList, "a");
        strLstAddZ(strList, "bcd");

        TEST_RESULT_VOID(pckWriteStrLstP(packWrite, strList), "write string list");
        TEST_RESULT_VOID(pckWriteStrLstP(packWrite, NULL), "write null string list");

        // End pack
        TEST_RESULT_VOID(pckWriteEndP(packWrite), "end");
        TEST_RESULT_VOID(pckWriteFree(packWrite), "free");

        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            hrnPackToStr(pckFromBuf(pack)),
               "1:u64:488"
             ", 2:u64:1911246845"
             ", 7:u64:18446744073709551615"
            ", 10:u64:1"
            ", 11:u64:77"
            ", 12:u32:127"
            ", 13:i64:-1"
            ", 14:i32:-1"
            ", 15:bool:true"
            ", 20:bool:false"
            ", 28:obj:"
            "{"
                  "1:bool:true"
                ", 2:bool:false"
                ", 5:strid:time"
                ", 7:mode:0707"
            "}"
            ", 37:array:"
            "["
                  "1:u64:0"
                ", 2:u64:1"
                ", 3:u64:2"
                ", 4:u64:3"
            "]"
            ", 38:str:sample"
            ", 39:str:enoughtoincreasebuffer"
            ", 40:str:"
            ", 41:str:small"
            ", 42:str:"
            ", 45:str:"
            ", 47:u32:0"
            ", 48:array:"
            "["
                  "1:obj:"
                "{"
                      "1:i32:555"
                    ", 3:i32:777"
                    ", 5:i64:1"
                    ", 7:u64:1"
                "}"
                ", 3:str:A"
                ", 5:time:33"
                ", 6:time:66"
            "]"
            ", 49:bin:050403020100"
            ", 51:bin:"
            ", 52:pack:<1:u64:345, 3:str:sub>"
            ", 54:array:[1:str:a, 2:str:bcd]",
            "check pack string");

        TEST_RESULT_STR_Z(
            bufHex(pack),
            "98e803"                                                //  1,  u64, 750
            "98fd9fad8f07"                                          //  2,  u64, 1911246845
            "9c01ffffffffffffffffff01"                              //  7,  u64, 0xFFFFFFFFFFFFFFFF
            "9601"                                                  // 10,  u64, 1
            "984d"                                                  // 11,  u64, 77
            "887f"                                                  // 12,  u32, 127
            "44"                                                    // 13,  i64, -1
            "34"                                                    // 14,  i32, -1
            "28"                                                    // 15, bool, true
            "2401"                                                  // 20, bool, false
            "57"                                                    // 28, obj begin
                "28"                                                //      1, bool
                "20"                                                //      2, bool
                "aac0a6ad01"                                        //      5, strid time
                "f903c703"                                          //      7, mode 0707
                "00"                                                //     obj end
            "1801"                                                  // 37, array begin
                "90"                                                //      1,  u64, 0
                "94"                                                //      2,  u64, 1
                "9802"                                              //      3,  u64, 2
                "9803"                                              //      4,  u64, 3
                "00"                                                //     array end
            "780673616d706c65"                                      // 38,  str, sample
            "7816656e6f756768746f696e637265617365627566666572"      // 39,  str, enoughtoincreasebuffer
            "70"                                                    // 40,  str, zero length
            "7805736d616c6c"                                        // 41,  str, small
            "70"                                                    // 42,  str, zero length
            "72"                                                    // 45,  str, zero length
            "81"                                                    // 47,  u32, 0
            "10"                                                    // 48, array begin
                "50"                                                //      1, obj begin
                    "38d608"                                        //           1, i32, 555
                    "39920c"                                        //           3, i32, 777
                    "4902"                                          //           5, i64, 1
                    "95"                                            //           7, u64, 1
                    "00"                                            //         obj end
                "790141"                                            //      3,  str, A
                "f90042"                                            //      5, time, 33
                "f8008401"                                          //      6, time, 66
                "00"                                                //     array end
            "f80106050403020100"                                    // 49,  bin, 0x050403020100
            "f101"                                                  // 51,  bin, zero length
            "f0020998d902790373756200"                              // 52,  pack, 1:u64:345, 3:str:sub
            "11780161780362636400"                                  // 54,  strlst, 1:str:a, 2:str:bcd
            "00",                                                   // end
            "check pack hex");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read pack");

        IoRead *read = ioBufferReadNewOpen(pack);

        PackRead *packRead = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(packRead, pckReadMove(pckReadNewIo(read), memContextPrior()), "move new read");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_UINT(pckReadU64P(packRead), 0750, "read mode");
        TEST_RESULT_UINT(pckReadU64P(packRead), 1911246845, "read timestamp");
        TEST_ERROR(pckReadU64P(packRead, .id = 2), FormatError, "field 2 was already read");
        TEST_ERROR(pckReadU32P(packRead, .id = 7), FormatError, "field 7 is type 'u64' but expected 'u32'");
        TEST_RESULT_UINT(pckReadU64P(packRead, .id = 7), 0xFFFFFFFFFFFFFFFF, "read max u64");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 9), true, "field 9 is null");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 10), false, "field 10 is not null");
        TEST_RESULT_UINT(pckReadU64P(packRead, .id = 10), 1, "read 1");
        TEST_RESULT_UINT(pckReadU32P(packRead, .id = 12), 127, "read 127 (skip field 11)");
        TEST_RESULT_INT(pckReadI64P(packRead), -1, "read -1");
        TEST_RESULT_INT(pckReadI32P(packRead, .id = 14), -1, "read -1");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 15), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead, .id = 20), false, "read false");

        TEST_ERROR(pckReadObjEndP(packRead), FormatError, "not in obj");
        TEST_RESULT_VOID(pckReadObjBeginP(packRead, .id = 28), "read object begin");
        TEST_ERROR(pckReadArrayEndP(packRead), FormatError, "not in array");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), true, "read true");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), false, "read false");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), false, "field 4 default is false");
        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 4), true, "field 4 is null");
        TEST_RESULT_UINT(pckReadStrIdP(packRead), pckTypeTime, "read strid");
        TEST_RESULT_UINT(pckReadStrIdP(packRead, .defaultValue = pckTypeTime), pckTypeTime, "read default strid");
        TEST_RESULT_UINT(pckReadModeP(packRead), 0707, "read mode");
        TEST_RESULT_UINT(pckReadModeP(packRead, .defaultValue = 0644), 0644, "read default mode");
        TEST_RESULT_BOOL(pckReadBoolP(packRead), false, "read default false");
        TEST_RESULT_VOID(pckReadObjEndP(packRead), "read object end");

        TEST_ERROR(pckReadArrayEndP(packRead), FormatError, "not in array");
        TEST_RESULT_BOOL(pckReadNext(packRead), true, "read next tag which should be an array");
        TEST_RESULT_UINT(pckReadId(packRead), 37, "check array id");
        TEST_RESULT_VOID(pckReadArrayBeginP(packRead, .id = pckReadId(packRead)), "read array begin");

        TEST_ERROR(pckReadObjEndP(packRead), FormatError, "not in obj");

        unsigned int value = 0;

        while (pckReadNext(packRead))
        {
            TEST_RESULT_UINT(pckReadU64P(packRead, .id = pckReadId(packRead)), value, zNewFmt("read %u", value));
            value++;
        }

        TEST_RESULT_VOID(pckReadArrayEndP(packRead), "read array end");

        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 39), "enoughtoincreasebuffer", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 41), "small", "read string (skipped prior)");
        TEST_RESULT_STR_Z(pckReadStrP(packRead), "", "zero length (skipped prior)");
        TEST_RESULT_STR(pckReadStrP(packRead, .id = 43), NULL, "read NULL string");
        TEST_RESULT_STR(pckReadStrP(packRead), NULL, "read NULL string");
        TEST_RESULT_STR_Z(pckReadStrP(packRead), "", "read empty string");

        TEST_RESULT_UINT(pckReadU32P(packRead), 0, "read default 0");
        TEST_RESULT_UINT(pckReadU32P(packRead, .id = 47), 0, "read 0");

        TEST_RESULT_VOID(pckReadArrayBeginP(packRead), "read array begin");
        TEST_RESULT_VOID(pckReadObjBeginP(packRead), "read object begin");
        TEST_RESULT_INT(pckReadI32P(packRead), 555, "read 0");
        TEST_RESULT_INT(pckReadI32P(packRead, .id = 3), 777, "read 0");
        TEST_RESULT_INT(pckReadI64P(packRead, .defaultValue = 44), 44, "read default 44");
        TEST_RESULT_INT(pckReadI64P(packRead, .defaultValue = 44), 1, "read 1");
        TEST_RESULT_UINT(pckReadU64P(packRead, .defaultValue = 55), 55, "read default 55");
        TEST_RESULT_UINT(pckReadU64P(packRead, .defaultValue = 55), 1, "read 1");
        TEST_RESULT_VOID(pckReadObjEndP(packRead), "read object end");
        TEST_RESULT_STR_Z(pckReadStrP(packRead, .id = 3), "A", "read A");
        TEST_RESULT_INT(pckReadTimeP(packRead, .defaultValue = 99), 99, "read default 99");
        TEST_RESULT_INT(pckReadTimeP(packRead, .id = 5, .defaultValue = 44), 33, "read 33");
        TEST_RESULT_INT(pckReadI32P(packRead, .id = 7, .defaultValue = 1), 1, "read default 1");
        TEST_RESULT_VOID(pckReadArrayEndP(packRead), "read array end");

        TEST_RESULT_STR_Z(bufHex(pckReadBinP(packRead)), "050403020100", "read bin");
        TEST_RESULT_PTR(pckReadBinP(packRead), NULL, "read bin null");
        TEST_RESULT_UINT(bufSize(pckReadBinP(packRead)), 0, "read bin zero length");

        TEST_RESULT_STR_Z(hrnPackReadToStr(pckReadPackReadP(packRead)), "1:u64:345, 3:str:sub", "read pack");
        TEST_RESULT_PTR(pckReadPackReadP(packRead), NULL, "read null pack");

        TEST_RESULT_STRLST_Z(pckReadStrLstP(packRead), "a\nbcd\n", "read string list");
        TEST_RESULT_PTR(pckReadStrLstP(packRead), NULL, "read null string list");

        TEST_RESULT_BOOL(pckReadNullP(packRead, .id = 999), true, "field 999 is null");
        TEST_RESULT_UINT(pckReadU64P(packRead, .id = 1000), 0, "field 1000 default is 0");

        TEST_RESULT_VOID(pckReadEndP(packRead), "end");
        TEST_RESULT_VOID(pckReadFree(packRead), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("EOF on short buffer");

        TEST_ASSIGN(packRead, pckReadNew(pckFromBuf(BUFSTRDEF(""))), "new read");
        TEST_ERROR(pckReadBuffer(packRead, 2), FormatError, "unexpected EOF");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid uint64");

        TEST_ASSIGN(packRead, pckReadNew(pckFromBuf(BUFSTRDEF("\255\255\255\255\255\255\255\255\255\255"))), "new read");
        TEST_ERROR(pckReadU64Internal(packRead), FormatError, "unterminated varint-128 integer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack pointer");

        TEST_ASSIGN(packWrite, pckWriteNewP(.size = 1), "new write");
        TEST_RESULT_VOID(pckWritePtrP(packWrite, NULL), "write default pointer");
        TEST_RESULT_VOID(pckWritePtrP(packWrite, "sample"), "write pointer");
        TEST_RESULT_VOID(pckWriteEndP(packWrite), "write end");

        TEST_ASSIGN(packRead, pckReadNew(pckDup(pckWriteResult(packWrite))), "new read");
        TEST_RESULT_Z(pckReadPtrP(packRead), NULL, "read default pointer");
        TEST_RESULT_Z(pckReadPtrP(packRead, .id = 2), "sample", "read pointer");

        TEST_RESULT_PTR(pckWriteResult(NULL), NULL, "null pack result");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read const packs");

        TEST_ASSIGN(packWrite, pckWriteNewP(), "new write");

        // Write pack to read as ptr/size
        packSub = pckWriteNewP();
        pckWriteU64P(packSub, 777);
        pckWriteEndP(packSub);

        TEST_RESULT_VOID(pckWritePackP(packWrite, pckWriteResult(packSub)), "write pack");

        // Write pack to read as const
        TEST_RESULT_VOID(pckWritePackP(packWrite, NULL), "write pack");

        packSub = pckWriteNewP();
        pckWriteU64P(packSub, 99);
        pckWriteEndP(packSub);

        TEST_RESULT_VOID(pckWritePackP(packWrite, pckWriteResult(packSub)), "write pack");
        TEST_RESULT_VOID(pckWriteEndP(packWrite), "write pack end");

        TEST_ASSIGN(packRead, pckReadNew(pckWriteResult(packWrite)), "new read");

        TEST_RESULT_BOOL(pckReadNext(packRead), true, "next pack");
        TEST_RESULT_UINT(pckReadSize(packRead), 4, "pack size");
        TEST_RESULT_STR_Z(bufHex(BUF(pckReadBufPtr(packRead), pckReadSize(packRead))), "98890600", "pack hex");
        TEST_RESULT_UINT(pckReadU64P(pckReadNewC(pckReadBufPtr(packRead), pckReadSize(packRead))), 777, "u64 value");
        TEST_RESULT_VOID(pckReadConsume(packRead), "consume pack");

        TEST_RESULT_PTR(pckReadPackReadConstP(packRead), NULL, "const null pack");
        TEST_RESULT_UINT(pckReadU64P(pckReadPackReadConstP(packRead)), 99, "const pack");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack/unpack write internal buffer empty");

        pack = bufNew(0);
        write = ioBufferWriteNew(pack);
        ioWriteOpen(write);

        // Make internal buffer small enough that it will never be used
        ioBufferSizeSet(0);

        TEST_ASSIGN(packWrite, pckWriteNewIo(write), "new write");
        TEST_RESULT_VOID(pckWriteStrP(packWrite, STRDEF("test")), "write string longer than internal buffer");
        TEST_RESULT_VOID(pckWriteEndP(packWrite), "end with internal buffer empty");

        TEST_ASSIGN(packRead, pckReadNew(pckFromBuf(pack)), "new read");
        TEST_RESULT_STR_Z(pckReadStrP(packRead), "test", "read string");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
