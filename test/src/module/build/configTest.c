/***********************************************************************************************************************************
Test Build Config
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
    if (testBegin("bldCfgParse() and bldCfgRender()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command parse errors");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            "command:\n"
            "  archive-get:\n"
            "    bogus: test\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown command definition 'bogus'");

        #define TEST_COMMAND_VALID                                                                                                 \
            "command:\n"                                                                                                           \
            "  archive-get:\n"                                                                                                     \
            "    internal: true\n"                                                                                                 \
            "\n"

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option group parse errors");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            TEST_COMMAND_VALID
            "optionGroup:\n"
            "  repo:\n"
            "    bogus: test\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown option group definition 'bogus'");

        #define TEST_OPTION_GROUP_VALID                                                                                            \
            "optionGroup:\n"                                                                                                       \
            "  repo:\n"                                                                                                            \
            "    prefix: repo\n"                                                                                                   \
            "\n"

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option parse errors");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            TEST_COMMAND_VALID
            TEST_OPTION_GROUP_VALID
            "option:\n"
            "  config:\n"
            "    bogus: test\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown option definition 'bogus'");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            TEST_COMMAND_VALID
            TEST_OPTION_GROUP_VALID
            "option:\n"
            "  config:\n"
            "    depend:\n"
            "      bogus: test\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown depend definition 'bogus'");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            TEST_COMMAND_VALID
            TEST_OPTION_GROUP_VALID
            "option:\n"
            "  config:\n"
            "    deprecate:\n"
            "      old: {bogus: test}\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown deprecate definition 'bogus'");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            TEST_COMMAND_VALID
            TEST_OPTION_GROUP_VALID
            "option:\n"
            "  config:\n"
            "    command:\n"
            "      backup:\n"
            "        bogus: test\n");

        TEST_ERROR(bldCfgParse(storageTest), FormatError, "unknown option command definition 'bogus'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and render config");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            "command:\n"
            "  archive-get:\n"
            "    command-role:\n"
            "      async: {}\n"
            "      local: {}\n"
            "      remote: {}\n"
            "    lock-type: archive\n"
            "    log-file: false\n"
            "    log-level-default: debug\n"
            "    parameter-allowed: true\n"
            "\n"
            "  backup:\n"
            "    internal: true\n"
            "    command-role:\n"
            "      local: {}\n"
            "      remote: {}\n"
            "    lock-type: backup\n"
            "    lock-required: true\n"
            "    lock-remote-required: true\n"
            "\n"
            "  help: {}\n"
            "\n"
            "optionGroup:\n"
            "  pg:\n"
            "    indexTotal: 8\n"
            "    prefix: pg\n"
            "  repo:\n"
            "    indexTotal: 4\n"
            "    prefix: repo\n"
            "\n"
            "option:\n"
            "  compress-type:\n"
            "    section: global\n"
            "    type: string\n"
            "    default: gz\n"
            "    command-role: {}\n"
            "    deprecate:\n"
            "      compress: {}\n"
            "\n"
            "  compress-level:\n"
            "    section: global\n"
            "    type: integer\n"
            "    required: false\n"
            "    allow-range: [0, 9]\n"
            "    command: compress-type\n"
            "    depend: compress-type\n"
            "\n"
            "  compress-level-network:\n"
            "    inherit: compress-level\n"
            "    internal: true\n"
            "    secure: true\n"
            "    default: 3\n"
            "    depend:\n"
            "      option: compress-type\n"
            "      list:\n"
            "        - none\n"
            "        - gz\n"
            "\n"
            "  config:\n"
            "    type: string\n"
            "    default: CFGOPTDEF_CONFIG_PATH \"/\" PROJECT_CONFIG_FILE\n"
            "    default-literal: true\n"
            "    negate: y\n"
            "\n"
            "  log-level-console:\n"
            "    section: global\n"
            "    type: string\n"
            "    default: warn\n"
            "    allow-list:\n"
            "      - off\n"
            "      - error\n"
            "      - warn\n"
            "      - debug1\n"
            "\n"
            "  log-level-file:\n"
            "    section: global\n"
            "    type: string\n"
            "    default: info\n"
            "    allow-list: log-level-console\n"
            "    command:\n"
            "      backup:\n"
            "        internal: true\n"
            "        required: false\n"
            "        default: warn\n"
            "        allow-list:\n"
            "          - off\n"
            "          - warn\n"
            "        depend:\n"
            "          option: log-level-console\n"
            "          list:\n"
            "            - warn\n"
            "        command-role:\n"
            "          main: {}\n"
            "      archive-get: {}\n"
            "\n"
            "  pg-path:\n"
            "    group: pg\n"
            "    type: path\n"
            "    deprecate:\n"
            "      db-path: {index: 1, reset: false}\n"
            "      db?-path: {reset: false}\n");

        TEST_RESULT_VOID(bldCfgRender(storageTest, bldCfgParse(storageTest)), "parse and render");

        TEST_STORAGE_GET(
            storageTest,
            "src/config/config.auto.h",
            COMMENT_BLOCK_BEGIN "\n"
            "Command and Option Configuration\n"
            "\n"
            "Automatically generated by 'make build-config' -- do not modify directly.\n"
            COMMENT_BLOCK_END "\n"
            "#ifndef CONFIG_CONFIG_AUTO_H\n"
            "#define CONFIG_CONFIG_AUTO_H\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Command constants\n"
            COMMENT_BLOCK_END "\n"
            "#define CFGCMD_ARCHIVE_GET                                          \"archive-get\"\n"
            "#define CFGCMD_BACKUP                                               \"backup\"\n"
            "#define CFGCMD_HELP                                                 \"help\"\n"
            "\n"
            "#define CFG_COMMAND_TOTAL                                           3\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Option group constants\n"
            COMMENT_BLOCK_END "\n"
            "#define CFG_OPTION_GROUP_TOTAL                                      2\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Option constants\n"
            COMMENT_BLOCK_END "\n"
            "#define CFGOPT_COMPRESS_LEVEL                                       \"compress-level\"\n"
            "#define CFGOPT_COMPRESS_LEVEL_NETWORK                               \"compress-level-network\"\n"
            "#define CFGOPT_COMPRESS_TYPE                                        \"compress-type\"\n"
            "#define CFGOPT_CONFIG                                               \"config\"\n"
            "#define CFGOPT_LOG_LEVEL_CONSOLE                                    \"log-level-console\"\n"
            "#define CFGOPT_LOG_LEVEL_FILE                                       \"log-level-file\"\n"
            "\n"
            "#define CFG_OPTION_TOTAL                                            7\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Option value constants\n"
            COMMENT_BLOCK_END "\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_DEBUG1                          STRID6(\"debug1\", 0x7475421441)\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_DEBUG1_Z                        \"debug1\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_ERROR                           STRID5(\"error\", 0x127ca450)\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_ERROR_Z                         \"error\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_OFF                             STRID5(\"off\", 0x18cf0)\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_OFF_Z                           \"off\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_WARN                            STRID5(\"warn\", 0x748370)\n"
            "#define CFGOPTVAL_LOG_LEVEL_CONSOLE_WARN_Z                          \"warn\"\n"
            "\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_DEBUG1                             STRID6(\"debug1\", 0x7475421441)\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_DEBUG1_Z                           \"debug1\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_ERROR                              STRID5(\"error\", 0x127ca450)\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_ERROR_Z                            \"error\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_OFF                                STRID5(\"off\", 0x18cf0)\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_OFF_Z                              \"off\"\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_WARN                               STRID5(\"warn\", 0x748370)\n"
            "#define CFGOPTVAL_LOG_LEVEL_FILE_WARN_Z                             \"warn\"\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Command enum\n"
            COMMENT_BLOCK_END "\n"
            "typedef enum\n"
            "{\n"
            "    cfgCmdArchiveGet,\n"
            "    cfgCmdBackup,\n"
            "    cfgCmdHelp,\n"
            "    cfgCmdNone,\n"
            "} ConfigCommand;\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Option group enum\n"
            COMMENT_BLOCK_END "\n"
            "typedef enum\n"
            "{\n"
            "    cfgOptGrpPg,\n"
            "    cfgOptGrpRepo,\n"
            "} ConfigOptionGroup;\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "Option enum\n"
            COMMENT_BLOCK_END "\n"
            "typedef enum\n"
            "{\n"
            "    cfgOptCompressLevel,\n"
            "    cfgOptCompressLevelNetwork,\n"
            "    cfgOptCompressType,\n"
            "    cfgOptConfig,\n"
            "    cfgOptLogLevelConsole,\n"
            "    cfgOptLogLevelFile,\n"
            "    cfgOptPgPath,\n"
            "} ConfigOption;\n"
            "\n"
            "#endif\n"
        );
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
