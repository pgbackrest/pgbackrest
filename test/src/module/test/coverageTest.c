/***********************************************************************************************************************************
Test Coverage Testing and Reporting
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    const Storage *const storageTest = storagePosixNewP(STRDEF(TEST_PATH), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("testCvgGenerate()"))
    {
        const String *const pathRepo = STRDEF(TEST_PATH "/repo");
        const String *const pathTest = STRDEF(TEST_PATH "/test");

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/define.yaml",
            "unit:\n"
            "  - name: common\n"
            "    test:\n"
            "      - name: none\n"
            "        total: 1\n"
            "\n"
            "      - name: error\n"
            "        total: 1\n"
            "        coverage:\n"
            "          - common/error/sub/error.vendor: included\n"
            "          - doc/common/error/error: included\n"
            "          - test/common/error/error\n"
            "          - test/noCode: noCode\n"
            "\n"
            "      - name: log\n"
            "        total: 1\n"
            "        coverage:\n"
            "          - common/log\n"
            "          - common/log2\n"
            "          - test/common/error/error\n"
            "\n"
            "integration:\n"
            "  - name: integration\n"
            "    test:\n"
            "      - name: all\n"
            "        total: 2\n"
            "\n"
            "performance:\n"
            "  - name: performance\n"
            "    test:\n"
            "      - name: type\n"
            "        total: 1\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "test/repo/test/src/common/error/error.c",
            /* 01 */ "FN_EXTERN const ErrorType *\n"
            /* 02 */ "errorCodeIsError(const int code)\n"
            /* 03 */ "{\n"
            /* 04 */ "    if (code == 0)\n"
            /* 05 */ "        return false;\n"
            /* 06 */ "\n"
            /* 07 */ "    return true;\n"
            /* 08 */ "}\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "test/repo/doc/src/common/error/error.c",
            /* 01 */ "int\n"
            /* 02 */ "returnCode(int code)\n"
            /* 03 */ "{\n"
            /* 04 */ "    return code;\n"
            /* 05 */ "}\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "test/repo/src/common/error/sub/error.vendor.c.inc",
            /* 01 */ "int\n"
            /* 02 */ "returnCode(int code)\n"
            /* 03 */ "{\n"
            /* 04 */ "    return code;\n"
            /* 05 */ "}\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/result/coverage/raw/common-type-error.json",
            "{"
            // {uncrustify_off - indentation}
                "\"gcc_version\": \"11.4.0\","
                "\"files\": ["
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"branches\": ["
                                    "{"
                                        "\"count\": 1"
                                    "},"
                                    "{"
                                        "\"count\": 0"
                                    "}"
                                "],"
                                "\"count\": 1,"
                                "\"line_number\": 4"
                            "},"
                            "{"
                                "\"count\": 1,"
                                "\"line_number\": 5"
                            "},"
                            "{"
                                "\"count\": 0,"
                                "\"line_number\": 7"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"start_line\": 2,"
                                "\"name\": \"errorCodeIsError\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 8"
                            "}"
                        "],"
                        "\"file\": \"../../../repo/test/src/common/error/error.c\""
                    "},"
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"count\": 1,"
                                "\"line_number\": 4"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"start_line\": 2,"
                                "\"name\": \"returnCode\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 5"
                            "}"
                        "],"
                        "\"file\": \"../../../repo/test/src/common/error/sub/error.vendor.c.inc\""
                    "},"
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"count\": 1,"
                                "\"line_number\": 4"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"start_line\": 2,"
                                "\"name\": \"returnCode\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 5"
                            "}"
                        "],"
                        "\"file\": \"../../../repo/doc/src/common/error/error.c\""
                    "}"
                "]"
            // {uncrustify_on}
            "}");

        HRN_STORAGE_PUT_Z(
            storageTest, "test/repo/src/common/log.c",
            /* 01 */ "static void\n"
            /* 02 */ "logAnySet(void)\n"
            /* 03 */ "{\n"
            /* 04 */ "    FUNCTION_TEST_VOID();\n"
            /* 05 */ "\n"
            /* 06 */ "    logLevelAny = logLevelStdOut;\n"
            /* 07 */ "\n"
            /* 08 */ "    if (logLevelStdErr > logLevelAny)\n"
            /* 09 */ "        FUNCTION_LOG_RETURN_VOID();\n"
            /* 10 */ "\n"
            /* 11 */ "    if (logLevelFile > logLevelAny && logFdFile != -1)\n"
            /* 12 */ "        logLevelAny = logLevelFile;\n"
            /* 13 */ "\n"
            /* 14 */ "    FUNCTION_TEST_RETURN_VOID();\n"
            /* 15 */ "}\n"
            /* 16 */ "\n"
            /* 17 */ "FN_EXTERN bool\n"
            /* 18 */ "logAny(const LogLevel logLevel)\n"
            /* 19 */ "{\n"
            /* 20 */ "    FUNCTION_TEST_BEGIN();\n"
            /* 21 */ "        FUNCTION_TEST_PARAM(ENUM, logLevel);\n"
            /* 22 */ "    FUNCTION_TEST_END();\n"
            /* 23 */ "\n"
            /* 24 */ "    ASSERT(logLevel);\n"
            /* 25 */ "\n"
            /* 26 */ "    FUNCTION_TEST_RETURN(BOOL, logLevel); // {uncovered - x}\n"
            /* 27 */ "    x = 1;\n"
            /* 28 */ "\n"
            /* 29 */ "{\n"
            /* 30 */ "}\n"
            /* 31 */ "\n"
            /* 32 */ "\n"
            /* 33 */ "\n"
            /* 34 */ "\n"
            /* 35 */ "{\n"
            /* 36 */ "}\n"
            /* 37 */ "\n"
            /* 38 */ "    x = 2;\n"
            /* 39 */ "}\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "test/repo/src/common/log2.c",
            /* 01 */ "int\n"
            /* 02 */ "returnCode(int code)\n"
            /* 03 */ "{\n"
            /* 04 */ "    if (code == 0)\n"
            /* 05 */ "        return code;\n"
            /* 06 */ "}\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "repo/test/result/coverage/raw/common-log.json",
            "{"
            // {uncrustify_off - indentation}
                "\"gcc_version\": \"11.4.0\","
                "\"files\": ["
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"branches\": ["
                                    "{"
                                        "\"count\": 0"
                                    "},"
                                    "{"
                                        "\"count\": 1"
                                    "}"
                                "],"
                                "\"count\": 1,"
                                "\"line_number\": 4"
                            "},"
                            "{"
                                "\"count\": 0,"
                                "\"line_number\": 5"
                            "},"
                            "{"
                                "\"count\": 1,"
                                "\"line_number\": 7"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"start_line\": 2,"
                                "\"name\": \"errorCodeIsError\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 8"
                            "}"
                        "],"
                        "\"file\": \"../../../repo/test/src/common/error/error.c\""
                    "},"
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"branches\": [],"
                                "\"count\": 0,"
                                "\"line_number\": 4"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 1,"
                                "\"line_number\": 6"
                            "},"
                            "{"
                                "\"branches\": ["
                                    "{"
                                        "\"fallthrough\": true,"
                                        "\"count\": 1"
                                    "},"
                                    "{"
                                        "\"count\": 0"
                                    "}"
                                "],"
                                "\"count\": 7,"
                                "\"junk\": 0,"
                                "\"line_number\": 8"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 1,"
                                "\"line_number\": 9"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 1,"
                                "\"line_number\": 14"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 1,"
                                "\"line_number\": 21"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 0,"
                                "\"line_number\": 22"
                            "},"
                            "{"
                                "\"branches\": ["
                                    "{"
                                        "\"count\": 1"
                                    "},"
                                    "{"
                                        "\"count\": 0"
                                    "}"
                                "],"
                                "\"count\": 1,"
                                "\"line_number\": 24"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 0,"
                                "\"line_number\": 26"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 0,"
                                "\"line_number\": 27"
                            "},"
                            "{"
                                "\"branches\": [],"
                                "\"count\": 0,"
                                "\"line_number\": 38"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"blocks\": 18,"
                                "\"start_line\": 2,"
                                "\"name\": \"logAnySet\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 15"
                            "},"
                            "{"
                                "\"blocks\": 18,"
                                "\"start_line\": 18,"
                                "\"name\": \"logAny\","
                                "\"execution_count\": 0,"
                                "\"end_line\": 33"
                            "}"
                        "],"
                        "\"junk\": 0,"
                        "\"file\": \"../../../repo/src/common/log.c\""
                    "},"
                    "{"
                        "\"lines\": ["
                            "{"
                                "\"branches\": ["
                                    "{"
                                        "\"count\": 0"
                                    "},"
                                    "{"
                                        "\"count\": 1"
                                    "}"
                                "],"
                                "\"count\": 1,"
                                "\"line_number\": 4"
                            "},"
                            "{"
                                "\"count\": 1,"
                                "\"line_number\": 5"
                            "}"
                        "],"
                        "\"functions\": ["
                            "{"
                                "\"start_line\": 2,"
                                "\"name\": \"returnCode\","
                                "\"execution_count\": 1,"
                                "\"end_line\": 6"
                            "}"
                        "],"
                        "\"file\": \"../../../repo/test/src/common/log2.c\""
                    "}"
                "]"
            // {uncrustify_on}
            "}");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("common/none");

        StringList *moduleList = strLstNew();
        strLstAddZ(moduleList, "common/none");

        TEST_RESULT_INT(testCvgGenerate(pathRepo, pathTest, STRDEF("none"), false, moduleList), 0, "generate");

        TEST_STORAGE_GET(
            storageTest, "repo/test/result/coverage/coverage.html",
            TEST_CVG_HTML_PRE
            TEST_CVG_HTML_TOC_PRE
            TEST_CVG_HTML_TOC_POST
            TEST_CVG_HTML_POST);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("common/error");

        moduleList = strLstNew();
        strLstAddZ(moduleList, "common/error");

        TEST_RESULT_INT(testCvgGenerate(pathRepo, pathTest, STRDEF("vm"), false, moduleList), 0, "generate");
        TEST_RESULT_LOG("P00   WARN: module 'test/common/error/error' did not have all tests run required for coverage");

        TEST_STORAGE_GET(
            storageTest, "repo/test/result/coverage/coverage.html",
            TEST_CVG_HTML_PRE
            TEST_CVG_HTML_TOC_PRE
            TEST_CVG_HTML_TOC_COVERED_PRE "doc/src/common/error/error.c" TEST_CVG_HTML_TOC_COVERED_POST
            TEST_CVG_HTML_TOC_COVERED_PRE "src/common/error/sub/error.vendor.c.inc" TEST_CVG_HTML_TOC_COVERED_POST
            TEST_CVG_HTML_TOC_POST
            TEST_CVG_HTML_POST);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("common/error and common/log summary");

        moduleList = strLstNew();
        strLstAddZ(moduleList, "common/error");
        strLstAddZ(moduleList, "common/log");

        TEST_RESULT_INT(testCvgGenerate(pathRepo, pathTest, STRDEF("vm"), true, moduleList), 2, "generate");
        TEST_RESULT_LOG(
            "P00   WARN: module 'src/common/log.c' is not fully covered (4/9 lines, 1/2 branches)\n"
            "P00   WARN: module 'src/common/log2.c' is not fully covered (2/2 lines, 1/2 branches)");

        TEST_STORAGE_GET(
            storageTest, "repo/doc/xml/auto/metric-coverage-report.auto.xml",
            "<table-row>\n"
            "    <table-cell>common</table-cell>\n"
            "    <table-cell>2/3 (66.67%)</table-cell>\n"
            "    <table-cell>2/4 (50.00%)</table-cell>\n"
            "    <table-cell>6/11 (54.55%)</table-cell>\n"
            "</table-row>\n"
            "\n"
            "<table-row>\n"
            "    <table-cell>common/error/sub</table-cell>\n"
            "    <table-cell>1/1 (100.0%)</table-cell>\n"
            "    <table-cell>---</table-cell>\n"
            "    <table-cell>1/1 (100.0%)</table-cell>\n"
            "</table-row>\n"
            "\n"
            "<table-row>\n"
            "    <table-cell>TOTAL</table-cell>\n"
            "    <table-cell>3/4 (75.00%)</table-cell>\n"
            "    <table-cell>2/4 (50.00%)</table-cell>\n"
            "    <table-cell>7/12 (58.33%)</table-cell>\n"
            "</table-row>\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("common/error and common/log");

        TEST_RESULT_INT(testCvgGenerate(pathRepo, pathTest, STRDEF("vm"), false, moduleList), 2, "generate");
        TEST_RESULT_LOG(
            "P00   WARN: module 'src/common/log.c' is not fully covered (5/9 lines, 1/2 branches)\n"
            "P00   WARN: module 'src/common/log2.c' is not fully covered (2/2 lines, 1/2 branches)");

        TEST_STORAGE_GET(
            storageTest, "repo/test/result/coverage/coverage.html",
            TEST_CVG_HTML_PRE

            TEST_CVG_HTML_TOC_PRE
            TEST_CVG_HTML_TOC_COVERED_PRE "doc/src/common/error/error.c" TEST_CVG_HTML_TOC_COVERED_POST
            TEST_CVG_HTML_TOC_COVERED_PRE "src/common/error/sub/error.vendor.c.inc" TEST_CVG_HTML_TOC_COVERED_POST
            TEST_CVG_HTML_TOC_UNCOVERED_PRE "src/common/log.c" TEST_CVG_HTML_TOC_UNCOVERED_MID "src/common/log.c"
            TEST_CVG_HTML_TOC_UNCOVERED_POST
            TEST_CVG_HTML_TOC_UNCOVERED_PRE "src/common/log2.c" TEST_CVG_HTML_TOC_UNCOVERED_MID "src/common/log2.c"
            TEST_CVG_HTML_TOC_UNCOVERED_POST
            TEST_CVG_HTML_TOC_COVERED_PRE "test/src/common/error/error.c" TEST_CVG_HTML_TOC_COVERED_POST
            TEST_CVG_HTML_TOC_POST

            TEST_CVG_HTML_RPT_PRE "src/common/log.c" TEST_CVG_HTML_RPT_MID1 "src/common/log.c" TEST_CVG_HTML_RPT_MID2
            TEST_CVG_HTML_RPT_LINE_PRE "1" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "static void" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "2" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "logAnySet(void)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "3" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "{" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "4" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE_UNCOVERED "    FUNCTION_TEST_VOID();" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "5" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "6" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "    logLevelAny = logLevelStdOut;" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "7" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "8" TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_PRE "[+ -]" TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_POST
            TEST_CVG_HTML_RPT_CODE "    if (logLevelStdErr > logLevelAny)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "9" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "        FUNCTION_LOG_RETURN_VOID();" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "10" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "11" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "    if (logLevelFile > logLevelAny && logFdFile != -1)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "12" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "        logLevelAny = logLevelFile;" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_SKIP
            TEST_CVG_HTML_RPT_LINE_PRE "17" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "FN_EXTERN bool" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "18" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "logAny(const LogLevel logLevel)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "19" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "{" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "20" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "    FUNCTION_TEST_BEGIN();" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "21" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "        FUNCTION_TEST_PARAM(ENUM, logLevel);" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "22" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE_UNCOVERED "    FUNCTION_TEST_END();" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "23" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "24" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "    ASSERT(logLevel);" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "25" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "26" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "    FUNCTION_TEST_RETURN(BOOL, logLevel); // {uncovered - x}" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "27" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE_UNCOVERED "    x = 1;" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_SKIP
            TEST_CVG_HTML_RPT_LINE_PRE "38" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE_UNCOVERED "    x = 2;" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_POST

            TEST_CVG_HTML_RPT_PRE "src/common/log2.c" TEST_CVG_HTML_RPT_MID1 "src/common/log2.c" TEST_CVG_HTML_RPT_MID2
            TEST_CVG_HTML_RPT_LINE_PRE "1" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "int" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "2" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "returnCode(int code)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "3" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "{" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "4" TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_PRE "[- +]" TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_POST
            TEST_CVG_HTML_RPT_CODE "    if (code == 0)" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_LINE_PRE "5" TEST_CVG_HTML_RPT_BRANCH_COVERED
            TEST_CVG_HTML_RPT_CODE "        return code;" TEST_CVG_HTML_RPT_LINE_POST
            TEST_CVG_HTML_RPT_POST

            TEST_CVG_HTML_POST);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
