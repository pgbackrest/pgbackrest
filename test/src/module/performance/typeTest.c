/***********************************************************************************************************************************
Test Type Performance

Test the performance of various types and data structures.  Generally speaking, the starting values should be high enough to "blow
up" in terms of execution time if there are performance problems without taking very long if everything is running smoothly.

These starting values can then be scaled up for profiling and stress testing as needed.  In general we hope to scale to 1000 without
running out of memory on the test systems or taking an undue amount of time.  It should be noted that in this context scaling to
1000 is nowhere near turning it up to 11.
***********************************************************************************************************************************/
#include "common/ini.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/time.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "info/manifest.h"
#include "postgres/version.h"

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
testIniLoadCountCallback(void *data, const String *section, const String *key, const String *value)
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
        .user = STRDEF("test"),
        .group = STRDEF("test"),
    };

    if (strEq(file, STRDEF("/pg")))
    {
        result.type = storageTypePath;
    }
    else
        THROW_FMT(AssertError, "unhandled file info '%s'", strPtr(file));

    return result;
}

static bool
storageTestManifestNewBuildInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageTestManifestNewBuild);
    (void)path; (void)level; (void)param;

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        StorageInfo result =
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
            result.name = STRDEF("base");
            callback(callbackData, &result);
        }
        else if (strEq(path, STRDEF("/pg/base")))
        {
            result.name = STRDEF("1000000000");
            callback(callbackData, &result);
        }
        else if (strEq(path, STRDEF("/pg/base/1000000000")))
        {
            result.type = storageTypeFile;
            result.size = 8192;
            result.mode = 0600;
            result.timeModified = 1595627966;

            for (unsigned int fileIdx = 0; fileIdx < this->fileTotal; fileIdx++)
            {
                result.name = strNewFmt("%u", 1000000000 + fileIdx);
                callback(callbackData, &result);
                MEM_CONTEXT_TEMP_RESET(10000);
            }
        }
        else
            THROW_FMT(AssertError, "unhandled file list info '%s'", strPtr(path));
    }
    MEM_CONTEXT_TEMP_END();

    return true;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("lstFind()"))
    {
        CHECK(testScale() <= 10000);
        int testMax = 100000 * (int)testScale();

        // Generate a large list of values (use int instead of string so there fewer allocations)
        List *list = lstNewP(sizeof(int), .comparator = testComparator);

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstAdd(list, &listIdx);

        CHECK(lstSize(list) == (unsigned int)testMax);

        TEST_LOG_FMT("generated %d item list", testMax);

        // Search for all values with an ascending sort
        lstSort(list, sortOrderAsc);

        TimeMSec timeBegin = timeMSec();

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            CHECK(*(int *)lstFind(list, &listIdx) == listIdx);

        TEST_LOG_FMT("asc search completed in %ums", (unsigned int)(timeMSec() - timeBegin));

        // Search for all values with an descending sort
        lstSort(list, sortOrderDesc);

        timeBegin = timeMSec();

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            CHECK(*(int *)lstFind(list, &listIdx) == listIdx);

        TEST_LOG_FMT("desc search completed in %ums", (unsigned int)(timeMSec() - timeBegin));
    }

    // *****************************************************************************************************************************
    if (testBegin("iniLoad()"))
    {
        CHECK(testScale() <= 10000);

        String *iniStr = strNew("[section1]\n");
        unsigned int iniMax = 100000 * (unsigned int)testScale();

        for (unsigned int keyIdx = 0; keyIdx < iniMax; keyIdx++)
            strCatFmt(iniStr, "key%u=\"value%u\"\n", keyIdx, keyIdx);

        TEST_LOG_FMT("ini size = %s, keys = %u", strPtr(strSizeFormat(strSize(iniStr))), iniMax);

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
        CHECK(testScale() <= 1000000);

        // Create a storage driver to test manifest build with an arbitrary number of files
        StorageTestManifestNewBuild driver =
        {
            .interface = storageInterfaceTestDummy,
            .fileTotal = 100000 * (unsigned int)testScale(),
        };

        driver.interface.info = storageTestManifestNewBuildInfo;
        driver.interface.infoList = storageTestManifestNewBuildInfoList;

        Storage *storagePg = storageNew(STRDEF("TEST"), STRDEF("/pg"), 0, 0, false, NULL, &driver, driver.interface);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("build manifest");

        MemContext *testContext = memContextNew("test");
        memContextKeep();
        Manifest *manifest = NULL;
        TimeMSec timeBegin = timeMSec();

        MEM_CONTEXT_BEGIN(testContext)
        {
            TEST_ASSIGN(
                manifest, manifestNewBuild(storagePg, PG_VERSION_91, false, false, NULL, NULL), "build with %" PRIu64 " files",
                driver.fileTotal);
        }
        MEM_CONTEXT_END();

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        TEST_LOG_FMT("memory used %zu", memContextSize(testContext));

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

        testContext = memContextNew("test");
        memContextKeep();
        timeBegin = timeMSec();

        MEM_CONTEXT_BEGIN(testContext)
        {
            manifest = manifestNewLoad(ioBufferReadNew(contentSave));
        }
        MEM_CONTEXT_END();

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        TEST_LOG_FMT("memory used %zu", memContextSize(testContext));

        TEST_RESULT_UINT(manifestFileTotal(manifest), driver.fileTotal, "   check file total");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("find all files");

        timeBegin = timeMSec();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile *file = manifestFile(manifest, fileIdx);
            CHECK(file == manifestFileFind(manifest, file->name));
        }

        TEST_LOG_FMT("completed in %ums", (unsigned int)(timeMSec() - timeBegin));
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
