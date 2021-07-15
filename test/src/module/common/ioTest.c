/***********************************************************************************************************************************
Test IO
***********************************************************************************************************************************/
#include <fcntl.h>
#include <netdb.h>

#include "common/type/json.h"

#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test functions for IoRead that are not covered by testing the IoBufferRead object
***********************************************************************************************************************************/
static bool
testIoReadOpen(void *driver)
{
    if (driver == (void *)998)
        return false;

    return true;
}

static size_t
testIoRead(void *driver, Buffer *buffer, bool block)
{
    ASSERT(driver == (void *)999);
    (void)block;

    bufCat(buffer, BUFSTRDEF("Z"));

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
testIoWrite(void *driver, const Buffer *buffer)
{
    ASSERT(driver == (void *)999);
    ASSERT(strEq(strNewBuf(buffer), STRDEF("ABC")));
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
} IoTestFilterSize;

static void
ioTestFilterSizeProcess(THIS_VOID, const Buffer *buffer)
{
    THIS(IoTestFilterSize);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(SIZE, this->size);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    this->size += bufUsed(buffer);

    FUNCTION_LOG_RETURN_VOID();
}

static Variant *
ioTestFilterSizeResult(THIS_VOID)
{
    THIS(IoTestFilterSize);

    return varNewUInt64(this->size);
}

static IoFilter *
ioTestFilterSizeNew(const char *type)
{
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoTestFilterSize")
    {
        IoTestFilterSize *driver = memNew(sizeof(IoTestFilterSize));

        *driver = (IoTestFilterSize)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        this = ioFilterNewP(strNewZ(type), driver, NULL, .in = ioTestFilterSizeProcess, .result = ioTestFilterSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Test filter to multiply input to the output.  It can also flush out a variable number of bytes at the end.
***********************************************************************************************************************************/
typedef struct IoTestFilterMultiply
{
    MemContext *memContext;
    unsigned int flushTotal;
    bool writeZero;
    char flushChar;
    Buffer *multiplyBuffer;
    unsigned int multiplier;
    IoFilter *bufferFilter;
} IoTestFilterMultiply;

static void
ioTestFilterMultiplyProcess(THIS_VOID, const Buffer *input, Buffer *output)
{
    THIS(IoTestFilterMultiply);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL && bufRemains(output) > 0);

    if (input == NULL)
    {
        // Write nothing into the output buffer to make sure the filter processing will skip the remaining filters
        if (!this->writeZero)
        {
            this->writeZero = true;
        }
        else
        {
            char flushZ[] = {this->flushChar, 0};
            bufCat(output, bufNewC(flushZ, 1));
            this->flushTotal--;
        }
    }
    else
    {
        if (this->multiplyBuffer == NULL)
        {
            this->multiplyBuffer = bufNew(bufUsed(input) * this->multiplier);
            const unsigned char *inputPtr = bufPtrConst(input);
            unsigned char *bufferPtr = bufPtr(this->multiplyBuffer);

            for (unsigned int charIdx = 0; charIdx < bufUsed(input); charIdx++)
            {
                for (unsigned int multiplierIdx = 0; multiplierIdx < this->multiplier; multiplierIdx++)
                    bufferPtr[charIdx * this->multiplier + multiplierIdx] = inputPtr[charIdx];
            }

            bufUsedSet(this->multiplyBuffer, bufSize(this->multiplyBuffer));
        }

        ioFilterProcessInOut(this->bufferFilter, this->multiplyBuffer, output);

        if (!ioFilterInputSame(this->bufferFilter))
            this->multiplyBuffer = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

static bool
ioTestFilterMultiplyDone(const THIS_VOID)
{
    THIS(const IoTestFilterMultiply);

    return this->flushTotal == 0;
}

static bool
ioTestFilterMultiplyInputSame(const THIS_VOID)
{
    THIS(const IoTestFilterMultiply);

    return ioFilterInputSame(this->bufferFilter);
}

static IoFilter *
ioTestFilterMultiplyNew(const char *type, unsigned int multiplier, unsigned int flushTotal, char flushChar)
{
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoTestFilterMultiply")
    {
        IoTestFilterMultiply *driver = memNew(sizeof(IoTestFilterMultiply));

        *driver = (IoTestFilterMultiply)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .bufferFilter = ioBufferNew(),
            .multiplier = multiplier,
            .flushTotal = flushTotal,
            .flushChar = flushChar,
        };

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(type));
        varLstAdd(paramList, varNewUInt(multiplier));
        varLstAdd(paramList, varNewUInt(flushTotal));

        this = ioFilterNewP(
            strNewZ(type), driver, paramList, .done = ioTestFilterMultiplyDone, .inOut = ioTestFilterMultiplyProcess,
            .inputSame = ioTestFilterMultiplyInputSame);
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
    if (testBegin("ioBufferSize()/ioBufferSizeSet() and ioTimeoutMs()/ioTimeoutMsSet()"))
    {
        TEST_RESULT_UINT(ioBufferSize(), 65536, "check initial buffer size");
        TEST_RESULT_VOID(ioBufferSizeSet(16384), "set buffer size");
        TEST_RESULT_UINT(ioBufferSize(), 16384, "check buffer size");

        TEST_RESULT_UINT(ioTimeoutMs(), 60000, "check initial timeout ms");
        TEST_RESULT_VOID(ioTimeoutMsSet(77777), "set timeout ms");
        TEST_RESULT_UINT(ioTimeoutMs(), 77777, "check timeout ms");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoRead, IoBufferRead, IoBuffer, IoSize, IoFilter, IoFilterGroup, and ioReadBuf()"))
    {
        IoRead *read = NULL;
        Buffer *buffer = bufNew(2);
        ioBufferSizeSet(2);

        TEST_ASSIGN(
            read, ioReadNewP((void *)998, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");

        TEST_RESULT_BOOL(ioReadOpen(read), false, "    open io object");

        TEST_ASSIGN(
            read, ioReadNewP((void *)999, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");

        TEST_RESULT_BOOL(ioReadOpen(read), true, "    open io object");
        TEST_RESULT_BOOL(ioReadReadyP(read), true, "read defaults to ready");
        TEST_RESULT_UINT(ioRead(read, buffer), 2, "    read 2 bytes");
        TEST_RESULT_BOOL(ioReadEof(read), false, "    no eof");
        TEST_RESULT_VOID(ioReadClose(read), "    close io object");
        TEST_RESULT_BOOL(testIoReadCloseCalled, true, "    check io object closed");

        TEST_RESULT_VOID(ioReadFree(read), "    free read object");

        // Read a zero-length buffer to be sure it is not passed on to the filter group
        // -------------------------------------------------------------------------------------------------------------------------
        IoRead *bufferRead = NULL;
        ioBufferSizeSet(2);
        buffer = bufNew(2);
        Buffer *bufferOriginal = bufNew(0);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create empty buffer read object");
        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 0, "    read 0 bytes");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), true, "    now eof");

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(2);
        buffer = bufNew(2);
        bufferOriginal = bufNewC("123", 3);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create buffer read object");

        TEST_RESULT_VOID(ioFilterGroupClear(ioReadFilterGroup(bufferRead)), "    clear does nothing when no filters");
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew()), "    add filter to be cleared");
        TEST_RESULT_VOID(ioFilterGroupClear(ioReadFilterGroup(bufferRead)), "    clear size filter");

        IoFilter *sizeFilter = ioSizeNew();
        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioTestFilterMultiplyNew("double", 2, 3, 'X')),
            "    add filter to filter group");
        TEST_RESULT_PTR(
            ioFilterGroupInsert(ioReadFilterGroup(bufferRead), 0, sizeFilter), bufferRead->pub.filterGroup,
            "    add filter to filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew()), "    add filter to filter group");
        IoFilter *bufferFilter = ioBufferNew();
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), bufferFilter), "    add filter to filter group");
        TEST_RESULT_PTR(ioFilterMove(NULL, memContextTop()), NULL, "    move NULL filter to top context");
        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupParamAll(ioReadFilterGroup(bufferRead))),
            "[{\"size\":null},{\"double\":[\"double\",2,3]},{\"size\":null},{\"buffer\":null}]", "    check filter params");

        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open");
        TEST_RESULT_INT(ioReadFd(bufferRead), -1, "    fd invalid");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 0, "    read 0 bytes (full buffer)");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "11", "    check read");
        TEST_RESULT_STR_Z(ioFilterType(sizeFilter), "size", "check filter type");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");

        TEST_RESULT_VOID(bufUsedZero(buffer), "    zero buffer");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "22", "    check read");

        TEST_ASSIGN(buffer, bufNew(3), "change output buffer size to 3");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 3, "    read 3 bytes");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "33X", "    check read");

        TEST_RESULT_VOID(bufUsedZero(buffer), "    zero buffer");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "XX", "    check read");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), true, "    eof");
        TEST_RESULT_UINT(ioBufferRead(ioReadDriver(bufferRead), buffer, true), 0, "    eof from driver");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 0, "    read 0 bytes");
        TEST_RESULT_VOID(ioReadClose(bufferRead), " close buffer read object");
        TEST_RESULT_STR_Z(
            jsonFromVar(ioFilterGroupResultAll(ioReadFilterGroup(bufferRead))),
            "{\"buffer\":null,\"double\":null,\"size\":[3,9]}",
            "    check filter result all");

        TEST_RESULT_PTR(ioReadFilterGroup(bufferRead), ioReadFilterGroup(bufferRead), "    check filter group");
        TEST_RESULT_UINT(
            varUInt64(varLstGet(varVarLst(ioFilterGroupResult(ioReadFilterGroup(bufferRead), ioFilterType(sizeFilter))), 0)), 3,
            "    check filter result");
        TEST_RESULT_PTR(
            ioFilterGroupResult(ioReadFilterGroup(bufferRead), STRDEF("double")), NULL, "    check filter result is NULL");
        TEST_RESULT_UINT(
            varUInt64(varLstGet(varVarLst(ioFilterGroupResult(ioReadFilterGroup(bufferRead), ioFilterType(sizeFilter))), 1)), 9,
            "    check filter result");

        TEST_RESULT_PTR(ioFilterDriver(bufferFilter), bufferFilter->pub.driver, "    check filter driver");
        TEST_RESULT_PTR(ioFilterInterface(bufferFilter), &bufferFilter->pub.interface, "    check filter interface");

        TEST_RESULT_VOID(ioFilterFree(bufferFilter), "    free buffer filter");
        TEST_RESULT_VOID(ioFilterGroupFree(ioReadFilterGroup(bufferRead)), "    free filter group object");

        // Set filter group results
        // -------------------------------------------------------------------------------------------------------------------------
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        filterGroup->pub.opened = true;
        TEST_RESULT_VOID(ioFilterGroupResultAllSet(filterGroup, NULL), "null result");
        TEST_RESULT_VOID(ioFilterGroupResultAllSet(filterGroup, jsonToVar(STRDEF("{\"test\":777}"))), "add result");
        filterGroup->pub.closed = true;
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, STRDEF("test"))), 777, "    check filter result");

        // Read a zero-size buffer to ensure filters are still processed even when there is no input.  Some filters (e.g. encryption
        // and compression) will produce output even if there is no input.
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(1024);
        buffer = bufNew(1024);
        bufferOriginal = bufNew(0);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create buffer read object");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioTestFilterMultiplyNew("double", 2, 5, 'Y')),
            "    add filter that produces output with no input");
        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open read");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 5, "    read 5 chars");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "YYYYY", "    check buffer");

        // Mixed line and buffer read
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(5);
        read = ioBufferReadNew(BUFSTRDEF("AAAAAA123\n1234\n\n12\nBDDDEFF"));
        ioReadOpen(read);
        buffer = bufNew(6);

        // Start with a small read
        TEST_RESULT_UINT(ioReadSmall(read, buffer), 6, "read buffer");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AAAAAA", "    check buffer");
        bufUsedSet(buffer, 3);
        bufLimitSet(buffer, 3);

        // Do line reads of various lengths
        TEST_RESULT_STR_Z(ioReadLine(read), "123", "read line");
        TEST_RESULT_STR_Z(ioReadLine(read), "1234", "read line");
        TEST_RESULT_STR_Z(ioReadLine(read), "", "read line");
        TEST_RESULT_STR_Z(ioReadLine(read), "12", "read line");

        // Read what was left in the line buffer
        TEST_RESULT_UINT(ioRead(read, buffer), 0, "read buffer");
        bufUsedSet(buffer, 2);
        TEST_RESULT_UINT(ioReadSmall(read, buffer), 1, "read buffer");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AAB", "    check buffer");
        bufUsedSet(buffer, 0);

        // Now do a full buffer read from the input
        TEST_RESULT_UINT(ioReadSmall(read, buffer), 3, "read buffer");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "DDD", "    check buffer");

        // Read line doesn't work without a linefeed
        TEST_ERROR(ioReadLine(read), FileReadError, "unexpected eof while reading line");

        // But those bytes can be picked up by a buffer read
        buffer = bufNew(2);
        TEST_RESULT_UINT(ioRead(read, buffer), 2, "read buffer");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "EF", "    check buffer");

        buffer = bufNew(1);
        TEST_RESULT_UINT(ioRead(read, buffer), 1, "read buffer");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "F", "    check buffer");

        // Nothing left to read
        TEST_ERROR(ioReadLine(read), FileReadError, "unexpected eof while reading line");
        TEST_RESULT_UINT(ioRead(read, buffer), 0, "read buffer");

        // Error if buffer is full and there is no linefeed
        ioBufferSizeSet(10);
        read = ioBufferReadNew(BUFSTRDEF("0123456789"));
        ioReadOpen(read);
        TEST_ERROR(ioReadLine(read), FileReadError, "unable to find line in 10 byte buffer");

        // Read line without eof
        read = ioBufferReadNew(BUFSTRDEF("1234"));
        ioReadOpen(read);
        TEST_RESULT_STR_Z(ioReadLineParam(read, true), "1234", "read line without eof");

        // Read IO into a buffer
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(8);

        bufferRead = ioBufferReadNew(BUFSTRDEF("a test string"));
        ioReadOpen(bufferRead);

        TEST_RESULT_STR_Z(strNewBuf(ioReadBuf(bufferRead)), "a test string", "read into buffer");

        // Drain read IO
        // -------------------------------------------------------------------------------------------------------------------------
        bufferRead = ioBufferReadNew(BUFSTRDEF("a better test string"));
        ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew());

        TEST_RESULT_BOOL(ioReadDrain(bufferRead), true, "drain read io");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(ioReadFilterGroup(bufferRead), SIZE_FILTER_TYPE_STR)), 20, "check length");

        // Cannot open file
        TEST_ASSIGN(
            read, ioReadNewP((void *)998, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");
        TEST_RESULT_BOOL(ioReadDrain(read), false, "cannot open");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoWrite, IoBufferWrite, IoBuffer, IoSize, IoFilter, and IoFilterGroup"))
    {
        IoWrite *write = NULL;
        ioBufferSizeSet(3);

        TEST_ASSIGN(
            write, ioWriteNewP((void *)999, .close = testIoWriteClose, .open = testIoWriteOpen, .write = testIoWrite),
            "create io write object");

        TEST_RESULT_VOID(ioWriteOpen(write), "    open io object");
        TEST_RESULT_BOOL(ioWriteReadyP(write), true, "write defaults to ready");
        TEST_RESULT_BOOL(testIoWriteOpenCalled, true, "    check io object open");
        TEST_RESULT_VOID(ioWriteStr(write, STRDEF("ABC")), "    write 3 bytes");
        TEST_RESULT_VOID(ioWriteClose(write), "    close io object");
        TEST_RESULT_BOOL(testIoWriteCloseCalled, true, "    check io object closed");

        TEST_RESULT_VOID(ioWriteFree(write), "    free write object");

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(3);
        IoWrite *bufferWrite = NULL;
        Buffer *buffer = bufNew(0);

        TEST_ASSIGN(bufferWrite, ioBufferWriteNew(buffer), "create buffer write object");
        IoFilterGroup *filterGroup = ioWriteFilterGroup(bufferWrite);
        IoFilter *sizeFilter = ioSizeNew();
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, sizeFilter), "    add filter to filter group");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(filterGroup, ioTestFilterMultiplyNew("double", 2, 3, 'X')), "    add filter to filter group");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(filterGroup, ioTestFilterMultiplyNew("single", 1, 1, 'Y')),
            "    add filter to filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, ioTestFilterSizeNew("size2")), "    add filter to filter group");

        TEST_RESULT_VOID(ioWriteOpen(bufferWrite), "    open buffer write object");
        TEST_RESULT_INT(ioWriteFd(bufferWrite), -1, "    fd invalid");
        TEST_RESULT_VOID(ioWriteLine(bufferWrite, BUFSTRDEF("AB")), "    write line");
        TEST_RESULT_VOID(ioWrite(bufferWrite, bufNew(0)), "    write 0 bytes");
        TEST_RESULT_VOID(ioWrite(bufferWrite, NULL), "    write 0 bytes");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AABB\n\n", "    check write");

        TEST_RESULT_VOID(ioWriteStr(bufferWrite, STRDEF("Z")), "    write string");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AABB\n\n", "    no change because output buffer is not full");
        TEST_RESULT_VOID(ioWriteStr(bufferWrite, STRDEF("12345")), "    write bytes");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AABB\n\nZZ1122334455", "    check write");

        TEST_RESULT_VOID(ioWriteClose(bufferWrite), " close buffer write object");
        TEST_RESULT_STR_Z(strNewBuf(buffer), "AABB\n\nZZ1122334455XXXY", "    check write after close");

        TEST_RESULT_PTR(ioWriteFilterGroup(bufferWrite), filterGroup, "    check filter group");
        TEST_RESULT_UINT(
            varUInt64(ioFilterGroupResult(filterGroup, ioFilterType(sizeFilter))), 9, "    check filter result");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, STRDEF("size2"))), 22, "    check filter result");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoFdRead, IoFdWrite, and ioFdWriteOneStr()"))
    {
        ioBufferSizeSet(16);

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                IoWrite *write = NULL;

                TEST_ASSIGN(write, ioFdWriteNewOpen(STRDEF("write test"), HRN_FORK_CHILD_WRITE_FD(), 1000), "move write");
                TEST_RESULT_BOOL(ioWriteReadyP(write), true, "write is ready");
                TEST_RESULT_INT(ioWriteFd(write), ((IoFdWrite *)write->driver)->fd, "check write fd");

                // Write a line to be read
                TEST_RESULT_VOID(ioWriteStrLine(write, STRDEF("test string 1")), "write test string");
                ioWriteFlush(write);
                ioWriteFlush(write);

                // Sleep so the other side will timeout
                const Buffer *buffer = BUFSTRDEF("12345678");
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                sleepMSec(1250);

                // Write a buffer in two parts and sleep in the middle so it will be read on the other side in two parts
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                sleepMSec(500);
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                ioWriteFlush(write);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNewOpen(STRDEF("read test"), HRN_FORK_PARENT_READ_FD(0), 1000);

                TEST_RESULT_INT(ioReadFd(read), ((IoFdRead *)ioReadDriver(read))->fd, "check fd");
                TEST_RESULT_PTR(ioReadInterface(read), &read->pub.interface, "check interface");
                TEST_RESULT_PTR(ioReadDriver(read), read->pub.driver, "check driver");

                // Read a string
                TEST_RESULT_STR_Z(ioReadLine(read), "test string 1", "read test string");

                // Only part of the buffer is written before timeout
                Buffer *buffer = bufNew(16);

                ((IoFdRead *)read->pub.driver)->timeout = 1;
                TEST_RESULT_BOOL(ioReadReadyP(read), false, "read is not ready (without throwing error)");
                ((IoFdRead *)read->pub.driver)->timeout = 1000;

                TEST_ERROR(ioRead(read, buffer), FileReadError, "timeout after 1000ms waiting for read from 'read test'");
                TEST_RESULT_UINT(bufSize(buffer), 16, "buffer is only partially read");

                // Read a buffer that is transmitted in two parts with blocking on the read side
                buffer = bufNew(16);
                bufLimitSet(buffer, 12);

                TEST_RESULT_UINT(ioRead(read, buffer), 12, "read buffer");
                bufLimitClear(buffer);
                TEST_RESULT_UINT(ioRead(read, buffer), 4, "read buffer");
                TEST_RESULT_STR_Z(strNewBuf(buffer), "1234567812345678", "check buffer");

                // Check EOF
                buffer = bufNew(16);

                TEST_RESULT_UINT(ioFdRead(ioReadDriver(read), buffer, true), 0, "read buffer at eof");
                TEST_RESULT_UINT(ioFdRead(ioReadDriver(read), buffer, true), 0, "read buffer at eof again");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(ioFdWriteOneStr(999999, STRDEF("test")), FileWriteError, "unable to write to fd: [9] Bad file descriptor");

        // -------------------------------------------------------------------------------------------------------------------------
        int fd = open(TEST_PATH "/test.txt", O_CREAT | O_TRUNC | O_WRONLY, 0700);

        TEST_RESULT_VOID(ioFdWriteOneStr(fd, STRDEF("test1\ntest2")), "write string to file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fdReadyRetry() edge conditions");

        TimeMSec timeout = 5757;
        TEST_RESULT_BOOL(fdReadyRetry(-1, EINTR, true, &timeout, 0), true, "first retry does not modify timeout");
        TEST_RESULT_UINT(timeout, 5757, "    check timeout");

        timeout = 0;
        TEST_RESULT_BOOL(fdReadyRetry(-1, EINTR, false, &timeout, timeMSec() + 10000), true, "retry before timeout");
        TEST_RESULT_BOOL(timeout > 0, true, "    check timeout");

        TEST_RESULT_BOOL(fdReadyRetry(-1, EINTR, false, &timeout, timeMSec()), false, "no retry after timeout");
        TEST_ERROR(fdReadyRetry(-1, EINVAL, true, &timeout, 0), KernelError, "unable to poll socket: [22] Invalid argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write is not ready on bad socket connection");

        struct addrinfo hints = (struct addrinfo)
        {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
        };

        int result;
        const char *hostBad = "172.31.255.255";
        struct addrinfo *hostBadAddress;

        if ((result = getaddrinfo(hostBad, "7777", &hints, &hostBadAddress)) != 0)
        {
            THROW_FMT(                                              // {uncoverable - lookup on IP should never fail}
                HostConnectError, "unable to get address for '%s': [%d] %s", hostBad, result, gai_strerror(result));
        }

        TRY_BEGIN()
        {
            int fd = socket(hostBadAddress->ai_family, hostBadAddress->ai_socktype, hostBadAddress->ai_protocol);
            THROW_ON_SYS_ERROR(fd == -1, HostConnectError, "unable to create socket");

            // Set socket non-blocking
            int flags;
            THROW_ON_SYS_ERROR((flags = fcntl(fd, F_GETFL)) == -1, ProtocolError, "unable to get flags");
            THROW_ON_SYS_ERROR(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1, ProtocolError, "unable to set O_NONBLOCK");

            // Attempt connection
            CHECK(connect(fd, hostBadAddress->ai_addr, hostBadAddress->ai_addrlen) == -1);

            // Create file descriptor write and wait for timeout
            IoWrite *write = NULL;
            TEST_ASSIGN(write, ioFdWriteNew(STR(hostBad), fd, 100), "new fd write");

            TEST_RESULT_BOOL(ioWriteReadyP(write), false, "write is not ready");
            TEST_ERROR(
                ioWriteReadyP(write, .error = true), FileWriteError, "timeout after 100ms waiting for write to '172.31.255.255'");
        }
        FINALLY()
        {
            // This needs to be freed or valgrind will complain
            freeaddrinfo(hostBadAddress);
        }
        TRY_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
