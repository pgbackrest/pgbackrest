/***********************************************************************************************************************************
Test IO
***********************************************************************************************************************************/
#include <fcntl.h>

#include "common/assert.h"

/***********************************************************************************************************************************
Test functions for IoRead that are not covered by testing the IoBufferRead object
***********************************************************************************************************************************/
static bool
testIoReadOpen(void *driver)
{
    ASSERT(driver == (void *)999);
    return false;
}

static size_t
testIoReadProcess(void *driver, Buffer *buffer)
{
    ASSERT(driver == (void *)999);
    bufCat(buffer, bufNewStr(strNew("Z")));
    return 1;
}

static bool testIoReadCloseCalled = false;

static void
testIoReadClose(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoReadCloseCalled = true;
}

/***********************************************************************************************************************************
Test functions for IoWrite that are not covered by testing the IoBufferWrite object
***********************************************************************************************************************************/
static bool testIoWriteOpenCalled = false;

static void
testIoWriteOpen(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoWriteOpenCalled = true;
}

static void
testIoWriteProcess(void *driver, const Buffer *buffer)
{
    ASSERT(driver == (void *)999);
    ASSERT(strEq(strNewBuf(buffer), strNew("ABC")));
}

static bool testIoWriteCloseCalled = false;

static void
testIoWriteClose(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoWriteCloseCalled = true;
}

/***********************************************************************************************************************************
Test filter that counts total bytes
***********************************************************************************************************************************/
typedef struct IoTestFilterSize
{
    MemContext *memContext;
    size_t size;
    IoFilter *filter;
} IoTestFilterSize;

static void
ioTestFilterSizeProcess(IoTestFilterSize *this, const Buffer *buffer)
{
    this->size += bufUsed(buffer);
}

static const Variant *
ioTestFilterSizeResult(IoTestFilterSize *this)
{
    return varNewUInt64(this->size);
}

