/***********************************************************************************************************************************
Test Documentation Build
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

    // Create default storage object for testing
    const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("cmdBuild()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("referenceManReplace()");

        TEST_ERROR(referenceManReplace(strNewZ("TEST {[")), AssertError, "unreplaced variable(s) in: TEST {[");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("referenceCommandSection()");

        TEST_RESULT_STR_Z(referenceCommandSection(NULL), "general", "null section remap");

        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("general")), "general", "general section no remap");
        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("log")), "log", "log section no remap");
        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("maintainer")), "maintainer", "maintainer section no remap");
        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("repository")), "repository", "repository section no remap");
        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("stanza")), "stanza", "stanza section no remap");

        TEST_RESULT_STR_Z(referenceCommandSection(STRDEF("other")), "command", "other section remap");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and render");

        HRN_STORAGE_PUT_Z(
            storageTest, "doc/xml/index.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
            "<doc title=\"{[project]}\" subtitle=\"Reliable {[postgres]} Backup &amp; Restore\" toc=\"n\">\n"
            "<description>{[project]} is a reliable backup and restore solution for {[postgres]}...</description>\n"
            "</doc>\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/config/config.yaml",
            "command:\n"
            "  backup: {}\n"
            "\n"
            "  check: {}\n"
            "\n"
            "  restore:\n"
            "    internal: true\n"
            "\n"
            "optionGroup:\n"
            "  pg: {}\n"
            "  repo: {}\n"
            "\n"
            "option:\n"
            "  config:\n"
            "    type: string\n"
            "    command:\n"
            "      backup:\n"
            "        internal: true\n"
            "      check: {}\n"
            "\n"
            "  buffer-size:\n"
            "    section: global\n"
            "    beta: true\n"
            "    type: integer\n"
            "    default: 1024\n"
            "    allow-list: [512, 1024, 2048, 4096]\n"
            "\n"
            "  internal:\n"
            "    section: global\n"
            "    internal: true\n"
            "    type: integer\n"
            "    default: 11\n"
            "\n"
            "  secure:\n"
            "    section: global\n"
            "    type: boolean\n"
            "    secure: true\n"
            "    default: false\n"
            "\n"
            "  repo-compress-level:\n"
            "    group: repo\n"
            "    type: integer\n"
            "    default: 9\n"
            "    allow-range: [0, 9]\n"
            "    command:\n"
            "      backup: {}\n"
            "    deprecate:\n"
            "      repo-compress-level: {}\n"
            "\n"
            "  force:\n"
            "    type: boolean\n"
            "    default: true\n"
            "    command:\n"
            "      backup:\n"
            "        default: true\n"
            "    deprecate:\n"
            "      frc: {}\n"
            "\n"
            "  internal-opt:\n"
            "    type: boolean\n"
            "    internal: true\n"
            "    default: true\n"
            "    command:\n"
            "      backup: {}\n"
            "\n"
            "  stanza:\n"
            "    type: string\n"
            "    default: demo\n"
            "    required: false\n"
            "    deprecate:\n"
            "      stanza: {}\n"
            "      stanza1: {}\n"
            "      stanza2: {}\n"
            "    command:\n"
            "      backup:\n"
            "        command-role:\n"
            "          local: {}\n"
            "          remote: {}\n"
            "      check: {}\n"
            "\n");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/help/help.xml",
            "<doc>\n"
            "    <config title=\"Config Title\">\n"
            "       <description>config description</description>"
            "\n"
            "       <text><p>config text</p></text>"
            "\n"
            "       <config-section-list>\n"
            "           <config-section id=\"general\" name=\"General\">\n"
            "               <text><p>general section</p></text>"
            "\n"
            "               <config-key-list>\n"
            "                   <config-key id=\"buffer-size\" name=\"Buffer Size\">\n"
            "                       <summary>Buffer size option summary.</summary>\n"
            "                       <text><p>Buffer size option description.</p></text>\n"
            "                       <example>128KiB</example>\n"
            "                       <example>256KiB</example>\n"
            "                   </config-key>\n"
            "\n"
            "                   <config-key id=\"internal\" name=\"Internal\">\n"
            "                       <summary>Internal option summary.</summary>\n"
            "                       <text><p>Internal option description</p></text>\n"
            "                   </config-key>\n"
            "\n"
            "                   <config-key id=\"secure\" name=\"Secure\">\n"
            "                       <summary>Secure option summary.</summary>\n"
            "                       <text><p>Secure option description</p></text>\n"
            "                   </config-key>\n"
            "\n"
            "                   <config-key id=\"stanza\" section=\"stanza\" name=\"Stanza\">\n"
            "                       <summary>Stanza option summary.</summary>\n"
            "                       <text><p>Stanza option description</p></text>\n"
            "                   </config-key>\n"
            "               </config-key-list>\n"
            "           </config-section>\n"
            "       </config-section-list>\n"
            "    </config>\n"
            "\n"
            "    <operation title=\"Command Title\">\n"
            "       <description>command description</description>"
            "       <text><p>command text</p></text>"
            "\n"
            "        <operation-general>\n"
            "           <option-list>\n"
            "               <option id=\"config\" name=\"Config\">\n"
            "                   <summary>config option summary.</summary>\n"
            "                   <text><p>config option description.</p></text>\n"
            "               </option>\n"
            "           </option-list>\n"
            "        </operation-general>\n"
            "\n"
            "       <command-list>\n"
            "           <command id=\"backup\" name=\"Backup\">\n"
            "               <summary>backup command summary.</summary>\n"
            "               <text><p>Backup command description.</p></text>\n"
            "\n"
            "               <option-list>\n"
            "                   <option id=\"force\" name=\"Force Backup\">\n"
            "                       <summary>Force option command backup summary.</summary>\n"
            "                       <text><p>Force option command backup description.</p></text>\n"
            "                       <example>n</example>"
            "                       <example>y</example>"
            "                   </option>\n"
            "\n"
            "                   <option id=\"internal-opt\" name=\"Internal Opt\">\n"
            "                       <summary>Internal option command backup summary.</summary>\n"
            "                       <text><p>internal option command backup description.</p></text>\n"
            "                       <example>n</example>"
            "                       <example>y</example>"
            "                   </option>\n"
            "\n"
            "                   <option id=\"repo-compress-level\" name=\"Repo Compress Level Backup\">\n"
            "                       <summary>Repo compress level option command backup summary.</summary>\n"
            "                       <text><p>Repo compress level option command backup description.</p></text>\n"
            "                       <example>4</example>"
            "                   </option>\n"
            "               </option-list>\n"
            "           </command>\n"
            "\n"
            "           <command id=\"check\" name=\"Check\">\n"
            "               <summary>Check command summary.</summary>\n"
            "               <text><p>Check command description.</p></text>\n"
            "           </command>\n"
            "\n"
            "           <command id=\"restore\" name=\"Restore\">\n"
            "               <summary>Restore command summary.</summary>\n"
            "               <text><p>Restore command description.</p></text>\n"
            "           </command>\n"
            "       </command-list>\n"
            "    </operation>\n"
            "</doc>\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("build documentation");

        TEST_RESULT_VOID(cmdBuild(TEST_PATH_STR), "write files");
        TEST_STORAGE_EXISTS(storageTest, "doc/output/xml/configuration.xml");
        TEST_STORAGE_EXISTS(storageTest, "doc/output/xml/command.xml");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("configuration.xml");

        TEST_STORAGE_GET(
            storageTest,
            "doc/output/xml/configuration.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
            "<doc title=\"{[project]}\" subtitle=\"Config Title\" toc=\"y\">"
            // {uncrustify_off - indentation}
                "<description>config description</description>"
                "<section id=\"introduction\">"
                    "<title>Introduction</title>"
                    "<p>config text</p>"
                "</section>"
                "<section id=\"section-general\">"
                    "<title>General Options</title>"
                    "<p>general section</p>"
                    "<section id=\"option-buffer-size\">"
                        "<title>Buffer Size Option (<id>--buffer-size</id>)</title>"
                        "<p>Buffer size option summary.</p>"
                        "<p>FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.</p>"
                        "<p>Buffer size option description.</p>"
                        "<code-block>default: 1024\n"
                        "example: buffer-size=128KiB\n"
                        "example: buffer-size=256KiB</code-block>"
                    "</section>"
                    "<section id=\"option-secure\">"
                        "<title>Secure Option (<id>--secure</id>)</title>"
                        "<p>Secure option summary.</p>"
                        "<p>Secure option description</p>"
                        "<code-block>default: n</code-block>"
                    "</section>"
                "</section>"
            // {uncrustify_on}
            "</doc>\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command.xml");

        TEST_STORAGE_GET(
            storageTest,
            "doc/output/xml/command.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
            "<doc title=\"{[project]}\" subtitle=\"Command Title\" toc=\"y\">"
            // {uncrustify_off - indentation}
                "<description>command description</description>"
                "<section id=\"introduction\"><title>Introduction</title>"
                    "<p>command text</p>"
                "</section>"
                "<section id=\"command-backup\">"
                    "<title>Backup Command (<id>backup</id>)</title>"
                    "<p>Backup command description.</p>"
                    "<section id=\"category-command\" toc=\"n\">"
                        "<title>Command Options</title>"
                        "<section id=\"option-force\">"
                            "<title>Force Backup Option (<id>--force</id>)</title>"
                            "<p>Force option command backup summary.</p>"
                            "<p>Force option command backup description.</p>"
                            "<code-block>default: y\n"
                            "example: --no-force --force</code-block>"
                            "<p>Deprecated Name: frc</p>"
                        "</section>"
                        "<section id=\"option-repo-compress-level\">"
                            "<title>Repo Compress Level Backup Option (<id>--repo-compress-level</id>)</title>"
                            "<p>Repo compress level option command backup summary.</p>"
                            "<p>Repo compress level option command backup description.</p>"
                            "<code-block>default: 9\n"
                            "allowed: 0-9\n"
                            "example: --repo1-compress-level=4</code-block>"
                        "</section>"
                    "</section>"
                    "<section id=\"category-general\" toc=\"n\">"
                        "<title>General Options</title>"
                        "<section id=\"option-buffer-size\">"
                            "<title>Buffer Size Option (<id>--buffer-size</id>)</title>"
                            "<p>Buffer size option summary.</p>"
                            "<p>FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.</p>"
                            "<p>Buffer size option description.</p>"
                            "<code-block>default: 1024\n"
                            "example: --buffer-size=128KiB --buffer-size=256KiB</code-block>"
                        "</section>"
                    "</section>"
                "</section>"
                "<section id=\"command-check\">"
                    "<title>Check Command (<id>check</id>)</title>"
                    "<p>Check command description.</p>"
                    "<section id=\"category-general\" toc=\"n\">"
                        "<title>General Options</title>"
                        "<section id=\"option-buffer-size\">"
                            "<title>Buffer Size Option (<id>--buffer-size</id>)</title>"
                            "<p>Buffer size option summary.</p>"
                            "<p>FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.</p>"
                            "<p>Buffer size option description.</p>"
                            "<code-block>default: 1024\n"
                            "example: --buffer-size=128KiB --buffer-size=256KiB</code-block>"
                        "</section>"
                        "<section id=\"option-config\">"
                            "<title>Config Option (<id>--config</id>)</title>"
                            "<p>config option summary.</p>"
                            "<p>config option description.</p>"
                        "</section>"
                    "</section>"
                    "<section id=\"category-stanza\" toc=\"n\">"
                        "<title>Stanza Options</title>"
                        "<section id=\"option-stanza\">"
                            "<title>Stanza Option (<id>--stanza</id>)</title>"
                            "<p>Stanza option summary.</p>"
                            "<p>Stanza option description</p>"
                            "<code-block>default: demo</code-block>"
                            "<p>Deprecated Names: stanza1, stanza2</p>"
                        "</section>"
                    "</section>"
                "</section>"
            // {uncrustify_on}
            "</doc>\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pgbackrest.1.txt");

        TEST_STORAGE_GET(
            storageTest,
            "doc/output/man/pgbackrest.1.txt",
            "NAME\n"
            "  pgBackRest - Reliable PostgreSQL Backup & Restore\n"
            "\n"
            "SYNOPSIS\n"
            "  pgbackrest [options] [command]\n"
            "\n"
            "DESCRIPTION\n"
            "  pgBackRest is a reliable backup and restore solution for PostgreSQL...\n"
            "\n"
            "COMMANDS\n"
            "  backup  backup command summary.\n"
            "  check   Check command summary.\n"
            "\n"
            "OPTIONS\n"
            "  Backup Options:\n"
            "    --force                Force option command backup summary.\n"
            "    --repo-compress-level  Repo compress level option command backup summary.\n"
            "\n"
            "  General Options:\n"
            "    --buffer-size          Buffer size option summary.\n"
            "    --config               config option summary.\n"
            "    --secure               Secure option summary.\n"
            "\n"
            "  Stanza Options:\n"
            "    --stanza               Stanza option summary.\n"
            "\n"
            "FILES\n"
            "  /etc/pgbackrest/pgbackrest.conf\n"
            "  /var/lib/pgbackrest\n"
            "  /var/log/pgbackrest\n"
            "  /var/spool/pgbackrest\n"
            "  /tmp/pgbackrest\n"
            "\n"
            "EXAMPLES\n"
            "  * Create a backup of the PostgreSQL `main` cluster:\n"
            "\n"
            "    $ pgbackrest --stanza=main backup\n"
            "\n"
            "    The `main` cluster should be configured in `/etc/pgbackrest/pgbackrest.conf`\n"
            "\n"
            "  * Show all available backups:\n"
            "\n"
            "    $ pgbackrest info\n"
            "\n"
            "  * Show all available backups for a specific cluster:\n"
            "\n"
            "    $ pgbackrest --stanza=main info\n"
            "\n"
            "  * Show backup specific options:\n"
            "\n"
            "    $ pgbackrest help backup\n"
            "\n"
            "SEE ALSO\n"
            "  /usr/share/doc/pgbackrest-doc/html/index.html\n"
            "  http://www.pgbackrest.org\n");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
