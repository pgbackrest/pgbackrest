/***********************************************************************************************************************************
Test Build Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/render.h"
#include "command/test/build.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/user.h"
#include "storage/posix/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct TestBuild
{
    TestBuildPub pub;                                               // Publicly accessible variables
};

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define MESON_COMMENT_BLOCK                                                                                                        \
    "############################################################################################################################" \
    "########"

/**********************************************************************************************************************************/
TestBuild *
testBldNew(
    const String *const pathRepo, const String *const pathTest, const String *const vm, const unsigned int vmId,
    const String *const moduleName, const unsigned int test, const uint64_t scale, const LogLevel logLevel, const bool logTime,
    const String *const timeZone)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(UINT, vmId);
        FUNCTION_LOG_PARAM(STRING, moduleName);
        FUNCTION_LOG_PARAM(UINT, test);
        FUNCTION_LOG_PARAM(UINT64, scale);
        FUNCTION_LOG_PARAM(ENUM, logLevel);
        FUNCTION_LOG_PARAM(BOOL, logTime);
        FUNCTION_LOG_PARAM(STRING, timeZone);
    FUNCTION_LOG_END();

    ASSERT(pathRepo != NULL);
    ASSERT(pathTest != NULL);
    ASSERT(vm != NULL);
    ASSERT(moduleName != NULL);
    ASSERT(scale != 0);

    TestBuild *this = NULL;

    OBJ_NEW_BEGIN(TestBuild, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (TestBuild)
        {
            .pub =
            {
                .pathRepo = strDup(pathRepo),
                .pathTest = strDup(pathTest),
                .vm = strDup(vm),
                .vmId = vmId,
                .moduleName = strDup(moduleName),
                .test = test,
                .scale = scale,
                .logLevel = logLevel,
                .logTime = logTime,
                .timeZone = strDup(timeZone),
            },
        };

        this->pub.storageRepo = storagePosixNewP(testBldPathRepo(this));
        this->pub.storageTest = storagePosixNewP(testBldPathTest(this));

        // Find the module to test
        this->pub.module = lstFind(testDefParse(testBldStorageRepo(this)).moduleList, &this->pub.moduleName);
        CHECK(AssertError, this->pub.module != NULL, "unable to find module");
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(TEST_BUILD, this);
}

/***********************************************************************************************************************************
Write files into the test path and keep a list of files written
***********************************************************************************************************************************/
static void
testBldWrite(const Storage *const storage, StringList *const fileList, const char *const file, const Buffer *const content)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING_LIST, fileList);
        FUNCTION_LOG_PARAM(STRINGZ, file);
        FUNCTION_LOG_PARAM(BUFFER, content);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Only write if the content has changed
        const Buffer *const current = storageGetP(storageNewReadP(storage, STR(file), .ignoreMissing = true));

        if (current == NULL || !bufEq(content, current))
            storagePutP(storageNewWriteP(storage, STR(file)), content);

        strLstAddZ(fileList, file);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
