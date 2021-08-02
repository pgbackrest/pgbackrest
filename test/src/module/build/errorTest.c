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
        // // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("command parse errors");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     "command:\n"
        //     "  archive-get:\n"
        //     "    bogus: test\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown command definition 'bogus'");

        // // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("option parse errors");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    bogus: test\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown option definition 'bogus'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    section: global\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "option 'config' requires 'type'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    depend:\n"
        //     "      bogus: test\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown depend definition 'bogus'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    command:\n"
        //     "      backup:\n"
        //     "        bogus: test\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown option command definition 'bogus'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    depend: bogus\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "dependency inherited from option 'bogus' before it is defined");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "    command:\n"
        //     "      bogus: {}\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "invalid command 'bogus' in option 'config' command list");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "    depend:\n"
        //     "      option: bogus\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "dependency on undefined option 'bogus'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "    group: bogus\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "option 'config' has invalid group 'bogus'");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  stanza:\n"
        //     "    type: string\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "    depend:\n"
        //     "      option: online\n"
        //     "  online:\n"
        //     "    type: boolean\n"
        //     "    depend:\n"
        //     "      option: config\n");

        // TEST_ERROR(
        //     bldCfgParse(storageTest), FormatError,
        //     "unable to resolve dependencies for option(s) 'config, online'\n"
        //     "HINT: are there circular dependencies?");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "option 'stanza' must exist");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "  stanza:\n"
        //     "    type: string\n"
        //     "    depend:\n"
        //     "      option: config\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "option 'stanza' may not depend on other option");

        // HRN_STORAGE_PUT_Z(
        //     storageTest, "src/build/config/config.yaml",
        //     TEST_COMMAND_VALID
        //     TEST_OPTION_GROUP_VALID
        //     "option:\n"
        //     "  config:\n"
        //     "    type: string\n"
        //     "  stanza:\n"
        //     "    type: string\n"
        //     "    command:\n"
        //     "      archive-get:\n"
        //     "        depend:\n"
        //     "          option: config\n");

        // TEST_ERROR(bldCfgParse(storageTest), FormatError, "option 'stanza' command 'archive-get' may not depend on other option");

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
