/***********************************************************************************************************************************
Test Type Performance

Test the performance of various types and data structures.  Generally speaking, the starting values should be high enough to "blow
up" in terms of execution time if there are performance problems without taking very long if everything is running smoothly.

These starting values can then be scaled up for profiling and stress testing as needed.  In general we hope to scale to 1000 without
running out of memory on the test systems or taking an undue amount of time.  It should be noted that in this context scaling to
1000 is nowhere near turning it up to 11.
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/ini.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/socket/client.h"
#include "common/stat.h"
#include "common/time.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "info/manifest.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test sort comparator
***********************************************************************************************************************************/
static int
testComparator(const void *item1, const void *item2)
{
    int int1 = *(int *)item1;
    int int2 = *(int *)item2;

    if (int1 < int2)
        return -1;

    if (int1 > int2)
        return 1;

    return 0;
}

/***********************************************************************************************************************************
Test callback to count ini load results
***********************************************************************************************************************************/
static void
testIniLoadCountCallback(void *const data, const String *const section, const String *const key, const String *const value)
{
    (*(unsigned int *)data)++;
    (void)section;
    (void)key;
    (void)value;
}

/***********************************************************************************************************************************
Driver to test manifestNewBuild(). Generates files for a valid-looking PostgreSQL cluster that can be scaled to any size.
***********************************************************************************************************************************/
typedef struct
{
    STORAGE_COMMON_MEMBER;
    uint64_t fileTotal;
} StorageTestManifestNewBuild;

STRING_STATIC(TEST_MANIFEST_PATH_USER_STR,                          "test");

static StorageInfo
storageTestManifestNewBuildInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    (void)thisVoid; (void)level; (void)param;

    StorageInfo result =
    {
        .level = storageInfoLevelDetail,
        .exists = true,
        .type = storageTypePath,
        .mode = 0600,
        .userId = 100,
        .groupId = 100,
        .user = TEST_MANIFEST_PATH_USER_STR,
        .group = TEST_MANIFEST_PATH_USER_STR,
    };

    if (strEq(file, STRDEF("/pg")))
    {
        result.type = storageTypePath;
    }
    else
        THROW_FMT(AssertError, "unhandled file info '%s'", strZ(file));

    return result;
}

