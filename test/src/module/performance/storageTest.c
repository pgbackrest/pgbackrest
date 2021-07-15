/***********************************************************************************************************************************
Storage Performance

Test the performance of various storage functions, in particular when implemented remotely.

Generally speaking, the starting values should be high enough to "blow up" in terms of execution time if there are performance
problems without taking very long if everything is running smoothly. These starting values can then be scaled up for profiling and
stress testing as needed.
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"

#include "common/crypto/hash.h"
#include "common/compress/gz/compress.h"
#include "common/compress/lz4/compress.h"
#include "common/io/filter/filter.h"
#include "common/io/filter/sink.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/io.h"
#include "common/type/object.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "storage/posix/storage.h"
#include "storage/remote/protocol.h"

/***********************************************************************************************************************************
Dummy callback functions
***********************************************************************************************************************************/
static void
storageTestDummyInfoListCallback(void *data, const StorageInfo *info)
{
    (void)data;
    (void)info;

    // Do some work in the mem context to blow up the total time if this is not efficient
    memResize(memNew(16), 32);
}

/***********************************************************************************************************************************
Driver to test storageInfoList
***********************************************************************************************************************************/
typedef struct
{
    STORAGE_COMMON_MEMBER;
    uint64_t fileTotal;
} StorageTestPerfInfoList;

static bool
storageTestPerfInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageTestPerfInfoList);
    (void)path; (void)level; (void)param;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (uint64_t fileIdx = 0; fileIdx < this->fileTotal; fileIdx++)
            {
                callback(callbackData, &(StorageInfo){.exists = true, .name = STRDEF("name")});
                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_TEMP_END();

    return this->fileTotal != 0;
}

/***********************************************************************************************************************************
Test filter to simulate throughput via rate limiting
***********************************************************************************************************************************/
typedef struct TestIoRate
{
    MemContext *memContext;                                         // Mem context of filter

    uint64_t timeBegin;                                             // Time when filter started processing data in ms
    uint64_t byteTotal;                                             // Total bytes processed
    uint64_t bytesPerSec;                                           // Rate in bytes per second to enforce
} TestIoRate;

static void
testIoRateProcess(THIS_VOID, const Buffer *input)
{
    THIS(TestIoRate);

    // Determine the elapsed time since the filter began processing data. The begin time is not set in the constructor because an
    // unknown amount of time can elapse between the filter being created and acually used.
    uint64_t timeElapsed = 0;

    if (this->timeBegin == 0)
        this->timeBegin = timeMSec();
    else
        timeElapsed = timeMSec() - this->timeBegin;

    // Add buffer used to the byte total
    this->byteTotal += bufUsed(input);

    // Determine how many ms these bytes should take to go through the filter and sleep if greater than elapsed time
    uint64_t timeRate = this->byteTotal * MSEC_PER_SEC / this->bytesPerSec;

    if (timeElapsed < timeRate)
        sleepMSec(timeRate - timeElapsed);
}

