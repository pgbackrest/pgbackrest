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
    const String *const pathRepo, const String *const pathTest, const String *const vm, const String *const vmInt,
    const unsigned int vmId, const String *const pgVersion, const TestDefModule *const module, const unsigned int test,
    const uint64_t scale, const LogLevel logLevel, const bool logTime, const String *const timeZone, const bool coverage,
    const bool profile, const bool optimize, const bool backTrace)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(STRING, vmInt);
        FUNCTION_LOG_PARAM(UINT, vmId);
        FUNCTION_LOG_PARAM(STRING, pgVersion);
        FUNCTION_LOG_PARAM_P(VOID, module);
        FUNCTION_LOG_PARAM(UINT, test);
        FUNCTION_LOG_PARAM(UINT64, scale);
        FUNCTION_LOG_PARAM(ENUM, logLevel);
        FUNCTION_LOG_PARAM(BOOL, logTime);
        FUNCTION_LOG_PARAM(STRING, timeZone);
        FUNCTION_LOG_PARAM(BOOL, coverage);
        FUNCTION_LOG_PARAM(BOOL, profile);
        FUNCTION_LOG_PARAM(BOOL, optimize);
        FUNCTION_LOG_PARAM(BOOL, backTrace);
    FUNCTION_LOG_END();

    ASSERT(pathRepo != NULL);
    ASSERT(pathTest != NULL);
    ASSERT(vm != NULL);
    ASSERT(vmInt != NULL);
    ASSERT(module != NULL);
    ASSERT(scale != 0);

    OBJ_NEW_BEGIN(TestBuild, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (TestBuild)
        {
            .pub =
            {
                .pathRepo = strDup(pathRepo),
                .pathTest = strDup(pathTest),
                .vm = strDup(vm),
                .vmInt = strDup(vmInt),
                .vmId = vmId,
                .pgVersion = strDup(pgVersion),
                .module = module,
                .test = test,
                .scale = scale,
                .logLevel = logLevel,
                .logTime = logTime,
                .timeZone = strDup(timeZone),
                .coverage = coverage,
                .profile = profile,
                .optimize = optimize,
                .backTrace = backTrace,
            },
        };

        this->pub.storageRepo = storagePosixNewP(testBldPathRepo(this));
        this->pub.storageTest = storagePosixNewP(testBldPathTest(this));
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(TEST_BUILD, this);
}

