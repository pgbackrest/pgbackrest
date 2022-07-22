/***********************************************************************************************************************************
Test Build Help
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessPack.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("bldHlpRenderXml()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("format errors");

        TEST_ERROR(
            bldHlpRenderXml(xmlDocumentRoot(xmlDocumentNewBuf(BUFSTRDEF("<doc><bogus/></doc>")))), FormatError,
            "unknown tag 'bogus'");

        TEST_ERROR(
            bldHlpRenderXml(xmlDocumentRoot(xmlDocumentNewBuf(BUFSTRDEF("<doc>bogus\n</doc>")))), FormatError,
            "text 'bogus\n' is invalid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xml to text");

        TEST_RESULT_STR_Z(
            bldHlpRenderXml(xmlDocumentRoot(xmlDocumentNewBuf(BUFSTRDEF(
                "<doc>"
                "<p><backrest/> <postgres/> {[dash]} "
                    "<b><br-option><cmd><code><exe><file><host><i><id><link><path><pg-setting><proper><setting>"
                    "<!-- COMMENT -->"
                    "info"
                    "</setting></proper></pg-setting></path></link></id></i></host></file></exe></code></cmd></br-option></b></p>\n"
                "\n"
                "<admonition>think about it</admonition>\n"
                "\n"
                "<p>List:</p>\n"
                "\n"
                "<list>\n"
                  "<list-item>item1</list-item>\n"
                  "<list-item>item2</list-item>\n"
                "</list>\n"
                "\n"
                "<p>last para</p>"
                "</doc>")))),
            "pgBackRest PostgreSQL - info\n"
            "\n"
            "NOTE: think about it\n"
            "\n"
            "List:\n"
            "\n"
            "* item1\n"
            "* item2\n"
            "\n"
            "last para",
            "render");
    }

    // *****************************************************************************************************************************
    if (testBegin("bldHlpParse() and bldHlpRender()"))
    {
        TEST_TITLE("error on missing command");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            "command:\n"
            "  backup: {}\n"
            "\n"
            "optionGroup:\n"
            "  pg: {}\n"
            "\n"
            "option:\n"
            "  buffer:\n"
            "    section: general\n"
            "    type: size\n"
            "\n"
            "  pg:\n"
            "    type: string\n"
            "    command-role:\n"
            "      local: {}"
            "\n"
            "  stanza:\n"
            "    type: string\n"
            "\n"
            "  xfer:\n"
            "    type: bool\n"
            "\n");

        BldCfg bldCfgErr = bldCfgParse(storageTest);

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/help/help.xml",
            "<doc>\n"
            "    <config>\n"
            "       <config-section-list>\n"
            "       </config-section-list>\n"
            "    </config>\n"
            "\n"
            "    <operation>\n"
            "        <operation-general>\n"
            "           <option-list>\n"
            "           </option-list>\n"
            "        </operation-general>\n"
            "\n"
            "       <command-list>\n"
            "       </command-list>\n"
            "    </operation>\n"
            "</doc>\n");

        TEST_ERROR(bldHlpParse(storageTest, bldCfgErr), FormatError, "command 'backup' must have help");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing config option");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/help/help.xml",
            "<doc>\n"
            "    <config>\n"
            "       <config-section-list>\n"
            "       </config-section-list>\n"
            "    </config>\n"
            "\n"
            "    <operation>\n"
            "        <operation-general>\n"
            "           <option-list>\n"
            "           </option-list>\n"
            "        </operation-general>\n"
            "\n"
            "       <command-list>\n"
            "           <command id=\"backup\" name=\"Backup\">\n"
            "               <summary>Backup.</summary>\n"
            "\n"
            "               <text>\n"
            "                   <p>Backup.</p>\n"
            "               </text>\n"
            "           </command>\n"
            "       </command-list>\n"
            "    </operation>\n"
            "</doc>\n");

        TEST_ERROR(bldHlpParse(storageTest, bldCfgErr), FormatError, "option 'buffer' must have help for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing command-line option");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/help/help.xml",
            "<doc>\n"
            "    <config>\n"
            "       <config-section-list>\n"
            "           <config-section id=\"general\" name=\"General\">\n"
            "               <config-key-list>\n"
            "                   <config-key id=\"buffer\" name=\"Buffer\">\n"
            "                       <summary>Buffer.</summary>\n"
            "\n"
            "                       <text>\n"
            "                           <p>Buffer.</p>\n"
            "                       </text>\n"
            "                   </config-key>\n"
            "               </config-key-list>\n"
            "           </config-section>\n"
            "       </config-section-list>\n"
            "    </config>\n"
            "\n"
            "    <operation>\n"
            "        <operation-general>\n"
            "           <option-list>\n"
            "           </option-list>\n"
            "        </operation-general>\n"
            "\n"
            "       <command-list>\n"
            "           <command id=\"backup\" name=\"Backup\">\n"
            "               <summary>Backup.</summary>\n"
            "\n"
            "               <text>\n"
            "                   <p>Backup.</p>\n"
            "               </text>\n"
            "           </command>\n"
            "       </command-list>\n"
            "    </operation>\n"
            "</doc>\n");

        TEST_ERROR(bldHlpParse(storageTest, bldCfgErr), FormatError, "option 'stanza' must have help for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and render");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            "command:\n"
            "  backup:\n"
            "    command-role:\n"
            "      async: {}\n"
            "      local: {}\n"
            "      remote: {}\n"
            "\n"
            "  check: {}\n"
            "\n"
            "  restore:\n"
            "    internal: true\n"
            "    command-role:\n"
            "      local: {}\n"
            "      remote: {}\n"
            "\n"
            "optionGroup:\n"
            "  pg: {}\n"
            "  repo: {}\n"
            "\n"
            "option:\n"
            "  config:\n"
            "    type: string\n"
            "    required: false\n"
            "    command:\n"
            "      backup:\n"
            "        internal: true\n"
            "      restore: {}\n"
            "\n"
            "  buffer-size:\n"
            "    section: global\n"
            "    type: integer\n"
            "    default: 1024\n"
            "    allow-list: [512, 1024, 2048, 4096]\n"
            "\n"
            "  force:\n"
            "    type: boolean\n"
            "    command:\n"
            "      check: {}\n"
            "      restore: {}\n"
            "\n"
            "  stanza:\n"
            "    type: string\n"
            "    required: false\n"
            "    deprecate:\n"
            "      stanza: {}\n"
            "      stanza1: {}\n"
            "      stanza2: {}\n"
            "\n");

        BldCfg bldCfg = bldCfgParse(storageTest);

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/help/help.xml",
            "<doc>\n"
            "    <config>\n"
            "       <config-section-list>\n"
            "           <config-section id=\"general\" name=\"General\">\n"
            "               <config-key-list>\n"
            "                   <config-key id=\"buffer-size\" name=\"Buffer Size\">\n"
            "                       <summary>Buffer size for file operations.</summary>\n"
            "\n"
            "                       <text>\n"
            "                           <p>Buffer.</p>\n"
            "                       </text>\n"
            "                   </config-key>\n"
            "\n"
            "                   <config-key id=\"stanza\" name=\"Stanza\">\n"
            "                       <summary>Defines the stanza.</summary>\n"
            "\n"
            "                       <text>\n"
            "                           <p>Stanza.</p>\n"
            "                       </text>\n"
            "                   </config-key>\n"
            "               </config-key-list>\n"
            "           </config-section>\n"
            "       </config-section-list>\n"
            "    </config>\n"
            "\n"
            "    <operation>\n"
            "        <operation-general>\n"
            "           <option-list>\n"
            "               <option id=\"config\" section=\"stanza\" name=\"Config\">\n"
            "                   <summary><backrest/> configuration file.</summary>\n"
            "\n"
            "                   <text>\n"
            "                       <p>Use this option to specify a different configuration file than the default.</p>\n"
            "                   </text>\n"
            "               </option>\n"
            "           </option-list>\n"
            "        </operation-general>\n"
            "\n"
            "       <command-list>\n"
            "           <command id=\"backup\" name=\"Backup\">\n"
            "               <summary>Backup cluster.</summary>\n"
            "\n"
            "               <text>\n"
            "                   <p><backrest/> backup.</p>\n"
            "               </text>\n"
            "           </command>\n"
            "\n"
            "           <command id=\"check\" name=\"Check\">\n"
            "               <summary>Check cluster.</summary>\n"
            "\n"
            "               <text>\n"
            "                   <p><backrest/> check.</p>\n"
            "               </text>\n"
            "\n"
            "               <option-list>\n"
            "                   <option id=\"force\" name=\"Force\">\n"
            "                       <summary>Force delete.</summary>\n"
            "\n"
            "                       <text>\n"
            "                           <p>Longer description.</p>\n"
            "                       </text>\n"
            "                   </option>\n"
            "               </option-list>\n"
            "           </command>\n"
            "\n"
            "           <command id=\"restore\" name=\"Restore\">\n"
            "               <summary>Restore cluster.</summary>\n"
            "\n"
            "               <text>\n"
            "                   <p><backrest/> restore.</p>\n"
            "               </text>\n"
            "\n"
            "               <option-list>\n"
            "                   <option id=\"force\" name=\"Force\">\n"
            "                       <summary>Force delete.</summary>\n"
            "\n"
            "                       <text>\n"
            "                           <p>Longer description.</p>\n"
            "                       </text>\n"
            "                   </option>\n"
            "               </option-list>\n"
            "           </command>\n"
            "       </command-list>\n"
            "    </operation>\n"
            "</doc>\n");

        TEST_RESULT_STR_Z(
            hrnPackReadToStr(pckReadNew(pckWriteResult(bldHlpRenderHelpAutoCPack(bldCfg, bldHlpParse(storageTest, bldCfg))))),
            "1:array:"
            "["
                // backup command
                "2:str:Backup cluster."
                ", 3:str:pgBackRest backup."
                // check command
                ", 5:str:Check cluster."
                ", 6:str:pgBackRest check."
                // restore command
                ", 7:bool:true"
                ", 8:str:Restore cluster."
                ", 9:str:pgBackRest restore."
            "]"
            ", 2:array:"
            "["
                // buffer-size option
                "2:str:general"
                ", 3:str:Buffer size for file operations."
                ", 4:str:Buffer."
                // config option
                ", 8:str:stanza"
                ", 9:str:pgBackRest configuration file."
                ", 10:str:Use this option to specify a different configuration file than the default."
                ", 12:array:"
                "["
                    // backup command override
                    "1:obj:"
                    "{"
                        "1:bool:true"
                    "}"
                "]"
                // force option
                ", 18:array:"
                "["
                    // check command override
                    "2:obj:"
                    "{"
                        "2:str:Force delete."
                        ", 3:str:Longer description."
                    "}"
                    // restore command override
                    ", 3:obj:"
                    "{"
                        "2:str:Force delete."
                        ", 3:str:Longer description."
                    "}"
                "]"
                // stanza option
                ", 20:str:general"
                ", 21:str:Defines the stanza."
                ", 22:str:Stanza."
                ", 23:array:"
                "["
                    "1:str:stanza1"
                    ", 2:str:stanza2"
                "]"
            "]",
            "parse and render");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check help file");

        TEST_RESULT_VOID(bldHlpRender(storageTest, bldCfg, bldHlpParse(storageTest, bldCfg)), "write file");
        TEST_STORAGE_EXISTS(storageTest, "src/command/help/help.auto.c.inc");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
