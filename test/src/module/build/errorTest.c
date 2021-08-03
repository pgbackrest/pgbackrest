/***********************************************************************************************************************************
Test Build Error
***********************************************************************************************************************************/
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("bldErrParse() and bldErrRender()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse errors");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/error/error.yaml",
            "assert: 24\n");

        TEST_ERROR(bldErrParse(storageTest), FormatError, "error 'assert' code must be >= 25 and <= 125");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/error/error.yaml",
            "assert: 126\n");

        TEST_ERROR(bldErrParse(storageTest), FormatError, "error 'assert' code must be >= 25 and <= 125");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and render error");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/error/error.yaml",
            "assert: 25\n"
            "option-invalid: 31\n"
            "runtime: 122\n");

        TEST_RESULT_VOID(bldErrRender(storageTest, bldErrParse(storageTest)), "parse and render");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check error.auto.h");

        TEST_STORAGE_GET(
            storageTest,
            "src/common/error.auto.h",
            COMMENT_BLOCK_BEGIN "\n"
            "Error Type Definition\n"
            "\n"
            "Automatically generated by 'make build-error' -- do not modify directly.\n"
            COMMENT_BLOCK_END "\n"
            "#ifndef COMMON_ERROR_AUTO_H\n"
            "#define COMMON_ERROR_AUTO_H\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Error type declarations\n"
            COMMENT_BLOCK_END "\n"
            "ERROR_DECLARE(AssertError);\n"
            "ERROR_DECLARE(OptionInvalidError);\n"
            "ERROR_DECLARE(RuntimeError);\n"
            "\n"
            "#endif\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check error.auto.c");

        TEST_STORAGE_GET(
            storageTest,
            "src/common/error.auto.c",
            COMMENT_BLOCK_BEGIN "\n"
            "Error Type Definition\n"
            "\n"
            "Automatically generated by 'make build-error' -- do not modify directly.\n"
            COMMENT_BLOCK_END "\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Error type definitions\n"
            COMMENT_BLOCK_END "\n"
            "ERROR_DEFINE( 25, AssertError, RuntimeError);\n"
            "ERROR_DEFINE( 31, OptionInvalidError, RuntimeError);\n"
            "ERROR_DEFINE(122, RuntimeError, RuntimeError);\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Error type array\n"
            COMMENT_BLOCK_END "\n"
            "static const ErrorType *errorTypeList[] =\n"
            "{\n"
            "    &AssertError,\n"
            "    &OptionInvalidError,\n"
            "    &RuntimeError,\n"
            "    NULL,\n"
            "};\n");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