static IoTestFilterSize *
ioTestFilterSizeNew(const char *type)
{
    IoTestFilterSize *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoTestFilterSize")
    {
        this = memNew(sizeof(IoTestFilterSize));
        this->memContext = MEM_CONTEXT_NEW();

        this->filter = ioFilterNew(
            strNew(type), this, (IoFilterProcessIn)ioTestFilterSizeProcess, (IoFilterResult)ioTestFilterSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("ioBufferSize() and ioBufferSizeSet()"))
    {
        TEST_RESULT_SIZE(ioBufferSize(), 65536, "check initial buffer size");
        TEST_RESULT_VOID(ioBufferSizeSet(16384), "set buffer size");
        TEST_RESULT_SIZE(ioBufferSize(), 16384, "check buffer size");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoRead and IoBufferRead"))
    {
        IoRead *read = NULL;
        Buffer *buffer = bufNew(2);

        TEST_ASSIGN(
            read, ioReadNew((void *)999, testIoReadOpen, testIoReadProcess, testIoReadClose, NULL), "create io read object");

        TEST_RESULT_BOOL(ioReadOpen(read), false, "    open io object");
        TEST_RESULT_SIZE(ioRead(read, buffer), 1, "    read 1 byte");
        TEST_RESULT_BOOL(ioReadEof(read), false, "    no eof");
        TEST_RESULT_VOID(ioReadClose(read), "    close io object");
        TEST_RESULT_BOOL(testIoReadCloseCalled, true, "    check io object closed");

        // -------------------------------------------------------------------------------------------------------------------------
        IoBufferRead *bufferRead = NULL;
        buffer = bufNew(2);
        Buffer *bufferOriginal = bufNewStr(strNew("123"));

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create buffer read object");
            TEST_RESULT_VOID(ioBufferReadMove(bufferRead, MEM_CONTEXT_OLD()), "    move object to new context");
            TEST_RESULT_VOID(ioBufferReadMove(NULL, MEM_CONTEXT_OLD()), "    move NULL object to new context");
        }
        MEM_CONTEXT_TEMP_END();

        IoFilterGroup *filterGroup = NULL;
        TEST_ASSIGN(filterGroup, ioFilterGroupNew(), "    create new filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, ioTestFilterSizeNew("size")->filter), "    add filter to filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, ioTestFilterSizeNew("size2")->filter), "    add filter to filter group");
        TEST_RESULT_VOID(ioReadFilterGroupSet(ioBufferReadIo(bufferRead), filterGroup), "    add filter group to read io");
        TEST_RESULT_PTR(ioFilterMove(NULL, memContextTop()), NULL, "    move NULL filter to top context");
        TEST_RESULT_PTR(ioFilterGroupResult(filterGroup, strNew("size")), NULL, "    check filter result is NULL");
        TEST_RESULT_PTR(ioFilterGroupResult(filterGroup, strNew("size2")), NULL, "    check filter result is NULL");

        TEST_RESULT_BOOL(ioReadEof(ioBufferReadIo(bufferRead)), false, "    not eof");
        TEST_RESULT_SIZE(ioRead(ioBufferReadIo(bufferRead), buffer), 2, "    read 2 bytes");
        TEST_RESULT_SIZE(ioRead(ioBufferReadIo(bufferRead), buffer), 0, "    read 0 bytes (full buffer)");
        TEST_RESULT_BOOL(memcmp(bufPtr(buffer), "12", 2) == 0, true, "    memcmp");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "12", "    check read");
        TEST_RESULT_SIZE(ioReadSize(ioBufferReadIo(bufferRead)), 2, "    read size is 2");
        TEST_RESULT_BOOL(ioReadEof(ioBufferReadIo(bufferRead)), false, "    not eof");

        TEST_RESULT_VOID(bufUsedZero(buffer), "    zero buffer");
        TEST_RESULT_SIZE(ioRead(ioBufferReadIo(bufferRead), buffer), 1, "    read 1 byte");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "3", "    check read");
        TEST_RESULT_BOOL(ioReadEof(ioBufferReadIo(bufferRead)), true, "    eof");
        TEST_RESULT_BOOL(ioBufferRead(bufferRead, buffer), 0, "    eof from driver");
        TEST_RESULT_SIZE(ioRead(ioBufferReadIo(bufferRead), buffer), 0, "    read 0 bytes");
        TEST_RESULT_SIZE(ioReadSize(ioBufferReadIo(bufferRead)), 3, "    read size is 3");
        TEST_RESULT_VOID(ioReadClose(ioBufferReadIo(bufferRead)), " close buffer read object");

        TEST_RESULT_PTR(ioReadFilterGroup(ioBufferReadIo(bufferRead)), filterGroup, "    check filter group");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, strNew("size"))), 3, "    check filter result");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, strNew("size2"))), 3, "    check filter result");

        TEST_RESULT_VOID(ioBufferReadFree(bufferRead), "    free buffer read object");
        TEST_RESULT_VOID(ioBufferReadFree(NULL), "    free NULL buffer read object");

        TEST_RESULT_VOID(ioFilterGroupFree(filterGroup), "    free filter group object");
        TEST_RESULT_VOID(ioFilterGroupFree(NULL), "    free NULL filter group object");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoWrite and IoBufferWrite"))
    {
        IoWrite *write = NULL;

        TEST_ASSIGN(
            write, ioWriteNew((void *)999, testIoWriteOpen, testIoWriteProcess, testIoWriteClose), "create io write object");

        TEST_RESULT_VOID(ioWriteOpen(write), "    open io object");
        TEST_RESULT_BOOL(testIoWriteOpenCalled, true, "    check io object open");
        TEST_RESULT_VOID(ioWrite(write, bufNewStr(strNew("ABC"))), "    write 3 bytes");
        TEST_RESULT_VOID(ioWriteClose(write), "    close io object");
        TEST_RESULT_BOOL(testIoWriteCloseCalled, true, "    check io object closed");

        // -------------------------------------------------------------------------------------------------------------------------
        IoBufferWrite *bufferWrite = NULL;
        Buffer *buffer = bufNew(0);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(bufferWrite, ioBufferWriteNew(buffer), "create buffer write object");
            TEST_RESULT_VOID(ioBufferWriteMove(bufferWrite, MEM_CONTEXT_OLD()), "    move object to new context");
            TEST_RESULT_VOID(ioBufferWriteMove(NULL, MEM_CONTEXT_OLD()), "    move NULL object to new context");
        }
        MEM_CONTEXT_TEMP_END();

        IoFilterGroup *filterGroup = NULL;
        TEST_ASSIGN(filterGroup, ioFilterGroupNew(), "    create new filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, ioTestFilterSizeNew("size")->filter), "    add filter to filter group");
        TEST_RESULT_VOID(ioWriteFilterGroupSet(ioBufferWriteIo(bufferWrite), filterGroup), "    add filter group to write io");

        TEST_RESULT_VOID(ioWriteOpen(ioBufferWriteIo(bufferWrite)), "    open buffer write object");
        TEST_RESULT_VOID(ioWrite(ioBufferWriteIo(bufferWrite), bufNewStr(strNew("ABC"))), "    write 3 bytes");
        TEST_RESULT_VOID(ioWrite(ioBufferWriteIo(bufferWrite), bufNewStr(strNew(""))), "    write 0 bytes");
        TEST_RESULT_VOID(ioWrite(ioBufferWriteIo(bufferWrite), NULL), "    write 0 bytes");
        TEST_RESULT_SIZE(ioWriteSize(ioBufferWriteIo(bufferWrite)), 3, "    write size is 3");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "ABC", "    check write");

        TEST_RESULT_VOID(ioWrite(ioBufferWriteIo(bufferWrite), bufNewStr(strNew("1234"))), "    write 4 bytes");
        TEST_RESULT_SIZE(ioWriteSize(ioBufferWriteIo(bufferWrite)), 7, "    write size is 7");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "ABC1234", "    check write");

        TEST_RESULT_VOID(ioWriteClose(ioBufferWriteIo(bufferWrite)), " close buffer write object");

        TEST_RESULT_PTR(ioWriteFilterGroup(ioBufferWriteIo(bufferWrite)), filterGroup, "    check filter group");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, strNew("size"))), 7, "    check filter result");

        TEST_RESULT_VOID(ioBufferWriteFree(bufferWrite), "    free buffer write object");
        TEST_RESULT_VOID(ioBufferWriteFree(NULL), "    free NULL buffer write object");
    }

    // *****************************************************************************************************************************
    if (testBegin("ioHandleWriteOneStr()"))
    {
        TEST_ERROR(
            ioHandleWriteOneStr(999999, strNew("test")), FileWriteError,
            "unable to write to 4 byte(s) to handle: [9] Bad file descriptor");

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.txt", testPath());
        int fileHandle = open(strPtr(fileName), O_CREAT | O_TRUNC | O_WRONLY, 0700);

        TEST_RESULT_VOID(ioHandleWriteOneStr(fileHandle, strNew("test1\ntest2")), "write string to file");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
