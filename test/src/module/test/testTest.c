/***********************************************************************************************************************************
Test Test Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Build list of test files to compare
***********************************************************************************************************************************/
static StringList *
testStorageList(const Storage *const storage)
{
    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageIterator *storageItr = storageNewItrP(storage, NULL, .sortOrder = sortOrderAsc, .recurse = true);

        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            if (info.type == storageTypeFile && !strBeginsWithZ(info.name, "build/"))
                strLstAdd(result, info.name);
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Copy a list of files from the repo to the test repo
***********************************************************************************************************************************/
#define TEST_COPY(source, destination, ...)                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        const char *const copyList[] = {__VA_ARGS__};                                                                              \
                                                                                                                                   \
        for (unsigned int copyIdx = 0; copyIdx < LENGTH_OF(copyList); copyIdx++)                                                   \
        {                                                                                                                          \
            HRN_STORAGE_PUT(                                                                                                       \
                destination, zNewFmt("repo/%s", copyList[copyIdx]), storageGetP(storageNewReadP(source, STR(copyList[copyIdx])))); \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    const Storage *const storageRepo = storagePosixNewP(STRDEF(HRN_PATH_REPO));
    const Storage *const storageTest = storagePosixNewP(STRDEF(TEST_PATH), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("cmdBldPathRelative()"))
    {
        TEST_ERROR(cmdBldPathRelative(STRDEF("/tmp"), STRDEF("/tmp")), AssertError, "assertion '!strEq(base, compare)' failed");

        TEST_RESULT_STR_Z(cmdBldPathRelative(STRDEF("/tmp/sub"), STRDEF("/tmp")), "..", "compare is sub of base");
        TEST_RESULT_STR_Z(cmdBldPathRelative(STRDEF("/tmp"), STRDEF("/tmp/sub")), "sub", "base is sub of compare");
    }

    // *****************************************************************************************************************************
    if (testBegin("lint*()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lintStrId()");

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test.c",
            "#define STRID5(str, strId)\n"
            "#define STRID6(str, strId)\n"
            "STR" "ID5(\"abcd\", TEST_STRID)\n"
            "STR" "ID5(\\\"abcd\\\")\n"
            "STR" "ID5(\\\"abcd)\n"
            "STR" "ID5(abcd)\n"
            "STR" "ID5( \"abcd)\n"
            "STRID5(\"abcd\")\n"
            "STRID5(\"abcd\", 0x20c410)\n");

        TEST_ERROR(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("none"), 3, STRDEF("invalid"),
                STRDEF("common/stack-trace"), 0, 1, logLevelDebug, true, NULL, false, false, false, true),
            FormatError, "4 linter error(s) in 'test.c' (see warnings above)");

        TEST_RESULT_LOG(
            "P00   WARN: 'STR" "ID5(\\\"abcd)' must have quotes around string parameter '\\\"abcd'\n"
            "P00   WARN: 'STR" "ID5(abcd)' must have quotes around string parameter 'abcd'\n"
            "P00   WARN: 'STR" "ID5( \"abcd)' must have quotes around string parameter '\"abcd'\n"
            "P00   WARN: 'STRID5(\"abcd\")' should be 'STRID5(\"abcd\", 0x20c410)'");
    }

    // *****************************************************************************************************************************
    if (testBegin("TestDef and TestBuild"))
    {
        // meson_options.txt
        // -------------------------------------------------------------------------------------------------------------------------
        const char *const mesonOption = strZ(strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("meson_options.txt")))));
        HRN_STORAGE_PUT_Z(storageTest, "repo/meson_options.txt", mesonOption);

        // Root meson.build
        // -------------------------------------------------------------------------------------------------------------------------
        String *const mesonBuildRoot = strCat(
            strNew(), strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("meson.build")))));

        HRN_STORAGE_PUT_Z(storageTest, "repo/meson.build", strZ(mesonBuildRoot));
        strReplace(mesonBuildRoot, STRDEF("subdir('"), STRDEF("# subdir('"));

        strCatZ(
            mesonBuildRoot,
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

        // harnessError.c
        // -------------------------------------------------------------------------------------------------------------------------
        String *const harnessErrorC = strCat(
            strNew(), strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("test/src/common/harnessError.c")))));

        strReplace(harnessErrorC, STRDEF("{[SHIM_MODULE]}"), STRDEF("#include \"" TEST_PATH "/repo/src/common/error/error.c\""));

        // Unit test harness
        // -------------------------------------------------------------------------------------------------------------------------
        String *const testC = strCat(strNew(), strNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("test/src/test.c")))));

        strReplace(testC, STRDEF("{[C_HRN_PATH]}"), STRDEF(TEST_PATH "/test/data-3"));
        strReplace(testC, STRDEF("{[C_HRN_PATH_REPO]}"), STRDEF(TEST_PATH "/repo"));
        strReplace(testC, STRDEF("{[C_LOG_LEVEL_TEST]}"), STRDEF("logLevelDebug"));
        strReplace(testC, STRDEF("{[C_TEST_GROUP]}"), STRDEF(TEST_GROUP));
        strReplace(testC, STRDEF("{[C_TEST_GROUP_ID]}"), STRDEF(TEST_GROUP_ID_Z));
        strReplace(testC, STRDEF("{[C_TEST_GROUP_ID_Z]}"), STRDEF("\"" TEST_GROUP_ID_Z "\""));
        strReplace(testC, STRDEF("{[C_TEST_IDX]}"), STRDEF("3"));
        strReplace(testC, STRDEF("{[C_TEST_PATH]}"), STRDEF(TEST_PATH "/test/test-3"));
        strReplace(testC, STRDEF("{[C_TEST_PGB_PATH]}"), STRDEF("../../../../repo"));
        strReplace(testC, STRDEF("{[C_TEST_SCALE]}"), STRDEF("1"));
        strReplace(testC, STRDEF("{[C_TEST_TIMING]}"), STRDEF("true"));
        strReplace(testC, STRDEF("{[C_TEST_USER]}"), STRDEF(TEST_USER));
        strReplace(testC, STRDEF("{[C_TEST_USER_ID]}"), STRDEF(TEST_USER_ID_Z));
        strReplace(testC, STRDEF("{[C_TEST_USER_ID_Z]}"), STRDEF("\"" TEST_USER_ID_Z "\""));
        strReplace(testC, STRDEF("{[C_TEST_USER_LEN]}"), strNewFmt("%zu", sizeof(TEST_USER) - 1));

        // Test definition
        // -------------------------------------------------------------------------------------------------------------------------
        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/define.yaml",
            "unit:\n"
            "  - name: common\n"
            "    test:\n"
            "      - name: pre\n"
            "        total: 1\n"
            "\n"
            "      - name: error\n"
            "        total: 9\n"
            "        feature: error\n"
            "        harness:\n"
            "          name: error\n"
            "          shim:\n"
            "            common/error/error: ~\n"
            "        coverage:\n"
            "          - common/error/error\n"
            "          - common/error/error.auto: noCode\n"
            "          - common/error/error.inc: included\n"
            "        depend:\n"
            "          - common/stackTrace\n"
            "          - common/type/stringStatic\n"
            "\n"
            "      - name: stack-trace\n"
            "        total: 4\n"
            "        feature: stackTrace\n"
            "        harness:\n"
            "          name: stackTrace\n"
            "          integration: false\n"
            "          shim:\n"
            "            common/stackTrace:\n"
            "              function:\n"
            "                - stackTraceBackCallback\n"
            "        coverage:\n"
            "          - common/stackTrace\n"
            "          - common/type/stringStatic\n"
            "        depend:\n"
            "          - common/debug\n"
            "\n"
            "  - name: test\n"
            "    test:\n"
            "      - name: shim\n"
            "        binReq: true\n"
            "        containerReq: true\n"
            "        total: 1\n"
            "        define: -DNDEBUG\n"
            "        harness: noShim\n"
            "        harness:\n"
            "          name: shim\n"
            "          shim:\n"
            "            test/common/shim:\n"
            "              function:\n"
            "                - shimFunc\n"
            "                - shimFunc2\n"
            "            test/common/shim2: ~\n"
            "        coverage:\n"
            "          - test/common/shim\n"
            "          - test/common/shim2\n"
            "        include:\n"
            "          - common/error/error\n"
            "          - test/common/include\n"
            "          - doc/command/build/build\n"
            "\n"
            "integration:\n"
            "  - name: integration\n"
            "    test:\n"
            "      - name: all\n"
            "        total: 2\n"
            "\n"
            "  - name: real\n"
            "    db: true\n"
            "    test:\n"
            "      - name: all\n"
            "        total: 1\n"
            "\n"
            "performance:\n"
            "  - name: performance\n"
            "    test:\n"
            "      - name: type\n"
            "        total: 1\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test common/stack-trace");

        TEST_COPY(
            storageRepo, storageTest,
            "src/common/assert.h",
            "src/common/debug.c",
            "src/common/debug.h",
            "src/common/error/error.auto.c.inc",
            "src/common/error/error.auto.h",
            "src/common/error/error.c",
            "src/common/error/error.h",
            "src/common/logLevel.h",
            "src/common/macro.h",
            "src/common/stackTrace.c",
            "src/common/stackTrace.h",
            "src/common/type/convert.h",
            "src/common/type/param.h",
            "src/common/type/stringStatic.h",
            "src/common/type/stringStatic.c",
            "src/common/type/stringZ.h",
            "test/src/common/harnessDebug.h",
            "test/src/common/harnessLog.h",
            "test/src/common/harnessError.c",
            "test/src/common/harnessError.h",
            "test/src/common/harnessStackTrace.h",
            "test/src/common/harnessStackTrace.c",
            "test/src/common/harnessTest.c",
            "test/src/common/harnessTest.h",
            "test/src/common/harnessTest.intern.h",
            "test/src/module/common/stackTraceTest.c",
            "test/src/test.c");

        HRN_STORAGE_PUT_EMPTY(storageTest, "repo/doc/src/command/build/build.c");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("none"), 3, STRDEF("invalid"),
                STRDEF("common/stack-trace"), 0, 1, logLevelDebug, true, NULL, false, false, false, true),
            "new build");

        // Older versions of ninja may error on a rebuild so a retry may occur
        TEST_RESULT_LOG_EMPTY_OR_CONTAINS("WARN: build failed for unit");

        const Storage *storageUnit = storagePosixNewP(STRDEF(TEST_PATH "/test/unit-3/none"));
        StringList *fileList = testStorageList(storageUnit);

        TEST_RESULT_STRLST_Z(
            fileList,
            "meson.build\n"
            "meson_options.txt\n"
            "src/common/stackTrace.c\n"
            "test/src/common/harnessError.c\n"
            "test/src/common/harnessStackTrace.c\n"
            "test.c\n",
            "check files");

        for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
        {
            const String *const file = strLstGet(fileList, fileIdx);

            if (strEqZ(file, "meson.build"))
            {
                TEST_STORAGE_GET(
                    storageUnit, strZ(file),
                    zNewFmt(
                        "%s"
                        "add_global_arguments('-DHRN_INTEST_STACKTRACE', language : 'c')\n"
                        "add_global_arguments('-DHRN_FEATURE_ERROR', language : 'c')\n"
                        "\n"
                        MESON_COMMENT_BLOCK "\n"
                        "# Unit test\n"
                        MESON_COMMENT_BLOCK "\n"
                        "src_unit = files(\n"
                        "    '../../../repo/src/common/debug.c',\n"
                        "    'test/src/common/harnessError.c',\n"
                        "    '../../../repo/test/src/common/harnessTest.c',\n"
                        "    'test.c',\n"
                        ")\n"
                        "\n"
                        "executable(\n"
                        "    'test-unit',\n"
                        "    sources: src_unit,\n"
                        "    include_directories:\n"
                        "        include_directories(\n"
                        "            '.',\n"
                        "            '../../../repo/src',\n"
                        "            '../../../repo/doc/src',\n"
                        "            '../../../repo/test/src',\n"
                        "        ),\n"
                        "    dependencies: [\n"
                        "        lib_backtrace,\n"
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
                        ")\n",
                        strZ(mesonBuildRoot)));
            }
            else if (strEqZ(file, "meson_options.txt"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), mesonOption);
            }
            else if (strEqZ(file, "src/common/stackTrace.c") || strEqZ(file, "test/src/common/harnessStackTrace.c"))
            {
                // No test needed
            }
            else if (strEqZ(file, "test/src/common/harnessError.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessErrorC));
            }
            else if (strEqZ(file, "test.c"))
            {
                String *const testCDup = strCat(strNew(), testC);

                strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("none"));
                strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
                strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("#define DEBUG_TEST_TRACE"));
                strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/none/build"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/none/src/pgbackrest"));
                strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("// No timezone specified"));

                strReplace(
                    testCDup, STRDEF("{[C_INCLUDE]}"),
                    STRDEF(
                        "#include \"test/src/common/harnessStackTrace.c\"\n"
                        "#include \"../../../repo/src/common/type/stringStatic.c\""));
                strReplace(testCDup, STRDEF("{[C_INCLUDE]}"), STRDEF("#include \"test/src/common/harnessStackTrace.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_INCLUDE]}"),
                    STRDEF("#include \"../../../repo/test/src/module/common/stackTraceTest.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_LIST]}"),
                    STRDEF(
                        "hrnAdd(  1,     true);\n"
                        "    hrnAdd(  2,     true);\n"
                        "    hrnAdd(  3,     true);\n"
                        "    hrnAdd(  4,     true);"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(testCDup));
            }
            else
                THROW_FMT(TestError, "no test for '%s'", strZ(file));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test common/error");

        TEST_COPY(
            storageRepo, storageTest,
            "test/src/common/harnessFork.h",
            "test/src/module/common/errorTest.c");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("none"), 3, STRDEF("invalid"),
                STRDEF("common/error"), 5, 1, logLevelDebug, true, NULL, false, false, false, true),
            "new build");

        // Older versions of ninja may error on a rebuild so a retry may occur
        TEST_RESULT_LOG_EMPTY_OR_CONTAINS("WARN: build failed for unit");

        fileList = testStorageList(storageUnit);

        TEST_RESULT_STRLST_Z(
            fileList,
            "meson.build\n"
            "meson_options.txt\n"
            "test/src/common/harnessError.c\n"
            "test.c\n",
            "check files");

        for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
        {
            const String *const file = strLstGet(fileList, fileIdx);

            if (strEqZ(file, "meson.build"))
            {
                TEST_STORAGE_GET(
                    storageUnit, strZ(file),
                    zNewFmt(
                        "%s"
                        "add_global_arguments('-DHRN_INTEST_ERROR', language : 'c')\n"
                        "\n"
                        MESON_COMMENT_BLOCK "\n"
                        "# Unit test\n"
                        MESON_COMMENT_BLOCK "\n"
                        "src_unit = files(\n"
                        "    '../../../repo/src/common/stackTrace.c',\n"
                        "    '../../../repo/src/common/type/stringStatic.c',\n"
                        "    '../../../repo/test/src/common/harnessTest.c',\n"
                        "    'test.c',\n"
                        ")\n"
                        "\n"
                        "executable(\n"
                        "    'test-unit',\n"
                        "    sources: src_unit,\n"
                        "    include_directories:\n"
                        "        include_directories(\n"
                        "            '.',\n"
                        "            '../../../repo/src',\n"
                        "            '../../../repo/doc/src',\n"
                        "            '../../../repo/test/src',\n"
                        "        ),\n"
                        "    dependencies: [\n"
                        "        lib_backtrace,\n"
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
                        ")\n",
                        strZ(mesonBuildRoot)));
            }
            else if (strEqZ(file, "meson_options.txt"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), mesonOption);
            }
            else if (strEqZ(file, "test/src/common/harnessError.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessErrorC));
            }
            else if (strEqZ(file, "test.c"))
            {
                String *const testCDup = strCat(strNew(), testC);

                strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("none"));
                strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
                strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("#define DEBUG_TEST_TRACE"));
                strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/none/build"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/none/src/pgbackrest"));
                strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("// No timezone specified"));

                strReplace(
                    testCDup, STRDEF("{[C_INCLUDE]}"),
                    STRDEF("#include \"test/src/common/harnessError.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_INCLUDE]}"),
                    STRDEF("#include \"../../../repo/test/src/module/common/errorTest.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_LIST]}"),
                    STRDEF(
                        "hrnAdd(  1,    false);\n"
                        "    hrnAdd(  2,    false);\n"
                        "    hrnAdd(  3,    false);\n"
                        "    hrnAdd(  4,    false);\n"
                        "    hrnAdd(  5,     true);\n"
                        "    hrnAdd(  6,    false);\n"
                        "    hrnAdd(  7,    false);\n"
                        "    hrnAdd(  8,    false);\n"
                        "    hrnAdd(  9,    false);"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(testCDup));
            }
            else
                THROW_FMT(TestError, "no test for '%s'", strZ(file));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test test/shim");

        String *const shimC = strCatZ(
            strNew(),
            "int\n"
            "shimFunc(void)\n"
            "{\n"
            "    return 777;\n"
            "}\n"
            "\n"
            "static int\n"
            "shimFunc2(\n"
            "   int param1,\n"
            "   int param2)\n"
            "{\n"
            "    return 777 + param1 + param2;\n"
            "}\n");

        HRN_STORAGE_PUT_Z(storageTest, "repo/test/src/common/shim.c", strZ(shimC));
        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/src/common/shim2.c",
            "int noShimFunc3(void);"
            "int noShimFunc3(void)\n"
            "{\n"
            "    return 888;\n"
            "}\n");

        strReplace(shimC, STRDEF("int\nshimFunc(void)"), STRDEF("int shimFunc_SHIMMED(void); int\nshimFunc_SHIMMED(void)"));
        strReplace(
            shimC, STRDEF("static int\nshimFunc2("),
            STRDEF("static int shimFunc2(int param1, int param2); static int\nshimFunc2_SHIMMED("));

        String *const harnessShimC = strCatZ(
            strNew(),
            "{[SHIM_MODULE]}\n"
            "\n"
            "static int\n"
            "shimFunc(void)\n"
            "{\n"
            "    return shimFunc_SHIMMED() + 1;\n"
            "    (void)shimFunc2; // To suppress unused warnings\n"
            "}\n"
            "\n"
            "static int\n"
            "shimFunc2(int param1, int param2)\n"
            "{\n"
            "    return shimFunc2_SHIMMED(param1, param2) + 2;\n"
            "    (void)shimFunc; // To suppress unused warnings\n"
            "}\n");

        HRN_STORAGE_PUT_Z(storageTest, "repo/test/src/common/harnessShim.c", strZ(harnessShimC));
        HRN_STORAGE_PUT_EMPTY(storageTest, "repo/test/src/common/harnessShim/sub.c");
        HRN_STORAGE_PUT_EMPTY(storageTest, "repo/test/src/common/harnessNoShim.c");
        HRN_STORAGE_PUT_EMPTY(storageTest, "repo/test/src/common/include.c");

        strReplace(
            harnessShimC, STRDEF("{[SHIM_MODULE]}"),
            STRDEF(
                "#include \"" TEST_PATH "/test/unit-3/uXX/test/src/common/shim.c\"\n"
                "#include \"" TEST_PATH "/repo/test/src/common/shim2.c\""));

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/src/module/test/shimTest.c",
            "static void\n"
            "testRun(void)\n"
            "{\n"
            "    FUNCTION_HARNESS_VOID();\n"
            "\n"
            "    if (testBegin(\"shims\"))\n"
            "    {\n"
            "        TEST_RESULT_INT(shimFunc_SHIMMED(), 777, \"shimFunc()\");\n"
            "        TEST_RESULT_INT(shimFunc(), 778, \"shimFunc()\");\n"
            "\n"
            "        TEST_RESULT_INT(shimFunc2_SHIMMED(111, 112), 1000, \"shimFunc2()\");\n"
            "        TEST_RESULT_INT(shimFunc2(111, 112), 1002, \"shimFunc2()\");\n"
            "\n"
            "        TEST_RESULT_INT(noShimFunc3(), 888, \"noShimFunc3()\");\n"
            "    }\n"
            "\n"
            "    FUNCTION_HARNESS_RETURN_VOID();\n"
            "}\n");

        HRN_STORAGE_PUT_EMPTY(storageTest, TEST_PATH "/test/unit-3/uXX/cleanme.txt");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("test/shim"), 0, 1, logLevelDebug, true, NULL, true, true, true, true),
            "new build");

        // Older versions of ninja may error on a rebuild so a retry may occur
        TEST_RESULT_LOG_EMPTY_OR_CONTAINS("WARN: build failed for unit");

        storageUnit = storagePosixNewP(STRDEF(TEST_PATH "/test/unit-3/uXX"));
        fileList = testStorageList(storageUnit);

        TEST_RESULT_STRLST_Z(
            fileList,
            "meson.build\n"
            "meson_options.txt\n"
            "src/common/stackTrace.c\n"
            "test/src/common/harnessError.c\n"
            "test/src/common/harnessShim.c\n"
            "test/src/common/harnessStackTrace.c\n"
            "test/src/common/shim.c\n"
            "test.c\n",
            "check files");

        for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
        {
            const String *const file = strLstGet(fileList, fileIdx);

            if (strEqZ(file, "meson.build"))
            {
                TEST_STORAGE_GET(
                    storageUnit, strZ(file),
                    zNewFmt(
                        "%s"
                        "add_global_arguments('-DHRN_FEATURE_ERROR', language : 'c')\n"
                        "add_global_arguments('-DHRN_FEATURE_STACKTRACE', language : 'c')\n"
                        "add_global_arguments('-DNDEBUG', language : 'c')\n"
                        "add_global_arguments('-DDEBUG_COVERAGE', language : 'c')\n"
                        "add_global_arguments('-DTEST_CONTAINER_REQUIRED', language : 'c')\n"
                        "\n"
                        MESON_COMMENT_BLOCK "\n"
                        "# Unit test\n"
                        MESON_COMMENT_BLOCK "\n"
                        "src_unit = files(\n"
                        "    '../../../repo/src/common/type/stringStatic.c',\n"
                        "    '../../../repo/src/common/debug.c',\n"
                        "    'test/src/common/harnessStackTrace.c',\n"
                        "    '../../../repo/test/src/common/harnessNoShim.c',\n"
                        "    '../../../repo/test/src/common/harnessShim/sub.c',\n"
                        "    '../../../repo/test/src/common/harnessTest.c',\n"
                        "    'test.c',\n"
                        ")\n"
                        "\n"
                        "executable(\n"
                        "    'test-unit',\n"
                        "    sources: src_unit,\n"
                        "    c_args: [\n"
                        "        '-O2',\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    link_args: [\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    include_directories:\n"
                        "        include_directories(\n"
                        "            '.',\n"
                        "            '../../../repo/src',\n"
                        "            '../../../repo/doc/src',\n"
                        "            '../../../repo/test/src',\n"
                        "        ),\n"
                        "    dependencies: [\n"
                        "        lib_backtrace,\n"
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
                        ")\n",
                        strZ(mesonBuildRoot)));
            }
            else if (strEqZ(file, "meson_options.txt"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), mesonOption);
            }
            else if (strEqZ(file, "src/common/stackTrace.c") || strEqZ(file, "test/src/common/harnessStackTrace.c"))
            {
                // No test needed
            }
            else if (strEqZ(file, "test/src/common/harnessError.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessErrorC));
            }
            else if (strEqZ(file, "test/src/common/harnessShim.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessShimC));
            }
            else if (strEqZ(file, "test/src/common/shim.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(shimC));
            }
            else if (strEqZ(file, "test.c"))
            {
                String *const testCDup = strCat(strNew(), testC);

                strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("uXX"));
                strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
                strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("// Debug test trace not enabled"));
                strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/uXX/build"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/uXX/src/pgbackrest"));
                strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("// No timezone specified"));

                strReplace(
                    testCDup,
                    STRDEF("{[C_INCLUDE]}"),
                    STRDEF(
                        "#include \"test/src/common/harnessShim.c\"\n"
                        "#include \"test/src/common/harnessError.c\"\n"
                        "#include \"../../../repo/test/src/common/include.c\"\n"
                        "#include \"../../../repo/doc/src/command/build/build.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_INCLUDE]}"), STRDEF("#include \"../../../repo/test/src/module/test/shimTest.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_LIST]}"),
                    STRDEF(
                        "hrnAdd(  1,     true);"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(testCDup));
            }
            else
                THROW_FMT(TestError, "no test for '%s'", strZ(file));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Run test/shim and build again to cleanup coverage data");

        HRN_SYSTEM(TEST_PATH "/test/unit-3/uXX/build/test-unit");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("test/shim"), 0, 1, logLevelDebug, true, NULL, true, true, true, true),
            "new build");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test real/all");

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/src/module/real/allTest.c",
            "static void\n"
            "testRun(void)\n"
            "{\n"
            "    FUNCTION_HARNESS_VOID();\n"
            "\n"
            "    if (testBegin(\"all\"))\n"
            "    {\n"
            "    }\n"
            "\n"
            "    FUNCTION_HARNESS_RETURN_VOID();\n"
            "}\n");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("real/all"), 0, 1, logLevelDebug, true, STRDEF("America/New_York"), false, true, false, true),
            "new build");

        storageUnit = storagePosixNewP(STRDEF(TEST_PATH "/test/unit-3/none"));
        fileList = testStorageList(storageUnit);

        TEST_RESULT_STRLST_Z(
            fileList,
            "meson.build\n"
            "meson_options.txt\n"
            "test/src/common/harnessError.c\n"
            "test/src/common/harnessShim.c\n"
            "test/src/common/shim.c\n"
            "test.c\n",
            "check files");

        for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
        {
            const String *const file = strLstGet(fileList, fileIdx);

            if (strEqZ(file, "meson.build"))
            {
                TEST_STORAGE_GET(
                    storageUnit, strZ(file),
                    zNewFmt(
                        "%s"
                        "add_global_arguments('-DHRN_FEATURE_ERROR', language : 'c')\n"
                        "add_global_arguments('-DHRN_FEATURE_STACKTRACE', language : 'c')\n"
                        "\n"
                        MESON_COMMENT_BLOCK "\n"
                        "# Unit test\n"
                        MESON_COMMENT_BLOCK "\n"
                        "src_unit = files(\n"
                        "    '../../../repo/src/common/stackTrace.c',\n"
                        "    '../../../repo/src/common/type/stringStatic.c',\n"
                        "    '../../../repo/src/common/debug.c',\n"
                        "    'test/src/common/harnessError.c',\n"
                        "    '../../../repo/test/src/common/harnessNoShim.c',\n"
                        "    'test/src/common/harnessShim.c',\n"
                        "    '../../../repo/test/src/common/harnessTest.c',\n"
                        "    'test.c',\n"
                        ")\n"
                        "\n"
                        "executable(\n"
                        "    'test-unit',\n"
                        "    sources: src_unit,\n"
                        "    c_args: [\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    link_args: [\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    include_directories:\n"
                        "        include_directories(\n"
                        "            '.',\n"
                        "            '../../../repo/src',\n"
                        "            '../../../repo/doc/src',\n"
                        "            '../../../repo/test/src',\n"
                        "        ),\n"
                        "    dependencies: [\n"
                        "        lib_backtrace,\n"
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
                        ")\n",
                        strZ(mesonBuildRoot)));
            }
            else if (strEqZ(file, "meson_options.txt"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), mesonOption);
            }
            else if (strEqZ(file, "src/common/stackTrace.c") || strEqZ(file, "test/src/common/harnessStackTrace.c"))
            {
                // No test needed
            }
            else if (strEqZ(file, "test/src/common/harnessError.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessErrorC));
            }
            else if (strEqZ(file, "test/src/common/harnessShim.c"))
            {
                String *const harnessShimCInt = strCat(strNew(), harnessShimC);
                strReplace(harnessShimCInt, STRDEF("uXX"), STRDEF("none"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessShimCInt));
            }
            else if (strEqZ(file, "test/src/common/shim.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(shimC));
            }
            else if (strEqZ(file, "test.c"))
            {
                String *const testCDup = strCat(strNew(), testC);

                strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("uXX"));
                strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
                strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("// Debug test trace not enabled"));
                strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/none/build"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/uXX/src/pgbackrest"));
                strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("hrnTzSet(\"America/New_York\");"));

                strReplace(testCDup, STRDEF("{[C_INCLUDE]}"), STRDEF(""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_INCLUDE]}"), STRDEF("#include \"../../../repo/test/src/module/real/allTest.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_LIST]}"),
                    STRDEF(
                        "hrnAdd(  1,     true);"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(testCDup));
            }
            else
                THROW_FMT(TestError, "no test for '%s'", strZ(file));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test performance/type");

        strReplace(
            mesonBuildRoot, STRDEF("    configuration.set('HAVE_LIBBACKTRACE'"),
            STRDEF("#    configuration.set('HAVE_LIBBACKTRACE'"));

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/src/module/performance/typeTest.c",
            "static void\n"
            "testRun(void)\n"
            "{\n"
            "    FUNCTION_HARNESS_VOID();\n"
            "\n"
            "    if (testBegin(\"type\"))\n"
            "    {\n"
            "    }\n"
            "\n"
            "    FUNCTION_HARNESS_RETURN_VOID();\n"
            "}\n");

        HRN_SYSTEM("chmod 000 " TEST_PATH "/test/unit-3/uXX/build");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("performance/type"), 0, 1, logLevelDebug, true, STRDEF("America/New_York"), false, true, false, false),
            "new build");

        TEST_RESULT_LOG(
            "P00   WARN: build failed for unit performance/type -- will retry: unable to list file info for path '" TEST_PATH
            "/test/unit-3/uXX/build': [13] Permission denied");

        storageUnit = storagePosixNewP(STRDEF(TEST_PATH "/test/unit-3/uXX"));
        fileList = testStorageList(storageUnit);

        TEST_RESULT_STRLST_Z(
            fileList,
            "meson.build\n"
            "meson_options.txt\n"
            "src/common/stackTrace.c\n"
            "test/src/common/harnessError.c\n"
            "test/src/common/harnessShim.c\n"
            "test/src/common/harnessStackTrace.c\n"
            "test/src/common/shim.c\n"
            "test.c\n",
            "check files");

        for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
        {
            const String *const file = strLstGet(fileList, fileIdx);

            if (strEqZ(file, "meson.build"))
            {
                TEST_STORAGE_GET(
                    storageUnit, strZ(file),
                    zNewFmt(
                        "%s"
                        "add_global_arguments('-DHRN_FEATURE_ERROR', language : 'c')\n"
                        "add_global_arguments('-DHRN_FEATURE_STACKTRACE', language : 'c')\n"
                        "add_global_arguments('-DTEST_CONTAINER_REQUIRED', language : 'c')\n"
                        "\n"
                        MESON_COMMENT_BLOCK "\n"
                        "# Unit test\n"
                        MESON_COMMENT_BLOCK "\n"
                        "src_unit = files(\n"
                        "    '../../../repo/src/common/type/stringStatic.c',\n"
                        "    '../../../repo/src/common/debug.c',\n"
                        "    'test/src/common/harnessError.c',\n"
                        "    'test/src/common/harnessStackTrace.c',\n"
                        "    '../../../repo/test/src/common/harnessNoShim.c',\n"
                        "    '../../../repo/test/src/common/harnessShim/sub.c',\n"
                        "    'test/src/common/harnessShim.c',\n"
                        "    '../../../repo/test/src/common/harnessTest.c',\n"
                        "    'test.c',\n"
                        ")\n"
                        "\n"
                        "executable(\n"
                        "    'test-unit',\n"
                        "    sources: src_unit,\n"
                        "    c_args: [\n"
                        "        '-O2',\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    link_args: [\n"
                        "        '-pg',\n"
                        "        '-no-pie',\n"
                        "    ],\n"
                        "    include_directories:\n"
                        "        include_directories(\n"
                        "            '.',\n"
                        "            '../../../repo/src',\n"
                        "            '../../../repo/doc/src',\n"
                        "            '../../../repo/test/src',\n"
                        "        ),\n"
                        "    dependencies: [\n"
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
                        ")\n",
                        strZ(mesonBuildRoot)));
            }
            else if (strEqZ(file, "meson_options.txt"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), mesonOption);
            }
            else if (strEqZ(file, "src/common/stackTrace.c") || strEqZ(file, "test/src/common/harnessStackTrace.c"))
            {
                // No test needed
            }
            else if (strEqZ(file, "test/src/common/harnessError.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessErrorC));
            }
            else if (strEqZ(file, "test/src/common/harnessShim.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(harnessShimC));
            }
            else if (strEqZ(file, "test/src/common/shim.c"))
            {
                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(shimC));
            }
            else if (strEqZ(file, "test.c"))
            {
                String *const testCDup = strCat(strNew(), testC);

                strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("uXX"));
                strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
                strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("false"));
                strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("// Debug test trace not enabled"));
                strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/uXX/build"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("true"));
                strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/uXX/src/pgbackrest"));
                strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("hrnTzSet(\"America/New_York\");"));

                strReplace(testCDup, STRDEF("{[C_INCLUDE]}"), STRDEF(""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_INCLUDE]}"),
                    STRDEF("#include \"../../../repo/test/src/module/performance/typeTest.c\""));
                strReplace(
                    testCDup, STRDEF("{[C_TEST_LIST]}"),
                    STRDEF(
                        "hrnAdd(  1,     true);"));

                TEST_STORAGE_GET(storageUnit, strZ(file), strZ(testCDup));
            }
            else
                THROW_FMT(TestError, "no test for '%s'", strZ(file));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Test performance/type with profiling off for coverage");

        TEST_RESULT_VOID(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("performance/type"), 0, 1, logLevelDebug, true, STRDEF("America/New_York"), false, false, false, false),
            "new build");

        // Older versions of ninja may error on a rebuild so a retry may occur
        TEST_RESULT_LOG_EMPTY_OR_CONTAINS("WARN: build failed for unit");

        storageUnit = storagePosixNewP(STRDEF(TEST_PATH "/test/unit-3/uXX"));

        // Test one file to make sure profiling was not enabled
        String *const testCDup = strCat(strNew(), testC);

        strReplace(testCDup, STRDEF("{[C_TEST_CONTAINER]}"), STRDEF("true"));
        strReplace(testCDup, STRDEF("{[C_TEST_VM]}"), STRDEF("uXX"));
        strReplace(testCDup, STRDEF("{[C_TEST_PG_VERSION]}"), STRDEF("invalid"));
        strReplace(testCDup, STRDEF("{[C_TEST_LOG_EXPECT]}"), STRDEF("false"));
        strReplace(testCDup, STRDEF("{[C_TEST_DEBUG_TEST_TRACE]}"), STRDEF("// Debug test trace not enabled"));
        strReplace(testCDup, STRDEF("{[C_TEST_PATH_BUILD]}"), STRDEF(TEST_PATH "/test/unit-3/uXX/build"));
        strReplace(testCDup, STRDEF("{[C_TEST_PROFILE]}"), STRDEF("false"));
        strReplace(testCDup, STRDEF("{[C_TEST_PROJECT_EXE]}"), STRDEF(TEST_PATH "/test/build/uXX/src/pgbackrest"));
        strReplace(testCDup, STRDEF("{[C_TEST_TZ]}"), STRDEF("hrnTzSet(\"America/New_York\");"));

        strReplace(testCDup, STRDEF("{[C_INCLUDE]}"), STRDEF(""));
        strReplace(
            testCDup, STRDEF("{[C_TEST_INCLUDE]}"),
            STRDEF("#include \"../../../repo/test/src/module/performance/typeTest.c\""));
        strReplace(
            testCDup, STRDEF("{[C_TEST_LIST]}"),
            STRDEF(
                "hrnAdd(  1,     true);"));

        TEST_STORAGE_GET(storageUnit, "test.c", strZ(testCDup));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Fatal test error");

        HRN_SYSTEM("chmod 000 " TEST_PATH "/repo/meson.build");

        TEST_ERROR(
            cmdTest(
                STRDEF(TEST_PATH "/repo"), storagePathP(storageTest, STRDEF("test")), STRDEF("uXX"), 3, STRDEF("invalid"),
                STRDEF("performance/type"), 0, 1, logLevelDebug, true, STRDEF("America/New_York"), false, false, false, false),
            FileOpenError,
            "build failed for unit performance/type: unable to open file '" TEST_PATH "/repo/meson.build' for read: [13] Permission"
            " denied");

        TEST_RESULT_LOG(
            "P00   WARN: build failed for unit performance/type -- will retry: unable to open file '" TEST_PATH "/repo/meson.build'"
            " for read: [13] Permission denied");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