/***********************************************************************************************************************************
Shim functions in a module
***********************************************************************************************************************************/
static String *
testBldShim(const String *const shimC, const StringList *const functionList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, shimC);
        FUNCTION_LOG_PARAM(STRING_LIST, functionList);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const inList = strLstNewSplitZ(shimC, "\n");
        ASSERT(strEmpty(strLstGet(inList, strLstSize(inList) - 1)));

        for (unsigned int inIdx = 0; inIdx < strLstSize(inList); inIdx++)
        {
            const String *const in = strLstGet(inList, inIdx);

            // Check if this line contains a shimmed function
            bool found = false;

            for (unsigned int functionIdx = 0; functionIdx < strLstSize(functionList); functionIdx++)
            {
                const String *const function = strLstGet(functionList, functionIdx);

                if (strBeginsWith(in, function) && strChr(in, '(') == (int)strSize(function))
                {
                    ASSERT(inIdx > 0);
                    found = true;

                    // For static functions add a forward declaration with the original name so the function can be called from the
                    // module. For extern functions add a forward declaration with the shimmed name so there is no warning from
                    // missing-prototypes.
                    strCatChr(result, ' ');

                    if (strBeginsWithZ(strLstGet(inList, inIdx - 1), "static "))
                        strCat(result, in);
                    else
                        strCatFmt(result, "%s_SHIMMED%s", strZ(function), strZ(strSub(in, strSize(function))));

                    unsigned int scanIdx = inIdx + 1;

                    while (true)
                    {
                        // In a properly formatted C file the end of the list can never be reached
                        ASSERT(scanIdx < strLstSize(inList));

                        const String *const scan = strLstGet(inList, scanIdx);

                        if (strEqZ(scan, "{"))
                            break;

                        if (strEndsWithZ(strLstGet(inList, scanIdx - 1), ","))
                            strCatChr(result, ' ');

                        strCat(result, strTrim(strDup(scan)));
                        scanIdx++;
                    }

                    strCatZ(result, "; ");
                    strCat(result, strLstGet(inList, inIdx - 1));

                    // Alter the function name so it can be shimmed
                    strCatFmt(result, "\n%s_SHIMMED%s", strZ(function), strZ(strSub(in, strSize(function))));
                    break;
                }
            }

            // Just copy the line when a function is not found
            if (!found)
            {
                if (inIdx != 0)
                    strCatChr(result, '\n');

                strCat(result, in);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
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

/***********************************************************************************************************************************
Generate a relative path from the compare path to the base path
***********************************************************************************************************************************/
static String *
cmdBldPathModule(const String *const moduleName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, moduleName);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strBeginsWithZ(moduleName, "test/"))
            strCatFmt(result, "test/src%s", strZ(strSub(moduleName, 4)));
        else if (strBeginsWithZ(moduleName, "doc/"))
            strCatFmt(result, "doc/src%s", strZ(strSub(moduleName, 3)));
        else
            strCatFmt(result, "src/%s", strZ(moduleName));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Generate a relative path from the compare path to the base path
***********************************************************************************************************************************/
static String *
cmdBldPathRelative(const String *const base, const String *const compare)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, base);
        FUNCTION_LOG_PARAM(STRING, compare);
    FUNCTION_LOG_END();

    ASSERT(base != NULL);
    ASSERT(compare != NULL);
    ASSERT(!strEq(base, compare));

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const baseList = strLstNewSplitZ(base, "/");
        const StringList *const compareList = strLstNewSplitZ(compare, "/");
        unsigned int compareIdx = 0;

        // Find the part of the paths that is the same
        while (
            compareIdx < strLstSize(baseList) && compareIdx < strLstSize(compareList) &&
            strEq(strLstGet(baseList, compareIdx), strLstGet(compareList, compareIdx)))
        {
            compareIdx++;
        }

        // Generate ../ part of relative path
        bool first = true;

        for (unsigned int dotIdx = compareIdx; dotIdx < strLstSize(baseList); dotIdx++)
        {
            if (!first)
                strCatChr(result, '/');
            else
                first = false;

            strCatZ(result, "..");
        }

        // Add remaining path
        for (unsigned int pathIdx = compareIdx; pathIdx < strLstSize(compareList); pathIdx++)
        {
            // If first is still set then compare must be a subpath of base (i.e. there were no .. parts)
            if (!first)
            {
                strCatChr(result, '/');
                first = false;
            }

            strCat(result, strLstGet(compareList, pathIdx));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
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
            strNewFmt("%s/unit-%u/%s", strZ(testBldPathTest(this)), testBldVmId(this), strZ(testBldVm(this))), .write = true);
        const Storage *const storageTestId = storagePosixNewP(
            strNewFmt("%s/test-%u", strZ(testBldPathTest(this)), testBldVmId(this)), .write = true);
        StringList *const storageUnitList = strLstNew();
        const TestDefModule *const module = testBldModule(this);
        const String *const pathUnit = storagePathP(storageUnit, NULL);
        const String *const pathRepo = testBldPathRepo(this);
        const String *const pathRepoRel = cmdBldPathRelative(pathUnit, pathRepo);

        // Build shim modules
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int shimIdx = 0; shimIdx < lstSize(module->shimList); shimIdx++)
        {
            const TestDefShim *const shim = lstGet(module->shimList, shimIdx);

            // Skip this shim for integration tests
            if (module->type == testDefTypeIntegration && !shim->integration)
                continue;

            const String *const shimFile = strNewFmt("%s.c", strZ(cmdBldPathModule(shim->name)));

            String *const shimC = strCatBuf(
                strNew(),
                storageGetP(storageNewReadP(testBldStorageRepo(this), strNewFmt("%s/%s", strZ(pathRepo), strZ(shimFile)))));

            testBldWrite(storageUnit, storageUnitList, strZ(shimFile), BUFSTR(testBldShim(shimC, shim->functionList)));
        }

        // Build harness modules
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *const harnessList = strLstNew();
        StringList *const harnessIncludeList = strLstNew();

        for (unsigned int harnessIdx = 0; harnessIdx < lstSize(module->harnessList); harnessIdx++)
        {
            const TestDefHarness *const harness = lstGet(module->harnessList, harnessIdx);

            // Skip this harness for integration tests
            if (module->type == testDefTypeIntegration && !harness->integration)
                continue;

            const String *const harnessFile = strNewFmt("test/src/common/%s.c", strZ(bldEnum("harness", harness->name)));
            const String *harnessPath = strNewFmt("%s/%s", strZ(pathRepo), strZ(harnessFile));

            // If there are includes then copy and update the harness
            if (!strLstEmpty(harness->includeList))
            {
                String *const includeReplace = strNew();

                for (unsigned int includeIdx = 0; includeIdx < strLstSize(harness->includeList); includeIdx++)
                {
                    const String *const include = strLstGet(harness->includeList, includeIdx);

                    strCatFmt(
                        includeReplace, "%s#include \"%s/%s.c\"", includeIdx == 0 ? "" : "\n",
                        lstExists(module->shimList, &include) ? strZ(pathUnit) : strZ(pathRepo), strZ(cmdBldPathModule(include)));

                    strLstAdd(harnessIncludeList, include);
                }

                String *const harnessC = strCatBuf(strNew(), storageGetP(storageNewReadP(testBldStorageRepo(this), harnessPath)));

                strReplace(harnessC, STRDEF("{[SHIM_MODULE]}"), includeReplace);
                testBldWrite(storageUnit, storageUnitList, strZ(harnessFile), BUFSTR(harnessC));
                strLstAdd(harnessList, harnessFile);
            }
            // Else harness can be referenced directly from the repo path
            else
                strLstAddFmt(harnessList, "%s/%s", strZ(pathRepoRel), strZ(harnessFile));
        }

        // Copy meson_options.txt
        // -------------------------------------------------------------------------------------------------------------------------
        testBldWrite(
            storageUnit, storageUnitList, "meson_options.txt",
            storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("meson_options.txt"))));

        // Build meson.build
        // -------------------------------------------------------------------------------------------------------------------------
        String *const mesonBuild = strCatBuf(
            strNew(), storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("meson.build"))));

        // Comment out subdirs that are not used for testing
        strReplace(mesonBuild, STRDEF("subdir('"), STRDEF("# subdir('"));

        if (!testBldBackTrace(this))
        {
            strReplace(
                mesonBuild, STRDEF("    configuration.set('HAVE_LIBBACKTRACE'"),
                STRDEF("#    configuration.set('HAVE_LIBBACKTRACE'"));
        }

        // Write build.auto.in
        strCatZ(
            mesonBuild,
            "\n"
            MESON_COMMENT_BLOCK "\n"
            "# Write configuration\n"
            MESON_COMMENT_BLOCK "\n"
            "configure_file(output: 'build.auto.h', configuration: configuration)\n"
            "\n"
            "add_global_arguments('-DFN_EXTERN=extern', language : 'c')\n"
            "add_global_arguments('-DVR_EXTERN_DECLARE=extern', language : 'c')\n"
            "add_global_arguments('-DVR_EXTERN_DEFINE=', language : 'c')\n"
            "add_global_arguments('-DERROR_MESSAGE_BUFFER_SIZE=131072', language : 'c')\n");

        // Configure features
        if (module->feature != NULL)
            strCatFmt(mesonBuild, "add_global_arguments('-DHRN_INTEST_%s', language : 'c')\n", strZ(module->feature));

        if (module->featureList != NULL)
        {
            for (unsigned int featureIdx = 0; featureIdx < strLstSize(module->featureList); featureIdx++)
            {
                strCatFmt(
                    mesonBuild, "add_global_arguments('-DHRN_FEATURE_%s', language : 'c')\n",
                    strZ(strLstGet(module->featureList, featureIdx)));
            }
        }

        // Add compiler flags
        if (module->flag != NULL)
            strCatFmt(mesonBuild, "add_global_arguments('%s', language : 'c')\n", strZ(module->flag));

        // Add coverage
        if (testBldCoverage(this))
            strCatZ(mesonBuild, "add_global_arguments('-DDEBUG_COVERAGE', language : 'c')\n");

        // Add container flag
        if (!strEqZ(testBldVm(this), "none"))
            strCatZ(mesonBuild, "add_global_arguments('-DTEST_CONTAINER_REQUIRED', language : 'c')\n");

        // Build unit test
        strCatZ(
            mesonBuild,
            "\n"
            MESON_COMMENT_BLOCK "\n"
            "# Unit test\n"
            MESON_COMMENT_BLOCK "\n"
            "src_unit = files(\n");

        for (unsigned int dependIdx = 0; dependIdx < strLstSize(module->dependList); dependIdx++)
        {
            const String *const depend = strLstGet(module->dependList, dependIdx);

            if (strLstExists(harnessIncludeList, depend))
                continue;

            strCatFmt(mesonBuild, "    '%s/%s.c',\n", strZ(pathRepoRel), strZ(cmdBldPathModule(depend)));
        }

        // Add harnesses
        for (unsigned int harnessIdx = 0; harnessIdx < strLstSize(harnessList); harnessIdx++)
        {
            const TestDefHarness *const harness = lstGet(module->harnessList, harnessIdx);

            // Add harness depends
            const String *const harnessDependPath = strNewFmt("test/src/common/%s", strZ(bldEnum("harness", harness->name)));
            StorageIterator *const storageItr = storageNewItrP(
                testBldStorageRepo(this), harnessDependPath, .expression = STRDEF("\\.c$"), .sortOrder = sortOrderAsc);

            while (storageItrMore(storageItr))
            {
                strCatFmt(
                    mesonBuild, "    '%s/%s/%s',\n", strZ(pathRepoRel), strZ(harnessDependPath),
                    strZ(storageItrNext(storageItr).name));
            }

            // Add harness if no includes are in module or coverage includes
            unsigned int includeIdx = 0;

            for (; includeIdx < strLstSize(harness->includeList); includeIdx++)
            {
                const String *const include = strLstGet(harness->includeList, includeIdx);

                if (lstExists(module->coverageList, &include) || strLstExists(module->includeList, include))
                    break;
            }

            if (includeIdx != strLstSize(harness->includeList))
                continue;

            strCatFmt(mesonBuild, "    '%s',\n", strZ(strLstGet(harnessList, harnessIdx)));
        }

        strCatFmt(
            mesonBuild,
            "    '%s/test/src/common/harnessTest.c',\n"
            "    'test.c',\n"
            ")\n"
            "\n"
            "executable(\n"
            "    'test-unit',\n"
            "    sources: src_unit,\n",
            strZ(pathRepoRel));

        // Add C args
        String *const cArg = strNew();

        if (testBldOptimize(this) || module->type == testDefTypePerformance)
            strCatZ(cArg, "\n        '-O2',");

        if (testBldProfile(this))
        {
            strCatZ(
                cArg,
                "\n        '-pg',"
                "\n        '-no-pie',");
        }

        if (!strEmpty(cArg))
        {
            strCatFmt(
                mesonBuild,
                "    c_args: [%s\n"
                "    ],\n",
                strZ(cArg));
        }

        // Add linker args
        String *const linkArg = strNew();

        if (testBldProfile(this))
        {
            strCatZ(
                linkArg,
                "\n        '-pg',"
                "\n        '-no-pie',");
        }

        if (!strEmpty(linkArg))
        {
            strCatFmt(
                mesonBuild,
                "    link_args: [%s\n"
                "    ],\n",
                strZ(linkArg));
        }

        strCatFmt(
            mesonBuild,
            "    include_directories:\n"
            "        include_directories(\n"
            "            '.',\n"
            "            '%s/src',\n"
            "            '%s/doc/src',\n"
            "            '%s/test/src',\n"
            "        ),\n"
            "    dependencies: [\n",
            strZ(pathRepoRel), strZ(pathRepoRel), strZ(pathRepoRel));

        if (testBldBackTrace(this))
        {
            strCatZ(
                mesonBuild,
                "        lib_backtrace,\n");
        }

        strCatZ(
            mesonBuild,
            "        lib_bz2,\n"
            "        lib_openssl,\n"
            "        lib_lz4,\n"
            "        lib_pq,\n"
            "        lib_ssh2,\n"
            "        lib_xml,\n"
            "        lib_yaml,\n"
            "        lib_z,\n"
            "        lib_zstd,\n"
            "    ],\n"
            ")\n");

        testBldWrite(storageUnit, storageUnitList, "meson.build", BUFSTR(mesonBuild));

        // Build test.c
        // -------------------------------------------------------------------------------------------------------------------------
        String *const testC = strCatBuf(
            strNew(), storageGetP(storageNewReadP(testBldStorageRepo(this), STRDEF("test/src/test.c"))));

        // Enable debug test trace
        if (!testBldProfile(this) && module->type != testDefTypePerformance)
            strReplace(testC, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("#define DEBUG_TEST_TRACE"));
        else
            strReplace(testC, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("// Debug test trace not enabled"));

        // Files to test/include
        StringList *const testIncludeFileList = strLstNew();

        for (unsigned int coverageIdx = 0; coverageIdx < lstSize(module->coverageList); coverageIdx++)
        {
            const TestDefCoverage *const coverage = lstGet(module->coverageList, coverageIdx);

            if (coverage->coverable && !coverage->included)
                strLstAdd(testIncludeFileList, coverage->name);
        }

        for (unsigned int includeIdx = 0; includeIdx < strLstSize(module->includeList); includeIdx++)
            strLstAdd(testIncludeFileList, strLstGet(module->includeList, includeIdx));

        StringList *const testIncludeFile = strLstNew();
        StringList *const harnessIncluded = strLstNew();

        for (unsigned int testIncludeFileIdx = 0; testIncludeFileIdx < strLstSize(testIncludeFileList); testIncludeFileIdx++)
        {
            const String *const include = strLstGet(testIncludeFileList, testIncludeFileIdx);
            unsigned int harnessIdx = 0;

            for (; harnessIdx < lstSize(module->harnessList); harnessIdx++)
            {
                const TestDefHarness *const harness = lstGet(module->harnessList, harnessIdx);

                if (strLstExists(harness->includeList, include))
                    break;
            }

            if (harnessIdx != lstSize(module->harnessList))
            {
                if (!strLstExists(harnessIncluded, strLstGet(harnessList, harnessIdx)))
                {
                    strLstAddFmt(testIncludeFile, "#include \"%s\"", strZ(strLstGet(harnessList, harnessIdx)));
                    strLstAdd(harnessIncluded, strLstGet(harnessList, harnessIdx));
                }
            }
            else
                strLstAddFmt(testIncludeFile, "#include \"%s/%s.c\"", strZ(pathRepoRel), strZ(cmdBldPathModule(include)));
        }

        strReplace(testC, STRDEF("{[C_INCLUDE]}"), strLstJoin(testIncludeFile, "\n"));

        // Test path
        strReplace(testC, STRDEF("{[C_TEST_PATH]}"), storagePathP(storageTestId, NULL));

        // Harness data path
        const String *const pathHarnessData = strNewFmt("%s/data-%u", strZ(testBldPathTest(this)), testBldVmId(this));
        strReplace(testC, STRDEF("{[C_HRN_PATH]}"), pathHarnessData);

        // Harness repo path
        strReplace(testC, STRDEF("{[C_HRN_PATH_REPO]}"), pathRepo);

        // Path to the project exe when it exists
        strReplace(
            testC, STRDEF("{[C_TEST_PROJECT_EXE]}"),
            storagePathP(testBldStorageTest(this), strNewFmt("build/%s/src/" PROJECT_BIN, strZ(testBldVmInt(this)))));

        // Path to source -- used to construct __FILENAME__ tests
        strReplace(testC, STRDEF("{[C_TEST_PGB_PATH]}"), strNewFmt("../%s", strZ(pathRepoRel)));

        // Test expect logging
        strReplace(testC, STRDEF("{[C_TEST_LOG_EXPECT]}"), module->type == testDefTypeUnit ? TRUE_STR : FALSE_STR);

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
                STRDEF("// No timezone specified") : strNewFmt("hrnTzSet(\"%s\");", strZ(testBldTimeZone(this))));

        // Scale performance test
        strReplace(testC, STRDEF("{[C_TEST_SCALE]}"), strNewFmt("%" PRIu64, testBldScale(this)));

        // Does this test run in a container?
        strReplace(testC, STRDEF("{[C_TEST_CONTAINER]}"), STR(cvtBoolToConstZ(!strEqZ(testBldVm(this), "none"))));

        // User/group info
        strReplace(testC, STRDEF("{[C_TEST_GROUP]}"), groupName());
        strReplace(testC, STRDEF("{[C_TEST_GROUP_ID]}"), strNewFmt("%u", groupId()));
        strReplace(testC, STRDEF("{[C_TEST_USER]}"), userName());
        strReplace(testC, STRDEF("{[C_TEST_USER_LEN]}"), strNewFmt("%zu", strSize(userName())));
        strReplace(testC, STRDEF("{[C_TEST_USER_ID]}"), strNewFmt("%u", userId()));

        // VM for integration testing
        strReplace(testC, STRDEF("{[C_TEST_VM]}"), testBldVmInt(this));

        // PostgreSQL version for integration testing
        strReplace(testC, STRDEF("{[C_TEST_PG_VERSION]}"), testBldPgVersion(this));

        // Test id
        strReplace(testC, STRDEF("{[C_TEST_IDX]}"), strNewFmt("%u", testBldVmId(this)));

        // Include test file
        strReplace(
            testC, STRDEF("{[C_TEST_INCLUDE]}"),
            strNewFmt("#include \"%s/test/src/module/%sTest.c\"", strZ(pathRepoRel), strZ(bldEnum(NULL, module->name))));

        // Test list
        String *const testList = strNew();

        for (unsigned int testIdx = 0; testIdx < module->total; testIdx++)
        {
            if (testIdx != 0)
                strCatZ(testList, "\n    ");

            strCatFmt(
                testList, "hrnAdd(%3u, %8s);", testIdx + 1,
                cvtBoolToConstZ(testBldTest(this) == 0 || testBldTest(this) == testIdx + 1));
        }

        strReplace(testC, STRDEF("{[C_TEST_LIST]}"), testList);

        // Profiling
        strReplace(testC, STRDEF("{[C_TEST_PROFILE]}"), STR(cvtBoolToConstZ(testBldProfile(this))));
        strReplace(testC, STRDEF("{[C_TEST_PATH_BUILD]}"), strNewFmt("%s/build", strZ(pathUnit)));

        // Write file
        testBldWrite(storageUnit, storageUnitList, "test.c", BUFSTR(testC));

        // Clean files that are not valid for this test
        // -------------------------------------------------------------------------------------------------------------------------
        StorageIterator *const storageItr = storageNewItrP(storageUnit, NULL, .recurse = true);

        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            if (info.type != storageTypeFile || strBeginsWithZ(info.name, "build/"))
                continue;

            if (!strLstExists(storageUnitList, info.name))
                storageRemoveP(storageUnit, info.name, .errorOnMissing = true);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