testBldUnit(TestBuild *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TEST_BUILD, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    userInit();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *const storageUnit = storagePosixNewP(
            strNewFmt("%s/unit-%u", strZ(testBldPathTest(this)), testBldVmId(this)), .write = true);
        const Storage *const storageTestId = storagePosixNewP(
            strNewFmt("%s/test-%u", strZ(testBldPathTest(this)), testBldVmId(this)), .write = true);
        StringList *const storageUnitList = strLstNew();

        // Copy meson_options.txt
        // -----------------------------------------------------------------------------------------------------------------------------
        testBldWrite(
            storageUnit, storageUnitList, "meson_options.txt",
            storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("meson_options.txt"))));

        // Build meson.build
        // -----------------------------------------------------------------------------------------------------------------------------
        String *const mesonBuild = strCatBuf(
            strNew(), storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("meson.build"))));

        // Comment out subdirs that are not used for testing
        strReplace(mesonBuild, STRDEF("subdir('"), STRDEF("# subdir('"));

        // Write build.auto.in
        strCatZ(
            mesonBuild,
            "\n"
            MESON_COMMENT_BLOCK "\n"
            "# Write configuration\n"
            MESON_COMMENT_BLOCK "\n"
            "configure_file(output: 'build.auto.h', configuration: configuration)\n"
            "\n"
            "add_global_arguments('-DERROR_MESSAGE_BUFFER_SIZE=131072', language : 'c')\n");

        // Configure features
        if (testBldModule(this)->feature != NULL)
            strCatFmt(mesonBuild, "add_global_arguments('-DHRN_INTEST_%s', language : 'c')\n", strZ(testBldModule(this)->feature));

        if (testBldModule(this)->featureList != NULL)
        {
            for (unsigned int featureIdx = 0; featureIdx < strLstSize(testBldModule(this)->featureList); featureIdx++)
            {
                strCatFmt(
                    mesonBuild, "add_global_arguments('-DHRN_FEATURE_%s', language : 'c')\n",
                    strZ(strLstGet(testBldModule(this)->featureList, featureIdx)));
            }
        }

        // Add compiler flags
        if (testBldModule(this)->flag != NULL)
            strCatFmt(mesonBuild, "add_global_arguments('%s', language : 'c')\n", strZ(testBldModule(this)->flag));

        // Build unit test
        strCatZ(
            mesonBuild,
            "\n"
            MESON_COMMENT_BLOCK "\n"
            "# Unit test\n"
            MESON_COMMENT_BLOCK "\n"
            "executable(\n"
            "    'test-unit',\n");

        for (unsigned int dependIdx = 0; dependIdx < strLstSize(testBldModule(this)->dependList); dependIdx++)
        {
            strCatFmt(
                mesonBuild, "    '%s/src/%s.c',\n", strZ(testBldPathRepo(this)),
                strZ(strLstGet(testBldModule(this)->dependList, dependIdx)));
        }

        strCatFmt(
            mesonBuild,
            "    '%s/test/src/common/harnessTest.c',\n"
            "    'test.c',\n"
            "    include_directories:\n"
            "        include_directories(\n"
            "            '.',\n"
            "            '%s/test/src',\n"
            "            '%s/src',\n"
            "        ),\n"
            ")\n",
            strZ(testBldPathRepo(this)), strZ(testBldPathRepo(this)), strZ(testBldPathRepo(this)));

        testBldWrite(storageUnit, storageUnitList, "meson.build", BUFSTR(mesonBuild));

        // Build test.c
        // -----------------------------------------------------------------------------------------------------------------------------
        String *const testC = strCatBuf(
            strNew(), storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("test/src/test.c"))));

        // Files to test/include
        StringList *const testIncludeFileList = strLstNew();

        if (testBldModule(this)->coverageList != NULL)
        {
            for (unsigned int coverageIdx = 0; coverageIdx < lstSize(testBldModule(this)->coverageList); coverageIdx++)
            {
                const TestDefCoverage *const coverage = lstGet(testBldModule(this)->coverageList, coverageIdx);

                if (coverage->coverable)
                    strLstAdd(testIncludeFileList, coverage->name);
            }
        }

        if (testBldModule(this)->includeList != NULL)
        {
            for (unsigned int includeIdx = 0; includeIdx < strLstSize(testBldModule(this)->includeList); includeIdx++)
                strLstAdd(testIncludeFileList, strLstGet(testBldModule(this)->includeList, includeIdx));
        }

        String *const testIncludeFile = strNew();

        for (unsigned int testIncludeFileIdx = 0; testIncludeFileIdx < strLstSize(testIncludeFileList); testIncludeFileIdx++)
        {
            if (testIncludeFileIdx != 0)
                strCatChr(testIncludeFile, '\n');

            strCatFmt(
                testIncludeFile, "#include \"%s/src/%s.c\"", strZ(testBldPathRepo(this)),
                strZ(strLstGet(testIncludeFileList, testIncludeFileIdx)));
        }

        strReplace(testC, STRDEF("{[C_INCLUDE]}"), testIncludeFile);

        // Test path
        strReplace(testC, STRDEF("{[C_TEST_PATH]}"), storagePathP(storageTestId, NULL));

        // Harness data path
        const String *const pathHarnessData = strNewFmt("%s/data-%u", strZ(testBldPathTest(this)), testBldVmId(this));
        strReplace(testC, STRDEF("{[C_HRN_PATH]}"), pathHarnessData);

        // Harness repo path
        strReplace(testC, STRDEF("{[C_HRN_PATH_REPO]}"), testBldPathRepo(this));

        // Path to the project exe when it exists
        const String *const pathProjectExe = storagePathP(
            testBldStorageTest(this),
            strNewFmt("bin/%s%s/" PROJECT_BIN, strZ(testBldVm(this)), strEqZ(testBldVm(this), "none") ? "/src" : ""));
        strReplace(testC, STRDEF("{[C_TEST_PROJECT_EXE]}"), pathProjectExe);

        // Path to source -- used to construct __FILENAME__ tests
        strReplace(testC, STRDEF("{[C_TEST_PGB_PATH]}"), testBldPathRepo(this));

        // Test log level
        strReplace(
            testC, STRDEF("{[C_LOG_LEVEL_TEST]}"),
            bldEnum("logLevel", strLower(strNewZ(logLevelStr(testBldLogLevel(this))))));

        // Log time/timestamp
        strReplace(testC, STRDEF("{[C_TEST_TIMING]}"), STR(cvtBoolToConstZ(testBldLogTime(this))));

        // Test timezone
        strReplace(
            testC, STRDEF("{[C_TEST_TZ]}"),
            testBldTimeZone(this) == NULL ?
                STRDEF("// No timezone specified") : strNewFmt("setenv(\"TZ\", \"%s\", true);", strZ(testBldTimeZone(this))));

        // Scale performance test
        strReplace(testC, STRDEF("{[C_TEST_SCALE]}"), strNewFmt("%" PRIu64, testBldScale(this)));

        // Does this test run in a container?
        strReplace(testC, STRDEF("{[C_TEST_CONTAINER]}"), STR(cvtBoolToConstZ(testBldModule(this)->containerRequired)));

        // User/group info
        strReplace(testC, STRDEF("{[C_TEST_GROUP]}"), groupName());
        strReplace(testC, STRDEF("{[C_TEST_GROUP_ID]}"), strNewFmt("%u", groupId()));
        strReplace(testC, STRDEF("{[C_TEST_USER]}"), userName());
        strReplace(testC, STRDEF("{[C_TEST_USER_ID]}"), strNewFmt("%u", userId()));

        // Test id
        strReplace(testC, STRDEF("{[C_TEST_IDX]}"), strNewFmt("%u", testBldVmId(this)));

        // Include test file
        strReplace(
            testC, STRDEF("{[C_TEST_INCLUDE]}"),
            strNewFmt(
                "#include \"%s/test/src/module/%sTest.c\"", strZ(testBldPathRepo(this)),
                strZ(bldEnum(NULL, testBldModuleName(this)))));

        // Test list
        String *const testList = strNew();

        for (unsigned int testIdx = 0; testIdx < testBldModule(this)->total; testIdx++)
        {
            if (testIdx != 0)
                strCatZ(testList, "\n    ");

            strCatFmt(
                testList, "hrnAdd(%3u, %8s);", testIdx + 1,
                cvtBoolToConstZ(testBldTest(this) == 0 || testBldTest(this) == testIdx + 1));
        }

        strReplace(testC, STRDEF("{[C_TEST_LIST]}"), testList);

        // Write file
        testBldWrite(storageUnit, storageUnitList, "test.c", BUFSTR(testC));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