static IoFilter *
testIoRateNew(uint64_t bytesPerSec)
{
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TestIoRate")
    {
        TestIoRate *driver = memNew(sizeof(TestIoRate));

        *driver = (TestIoRate)
        {
            .memContext = memContextCurrent(),
            .bytesPerSec = bytesPerSec,
        };

        this = ioFilterNewP(STRDEF("TestIoRate"), driver, NULL, .in = testIoRateProcess);
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
    if (testBegin("storageInfoList()"))
    {
        // One million files represents a fairly large cluster
        CHECK(TEST_SCALE <= 2000);
        uint64_t fileTotal = (uint64_t)1000000 * TEST_SCALE;

        HRN_FORK_BEGIN(.timeout = 60000)
        {
            HRN_FORK_CHILD_BEGIN()
            {
                // Create a basic configuration so the remote storage driver can determine the storage type
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
                strLstAddZ(argList, "--" CFGOPT_PROCESS "=0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote);

                // Create a driver to test remote performance of storageInfoList() and inject it into storageRepo()
                StorageTestPerfInfoList driver =
                {
                    .interface = storageInterfaceTestDummy,
                    .fileTotal = fileTotal,
                };

                driver.interface.infoList = storageTestPerfInfoList;

                storageHelper.storageRepo = memNew(sizeof(Storage *));
                storageHelper.storageRepo[0] = storageNew(
                    strIdFromZ(stringIdBit6, "test"), STRDEF("/"), 0, 0, false, NULL, &driver, driver.interface);

                // Setup handler for remote storage protocol
                ProtocolServer *server = protocolServerNew(
                    STRDEF("storage test server"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());

                static const ProtocolServerHandler commandHandler[] = {PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST};
                protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));

            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Create client
                ProtocolClient *client = protocolClientNew(
                    STRDEF("storage test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0));

                // Create remote storage
                Storage *storageRemote = storageRemoteNew(
                    STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL, client, 1);

                TimeMSec timeBegin = timeMSec();

                // Storage info list
                TEST_RESULT_VOID(
                    storageInfoListP(storageRemote, NULL, storageTestDummyInfoListCallback, NULL), "list remote files");

                TEST_LOG_FMT("list transferred in %ums", (unsigned int)(timeMSec() - timeBegin));

                // Free client
                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("benchmark filters"))
    {
        // 4MB buffers are the current default
        ioBufferSizeSet(4 * 1024 * 1024);

        // 1MB is a fairly normal table size
        CHECK(TEST_SCALE <= 1024 * 1024 * 1024);
        uint64_t blockTotal = (uint64_t)1 * TEST_SCALE;

        // Set iteration
        unsigned int iteration = 1;

        // Set rate
        uint64_t rateIn = 0; // MB/s (0 disables)
        uint64_t rateOut = 0; // MB/s (0 disables)

        // Get the sample pages from disk
        Buffer *block = storageGetP(storageNewReadP(storagePosixNewP(HRN_PATH_REPO_STR), STRDEF("test/data/filecopy.table.bin")));
        ASSERT(bufUsed(block) == 1024 * 1024);

        // Build the input buffer
        Buffer *input = bufNew((size_t)blockTotal * bufSize(block));

        for (unsigned int blockIdx = 0; blockIdx < blockTotal; blockIdx++)
            memcpy(bufPtr(input) + (blockIdx * bufSize(block)), bufPtr(block), bufSize(block));

        bufUsedSet(input, bufSize(input));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE_FMT(
            "%u iteration(s) of %zuMiB with %" PRIu64 "MB/s input, %" PRIu64 "MB/s output", iteration,
            bufUsed(input) / bufUsed(block), rateIn, rateOut);

        #define BENCHMARK_BEGIN()                                                                                                  \
            IoWrite *write = ioBufferWriteNew(bufNew(0));                                                                          \

        #define BENCHMARK_FILTER_ADD(filter)                                                                                       \
            ioFilterGroupAdd(ioWriteFilterGroup(write), filter);

        #define BENCHMARK_END(addTo)                                                                                               \
            if (rateOut != 0)                                                                                                      \
                ioFilterGroupAdd(ioWriteFilterGroup(write), testIoRateNew(rateOut * 1000 * 1000));                                 \
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioSinkNew());                                                              \
            ioWriteOpen(write);                                                                                                    \
                                                                                                                                   \
            IoRead *read = ioBufferReadNew(input);                                                                                 \
            if (rateIn != 0)                                                                                                       \
                ioFilterGroupAdd(ioReadFilterGroup(read), testIoRateNew(rateIn * 1000 * 1000));                                    \
            ioReadOpen(read);                                                                                                      \
                                                                                                                                   \
            uint64_t benchMarkBegin = timeMSec();                                                                                  \
                                                                                                                                   \
            Buffer *buffer = bufNew(ioBufferSize());                                                                               \
                                                                                                                                   \
            do                                                                                                                     \
            {                                                                                                                      \
                ioRead(read, buffer);                                                                                              \
                ioWrite(write, buffer);                                                                                            \
                bufUsedZero(buffer);                                                                                               \
            }                                                                                                                      \
            while (!ioReadEof(read));                                                                                              \
                                                                                                                                   \
            ioReadClose(read);                                                                                                     \
            ioWriteClose(write);                                                                                                   \
                                                                                                                                   \
            addTo += timeMSec() - benchMarkBegin;

        // Start totals to 1ms just in case something takes 0ms to run
        uint64_t copyTotal = 1;
        uint64_t md5Total = 1;
        uint64_t sha1Total = 1;
        uint64_t sha256Total = 1;
        uint64_t gzip6Total = 1;

#ifdef HAVE_LIBLZ4
        uint64_t lz41Total = 1;
#endif // HAVE_LIBLZ4

        for (unsigned int idx = 0; idx < iteration; idx++)
        {
            // -------------------------------------------------------------------------------------------------------------------------
            TEST_LOG_FMT("copy iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_END(copyTotal);
            }
            MEM_CONTEXT_TEMP_END();

            // -------------------------------------------------------------------------------------------------------------------------
            TEST_LOG_FMT("md5 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(cryptoHashNew(HASH_TYPE_MD5_STR));
                BENCHMARK_END(md5Total);
            }
            MEM_CONTEXT_TEMP_END();

            // -------------------------------------------------------------------------------------------------------------------------
            TEST_LOG_FMT("sha1 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(cryptoHashNew(HASH_TYPE_SHA1_STR));
                BENCHMARK_END(sha1Total);
            }
            MEM_CONTEXT_TEMP_END();

            // -------------------------------------------------------------------------------------------------------------------------
            TEST_LOG_FMT("sha256 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(cryptoHashNew(HASH_TYPE_SHA256_STR));
                BENCHMARK_END(sha256Total);
            }
            MEM_CONTEXT_TEMP_END();

            // -------------------------------------------------------------------------------------------------------------------------
            TEST_LOG_FMT("gzip -6 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(gzCompressNew(6));
                BENCHMARK_END(gzip6Total);
            }
            MEM_CONTEXT_TEMP_END();

            // -------------------------------------------------------------------------------------------------------------------------
#ifdef HAVE_LIBLZ4
            TEST_LOG_FMT("lz4 -1 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(lz4CompressNew(1));
                BENCHMARK_END(lz41Total);
            }
            MEM_CONTEXT_TEMP_END();
#endif // HAVE_LIBLZ4
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("results");

        #define TEST_RESULT(name, total)                                                                                           \
            TEST_LOG_FMT(                                                                                                          \
                "%s time %" PRIu64"ms, avg time %" PRIu64"ms, avg throughput: %" PRIu64 "MB/s", name, total, total / iteration,    \
                iteration * blockTotal * 1024 * 1024 * 1000 / total / 1000000);

        TEST_RESULT("copy", copyTotal);
        TEST_RESULT("md5", md5Total);
        TEST_RESULT("sha1", sha1Total);
        TEST_RESULT("sha256", sha256Total);
        TEST_RESULT("gzip -6", gzip6Total);

#ifdef HAVE_LIBLZ4
        TEST_RESULT("lz4 -1", lz41Total);
#endif // HAVE_LIBLZ4
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