static StorageList *
storageTestManifestNewBuildList(THIS_VOID, const String *path, StorageInfoLevel level, StorageInterfaceListParam param)
{
    THIS(StorageTestManifestNewBuild);
    (void)path; (void)level; (void)param;

    StorageList *const result = storageLstNew(storageInfoLevelDetail);

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        StorageInfo info =
        {
            .level = storageInfoLevelDetail,
            .exists = true,
            .type = storageTypePath,
            .mode = 0700,
            .userId = 100,
            .groupId = 100,
            .user = STRDEF("test"),
            .group = STRDEF("test"),
        };

        if (strEq(path, STRDEF("/pg")))
        {
            info.name = STRDEF("base");
            storageLstAdd(result, &info);
        }
        else if (strEq(path, STRDEF("/pg/base")))
        {
            info.name = STRDEF("1000000000");
            storageLstAdd(result, &info);
        }
        else if (strEq(path, STRDEF("/pg/base/1000000000")))
        {
            info.type = storageTypeFile;
            info.size = 8192;
            info.mode = 0600;
            info.timeModified = 1595627966;

            for (unsigned int fileIdx = 0; fileIdx < this->fileTotal; fileIdx++)
            {
                info.name = strNewFmt("%u", 1000000000 + fileIdx);
                storageLstAdd(result, &info);
                MEM_CONTEXT_TEMP_RESET(10000);
            }
        }
        else
            THROW_FMT(AssertError, "unhandled file list info '%s'", strZ(path));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("lstFind()"))
    {
        ASSERT(TEST_SCALE <= 10000);
        int testMax = 100000 * (int)TEST_SCALE;

        // Generate a large list of values (use int instead of string so there fewer allocations)
        List *list = lstNewP(sizeof(int), .comparator = testComparator);

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstAdd(list, &listIdx);

        ASSERT(lstSize(list) == (unsigned int)testMax);

        TEST_LOG_FMT("generated %d item list", testMax);

        // Search for all values with an ascending sort
        lstSort(list, sortOrderAsc);

        TimeMSec timeBegin = timeMSec();

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            ASSERT(*(int *)lstFind(list, &listIdx) == listIdx);

        TEST_LOG_FMT("asc search completed in %ums", (unsigned int)(timeMSec() - timeBegin));

        // Search for all values with an descending sort
        lstSort(list, sortOrderDesc);

        timeBegin = timeMSec();

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            ASSERT(*(int *)lstFind(list, &listIdx) == listIdx);

        TEST_LOG_FMT("desc search completed in %ums", (unsigned int)(timeMSec() - timeBegin));
    }

    // *****************************************************************************************************************************
    if (testBegin("lstRemoveIdx()"))
    {
        ASSERT(TEST_SCALE <= 10000);
        int testMax = 1000000 * (int)TEST_SCALE;

        // Generate a large list of values (use int instead of string so there fewer allocations)
        List *list = lstNewP(sizeof(int));

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstAdd(list, &listIdx);

        ASSERT(lstSize(list) == (unsigned int)testMax);

        TEST_LOG_FMT("generated %d item list", testMax);

        // Remove all values from index 0
        TimeMSec timeBegin = timeMSec();

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstRemoveIdx(list, 0);

        TEST_LOG_FMT("remove completed in %ums", (unsigned int)(timeMSec() - timeBegin));

        ASSERT(lstEmpty(list));
    }

    // *****************************************************************************************************************************
    if (testBegin("iniLoad()"))
    {
        ASSERT(TEST_SCALE <= 10000);

        String *iniStr = strCatZ(strNew(), "[section1]\n");
        unsigned int iniMax = 100000 * (unsigned int)TEST_SCALE;

        for (unsigned int keyIdx = 0; keyIdx < iniMax; keyIdx++)
            strCatFmt(iniStr, "key%u=\"value%u\"\n", keyIdx, keyIdx);

        TEST_LOG_FMT("ini size = %s, keys = %u", strZ(strSizeFormat(strSize(iniStr))), iniMax);

        TimeMSec timeBegin = timeMSec();
        unsigned int iniTotal = 0;

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(BUFSTR(iniStr)), testIniLoadCountCallback, &iniTotal), "parse ini");
        TEST_LOG_FMT("parse completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        TEST_RESULT_INT(iniTotal, iniMax, "    check ini total");
    }

    // Build/load/save a larger manifest to test performance and memory usage. The default sizing is for a "typical" large cluster
    // but this can be scaled to test larger cluster sizes.
    // *****************************************************************************************************************************
    if (testBegin("manifestNewBuild()/manifestNewLoad()/manifestSave()"))
    {
        ASSERT(TEST_SCALE <= 1000000);

        // Create a storage driver to test manifest build with an arbitrary number of files
        StorageTestManifestNewBuild driver =
        {
            .interface = storageInterfaceTestDummy,
            .fileTotal = 100000 * (unsigned int)TEST_SCALE,
        };

        driver.interface.info = storageTestManifestNewBuildInfo;
        driver.interface.list = storageTestManifestNewBuildList;

        const Storage *const storagePg = storageNew(
            strIdFromZ("test"), STRDEF("/pg"), 0, 0, false, NULL, &driver, driver.interface);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("build manifest");

        MemContext *testContext = memContextNewP("test", .childQty = MEM_CONTEXT_QTY_MAX);
        memContextKeep();
        Manifest *manifest = NULL;
        TimeMSec timeBegin = timeMSec();

        MEM_CONTEXT_BEGIN(testContext)
        {
            TEST_ASSIGN(
                manifest, manifestNewBuild(storagePg, PG_VERSION_91, 999999999, false, false, false, NULL, NULL), "build files");
        }
        MEM_CONTEXT_END();

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        // TEST_LOG_FMT("memory used %zu", memContextSize(testContext));

        TEST_RESULT_UINT(manifestFileTotal(manifest), driver.fileTotal, "   check file total");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("save manifest");

        Buffer *contentSave = bufNew(0);
        timeBegin = timeMSec();

        manifestSave(manifest, ioBufferWriteNew(contentSave));

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));

        memContextFree(testContext);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("load manifest");

        testContext = memContextNewP("test", .childQty = MEM_CONTEXT_QTY_MAX);
        memContextKeep();
        timeBegin = timeMSec();

        MEM_CONTEXT_BEGIN(testContext)
        {
            manifest = manifestNewLoad(ioBufferReadNew(contentSave));
        }
        MEM_CONTEXT_END();

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        // TEST_LOG_FMT("memory used %zu", memContextSize(testContext));

        TEST_RESULT_UINT(manifestFileTotal(manifest), driver.fileTotal, "   check file total");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("find all files");

        timeBegin = timeMSec();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile file = manifestFile(manifest, fileIdx);
            ASSERT(strEq(file.name, manifestFileFind(manifest, file.name).name));
        }

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
    }

    // Make sure statistics collector performs well
    // *****************************************************************************************************************************
    if (testBegin("statistics collector"))
    {
        ASSERT(TEST_SCALE <= 1000000);

        // Setup a list of stats to use for testing
        #define TEST_STAT_TOTAL 100
        String *statList[TEST_STAT_TOTAL];

        for (unsigned int statIdx = 0; statIdx < TEST_STAT_TOTAL; statIdx++)
            statList[statIdx] = strNewFmt("STAT%u", statIdx);

        uint64_t runTotal = (uint64_t)TEST_SCALE * (uint64_t)100000;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE_FMT("update %d stats %" PRIu64 " times", TEST_STAT_TOTAL, runTotal);

        TimeMSec timeBegin = timeMSec();

        for (uint64_t runIdx = 0; runIdx < runTotal; runIdx++)
        {
            for (unsigned int statIdx = 0; statIdx < TEST_STAT_TOTAL; statIdx++)
                statInc(statList[statIdx]);
        }

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
    }

    // *****************************************************************************************************************************
    if (testBegin("SocketClient"))
    {
        // This test must be done here because the problem with variables being clobbered after a long jump is only present in
        // optimized builds, so the unit test will not notice if the volatile keyword goes missing in sckClientOpen(). Since the
        // performance tests are built with optimization is it more likely to be caught here.
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create socket with error to check for leaks");

        const Storage *const storageFd = storagePosixNewP(strNewFmt("/proc/%d/fd", getpid()));
        unsigned int fdBefore = strLstSize(storageListP(storageFd, NULL));

        TEST_ERROR(
            ioClientOpen(sckClientNew(STRDEF("172.31.255.255"), 7777, 0, 0)), HostConnectError,
            "timeout connecting to '172.31.255.255:7777'");

        TEST_RESULT_UINT(strLstSize(storageListP(storageFd, NULL)), fdBefore, "socket was freed");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
