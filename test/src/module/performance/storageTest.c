/***********************************************************************************************************************************
Storage Performance

Test the performance of various storage functions, in particular when implemented remotely.

Generally speaking, the starting values should be high enough to "blow up" in terms of execution time if there are performance
problems without taking very long if everything is running smoothly. These starting values can then be scaled up for profiling and
stress testing as needed.
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"

#include "common/crypto/hash.h"
#include "common/compress/gz/compress.h"
#include "common/compress/lz4/compress.h"
#include "common/io/filter/filter.intern.h"
#include "common/io/filter/sink.h"
#include "common/io/bufferWrite.h"
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "common/io/io.h"
#include "common/type/object.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "storage/posix/storage.h"
#include "storage/remote/protocol.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Dummy functions and interface for constructing test drivers
***********************************************************************************************************************************/
static StorageInfo
storageTestDummyInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    (void)thisVoid; (void)file; (void)level; (void)param; return (StorageInfo){.exists = false};
}

static bool
storageTestDummyInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    (void)thisVoid; (void)path; (void)level; (void)callback; (void)callbackData; (void)param; return false;
}

static StorageRead *
storageTestDummyNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    (void)thisVoid; (void)file; (void)ignoreMissing; (void)param; return NULL;
}

static StorageWrite *
storageTestDummyNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    (void)thisVoid; (void)file; (void)param; return NULL;
}

static bool
storageTestDummyPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    (void)thisVoid; (void)path; (void)recurse; (void)param; return false;
}

static void
storageTestDummyRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    (void)thisVoid; (void)file; (void)param;
}

static const StorageInterface storageInterfaceTestDummy =
{
    .info = storageTestDummyInfo,
    .infoList = storageTestDummyInfoList,
    .newRead = storageTestDummyNewRead,
    .newWrite = storageTestDummyNewWrite,
    .pathRemove = storageTestDummyPathRemove,
    .remove = storageTestDummyRemove,
};

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
                callback(callbackData, &(StorageInfo){.exists = true});
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
    uint64_t timeRate = this->byteTotal / this->bytesPerSec * MSEC_PER_SEC;

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
        CHECK(testScale() <= 2000);
        uint64_t fileTotal = (uint64_t)1000000 * testScale();

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                // Create a basic configuration so the remote storage driver can determine the storage type
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
                strLstAddZ(argList, "--" CFGOPT_PROCESS "=0");
                strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
                harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleRemote, argList);

                // Create a driver to test remote performance of storageInfoList() and inject it into storageRepo()
                StorageTestPerfInfoList driver =
                {
                    .interface = storageInterfaceTestDummy,
                    .fileTotal = fileTotal,
                };

                driver.interface.infoList = storageTestPerfInfoList;

                storageHelper.storageRepo = storageNew(STRDEF("TEST"), STRDEF("/"), 0, 0, false, NULL, &driver, driver.interface);

                // Setup handler for remote storage protocol
                IoRead *read = ioHandleReadNew(strNew("storage server read"), HARNESS_FORK_CHILD_READ(), 60000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("storage server write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                ProtocolServer *server = protocolServerNew(strNew("storage test server"), strNew("test"), read, write);
                protocolServerHandlerAdd(server, storageRemoteProtocol);
                protocolServerProcess(server);

            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                // Create client
                IoRead *read = ioHandleReadNew(strNew("storage client read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 60000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("storage client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                ProtocolClient *client = protocolClientNew(strNew("storage test client"), strNew("test"), read, write);

                // Create remote storage
                Storage *storageRemote = storageRemoteNew(
                    STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL, client, 1);

                // Storage info list
                TEST_RESULT_VOID(
                    storageInfoListP(storageRemote, NULL, storageTestDummyInfoListCallback, NULL),
                    "list %" PRIu64 " remote files", fileTotal);

                // Free client
                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("benchmark filters"))
    {
        // 4MB buffers are the current default
        ioBufferSizeSet(1024 * 1024);

        // 1MB is a fairly normal table size
        CHECK(testScale() <= 1024 * 1024 * 1024);
        uint64_t blockTotal = (uint64_t)1 * testScale();

        // Set iteration
        unsigned int iteration = 1;

        // Set rate
        uint64_t rateIn = 100000;
        uint64_t rateOut = 100000;

        // Get the sample pages from disk
        Buffer *block = storageGetP(storageNewReadP(storagePosixNewP(STR(testRepoPath())), STRDEF("test/data/filecopy.table.bin")));
        ASSERT(bufUsed(block) == 1024 * 1024);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE_FMT(
            "%u iteration(s) of %" PRIu64 "MiB with %" PRIu64 "MB/s input, %" PRIu64 "MB/s output", iteration, blockTotal, rateIn,
            rateOut);

        #define BENCHMARK_BEGIN()                                                                                                  \
            IoWrite *write = ioBufferWriteNew(bufNew(0));                                                                          \
            ioFilterGroupAdd(ioWriteFilterGroup(write), testIoRateNew(rateIn * 1000 * 1000));

        #define BENCHMARK_FILTER_ADD(filter)                                                                                       \
            ioFilterGroupAdd(ioWriteFilterGroup(write), filter);

        #define BENCHMARK_END(addTo)                                                                                               \
            ioFilterGroupAdd(ioWriteFilterGroup(write), testIoRateNew(rateOut * 1000 * 1000));                                     \
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioSinkNew());                                                              \
            ioWriteOpen(write);                                                                                                    \
                                                                                                                                   \
            uint64_t benchMarkBegin = timeMSec();                                                                                  \
                                                                                                                                   \
            for (uint64_t blockIdx = 0; blockIdx < blockTotal; blockIdx++)                                                         \
                ioWrite(write, block);                                                                                             \
                                                                                                                                   \
            ioWriteClose(write);                                                                                                   \
                                                                                                                                   \
            addTo += timeMSec() - benchMarkBegin;

        // Start totals to 1ms just in case something takes 0ms to run
        uint64_t copyTotal = 1;
        uint64_t md5Total = 1;
        uint64_t sha1Total = 1;
        uint64_t sha256Total = 1;
        uint64_t gzip6Total = 1;
        uint64_t lz41Total = 1;

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
            TEST_LOG_FMT("lz4 -1 iteration %u", idx + 1);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                BENCHMARK_BEGIN();
                BENCHMARK_FILTER_ADD(lz4CompressNew(1));
                BENCHMARK_END(lz41Total);
            }
            MEM_CONTEXT_TEMP_END();
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("results");

        TEST_LOG_FMT("copy average: %" PRIu64 "MiB/s", blockTotal * 1000 / copyTotal / iteration);
        TEST_LOG_FMT("md5 average: %" PRIu64 "MiB/s", blockTotal * 1000 / md5Total / iteration);
        TEST_LOG_FMT("sha1 average: %" PRIu64 "MiB/s", blockTotal * 1000 / sha1Total / iteration);
        TEST_LOG_FMT("sha256 average: %" PRIu64 "MiB/s", blockTotal * 1000 / sha256Total / iteration);
        TEST_LOG_FMT("gzip -6 average: %" PRIu64 "MiB/s", blockTotal * 1000 / gzip6Total / iteration);
        TEST_LOG_FMT("lz4 -1 average: %" PRIu64 "MiB/s", blockTotal * 1000 / lz41Total / iteration);
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
