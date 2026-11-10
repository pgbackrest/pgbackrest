/***********************************************************************************************************************************
Test Help Command
***********************************************************************************************************************************/
#include "config/load.h"
#include "config/parse.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"
#include "version.h"

#include "harness/config.h"
#include "harness/storage.h"

/***********************************************************************************************************************************
Configuration load with just enough functionality to test help
***********************************************************************************************************************************/
static void
testCfgLoad(const StringList *const argList)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING_LIST, argList);
    FUNCTION_HARNESS_END();

    // Parse config
    cfgParseP(storageLocal(), strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true);

    // Apply special option rules
    cfgLoadUpdateOption();

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Last blocks of help text, where a block is the text between blank lines
***********************************************************************************************************************************/
static String *
testHelpBlockLast(const String *const help, const unsigned int blockTotal)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, help);
        FUNCTION_HARNESS_PARAM(UINT, blockTotal);
    FUNCTION_HARNESS_END();

    const StringList *const blockList = strLstNewSplitZ(help, "\n\n");
    StringList *const result = strLstNew();

    ASSERT(blockTotal > 0 && blockTotal <= strLstSize(blockList));

    for (unsigned int blockIdx = strLstSize(blockList) - blockTotal; blockIdx < strLstSize(blockList); blockIdx++)
        strLstAdd(result, strLstGet(blockList, blockIdx));

    FUNCTION_HARNESS_RETURN(STRING, strLstJoin(result, "\n\n"));
}

/***********************************************************************************************************************************
First blocks of help text, where a block is the text between blank lines
***********************************************************************************************************************************/
static String *
testHelpBlockFirst(const String *const help, const unsigned int blockTotal)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, help);
        FUNCTION_HARNESS_PARAM(UINT, blockTotal);
    FUNCTION_HARNESS_END();

    const StringList *const blockList = strLstNewSplitZ(help, "\n\n");
    StringList *const result = strLstNew();

    ASSERT(blockTotal > 0 && blockTotal <= strLstSize(blockList));

    for (unsigned int blockIdx = 0; blockIdx < blockTotal; blockIdx++)
        strLstAdd(result, strLstGet(blockList, blockIdx));

    FUNCTION_HARNESS_RETURN(STRING, strLstJoin(result, "\n\n"));
}

/***********************************************************************************************************************************
Section headings of command help
***********************************************************************************************************************************/
static String *
testHelpSectionList(const String *const help)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, help);
    FUNCTION_HARNESS_END();

    const StringList *const lineList = strLstNewSplitZ(help, "\n");
    String *const result = strNew();

    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
    {
        if (strEndsWithZ(strLstGet(lineList, lineIdx), " Options:"))
            strCatFmt(result, "%s\n", strZ(strLstGet(lineList, lineIdx)));
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Entry for an option in command help, i.e. the line with the option name and the lines its summary wraps onto. NULL when the option
is not listed.
***********************************************************************************************************************************/
static String *
testHelpOption(const String *const help, const char *const optionName)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, help);
        FUNCTION_HARNESS_PARAM(STRINGZ, optionName);
    FUNCTION_HARNESS_END();

    const StringList *const lineList = strLstNewSplitZ(help, "\n");
    const String *const optionPrefix = strNewFmt("  --%s ", optionName);
    String *result = NULL;

    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
    {
        const String *const line = strLstGet(lineList, lineIdx);

        // Line with the option name
        if (result == NULL)
        {
            if (strBeginsWith(line, optionPrefix))
                result = strCatFmt(strNew(), "%s\n", strZ(line));
        }
        // Line the summary wraps onto
        else if (strBeginsWithZ(line, "    "))
            strCatFmt(result, "%s\n", strZ(line));
        else
            break;
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Program name a version are used multiple times
    const char *helpVersion = PROJECT_NAME " " PROJECT_VERSION;

    // General help text is used in more than one test
    const char *generalHelp = zNewFmt(
        "%s - General help\n"
        "\n"
        "Usage:\n"
        "    pgbackrest [options] [command]\n"
        "\n"
        "Commands:\n"
        "    annotate        add or modify backup annotation\n"
        "    archive-get     get a WAL segment from the archive\n"
        "    archive-push    push a WAL segment to the archive\n"
        "    backup          backup a database cluster\n"
        "    check           check the configuration\n"
        "    expire          expire backups that exceed retention\n"
        "    help            get help\n"
        "    info            retrieve information about backups\n"
        "    repo-get        get a file from a repository\n"
        "    repo-ls         list files in a repository\n"
        "    restore         restore a database cluster\n"
        "    server          pgBackRest server\n"
        "    server-ping     ping pgBackRest server\n"
        "    stanza-create   create the required stanza data\n"
        "    stanza-delete   delete a stanza\n"
        "    stanza-upgrade  upgrade a stanza\n"
        "    start           allow pgBackRest processes to run\n"
        "    stop            stop pgBackRest processes from running\n"
        "    verify          verify contents of a repository\n"
        "    version         get version\n"
        "\n"
        "Use 'pgbackrest help [command]' for more information.\n",
        helpVersion);

    // *****************************************************************************************************************************
    if (testBegin("helpRenderSplitSize()"))
    {
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF("abc def"), " ", 3), "abc\ndef\n", "two items");
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF("abc def"), " ", 4), "abc\ndef\n", "two items, size > each item");
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF("abc def ghi"), " ", 4), "abc\ndef\nghi\n", "three items");
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF("abc def ghi"), " ", 8), "abc def\nghi\n", "three items, size > first two");
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF("abc def "), " ", 4), "abc\ndef \n", "two items, size includes space");
        TEST_RESULT_STRLST_Z(helpRenderSplitSize(STRDEF(""), " ", 1), "\n", "empty list");

        TEST_RESULT_STRLST_Z(
            helpRenderSplitSize(STRDEF("this is a short sentence"), " ", 10),
            "this is a\n"
            "short\n"
            "sentence\n",
            "variable word sizes");
    }

    // *****************************************************************************************************************************
    if (testBegin("helpRenderText() and helpRenderSummary()"))
    {
        TEST_RESULT_STR_Z(
            helpRenderText(STRDEF("this is a short sentence"), false, false, 0, false, 80), "this is a short sentence", "one line");

        TEST_RESULT_STR_Z(
            helpRenderText(STRDEF("this is a short sentence"), false, false, 4, false, 14),
            "this is a\n"
            "    short\n"
            "    sentence",
            "three lines, no indent first");

        TEST_RESULT_STR_Z(
            helpRenderText(STRDEF("this is a short sentence"), true, true, 4, true, 132),
            "    this is a short sentence\n"
            "\n"
            "    FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.",
            "beta feature");

        TEST_RESULT_STR_Z(
            helpRenderText(STRDEF("This is a short paragraph.\n\nHere is another one."), true, false, 2, true, 16),
            "  This is a\n"
            "  short\n"
            "  paragraph.\n"
            "\n"
            "  Here is\n"
            "  another one.\n"
            "\n"
            "  FOR INTERNAL\n"
            "  USE ONLY. DO\n"
            "  NOT USE IN\n"
            "  PRODUCTION.",
            "two paragraphs, indent first, internal");

        TEST_RESULT_STR_Z(helpRenderSummary(STRDEF("Summary.")), "summary", "render summary");
    }

    // *****************************************************************************************************************************
    if (testBegin("helpRender()"))
    {
        StringList *argList = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version");

        const char *versionOnly = zNewFmt("%s\n", helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "version");
        TEST_RESULT_VOID(testCfgLoad(argList), "version from version command");
        TEST_RESULT_STR_Z(helpRender(), versionOnly, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "version");
        hrnCfgArgRawZ(argList, cfgOptOutput, "text");
        TEST_RESULT_VOID(testCfgLoad(argList), "version text output from version command");
        TEST_RESULT_STR_Z(helpRender(), versionOnly, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "version");
        hrnCfgArgRawZ(argList, cfgOptOutput, "num");
        TEST_RESULT_VOID(testCfgLoad(argList), "version num output from version command");
        TEST_RESULT_STR_Z(helpRender(), zNewFmt("%d", PROJECT_VERSION_NUM), "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "--version");
        TEST_RESULT_VOID(testCfgLoad(argList), "version from version option");
        TEST_RESULT_STR_Z(helpRender(), versionOnly, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("general invocation");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from empty command line");
        TEST_RESULT_STR_Z(helpRender(), generalHelp, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from help command");
        TEST_RESULT_STR_Z(helpRender(), generalHelp, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "--version");
        strLstAddZ(argList, "--help");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from help option");
        TEST_RESULT_STR_Z(helpRender(), generalHelp, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restore command");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "32768");
        hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 1, "backup.example.net");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 2, "dr.test.org");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "/link1=/dest1");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "/link2=/dest2");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "db1");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "db2");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command");
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

        const String *commandHelp = NULL;
        TEST_ASSIGN(commandHelp, helpRender(), "render help");

        TEST_RESULT_STR_Z(
            testHelpBlockFirst(commandHelp, 2), zNewFmt("%s - 'restore' command help\n\nRestore a database cluster.", helpVersion),
            "header and summary");
        TEST_RESULT_STR_Z(
            testHelpBlockLast(commandHelp, 1), "Use 'pgbackrest help restore [option]' for more information.\n", "footer");
        TEST_RESULT_STR_Z(
            testHelpSectionList(commandHelp),
            "Command Options:\n"
            "General Options:\n"
            "Log Options:\n"
            "Maintainer Options:\n"
            "Repository Options:\n"
            "Stanza Options:\n",
            "sections");

        // The column width is set by the longest option name
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "repo-sftp-private-key-passphrase"),
            "  --repo-sftp-private-key-passphrase  SFTP private key passphrase\n",
            "longest option name");

        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "force"),
            "  --force                             force a restore [default=n]\n",
            "default on the same line");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "archive-mode"),
            "  --archive-mode                      preserve or disable archiving on restored\n"
            "                                      cluster [default=preserve]\n",
            "summary wraps with default");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "repo-host-config"),
            "  --repo-host-config                  pgBackRest repository host configuration\n"
            "                                      file\n"
            "                                      [default=/etc/pgbackrest/pgbackrest.conf]\n",
            "summary and default wrap");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "buffer-size"),
            "  --buffer-size                       buffer size for I/O operations\n"
            "                                      [current=32768, default=1MiB]\n",
            "current and default");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "cmd"),
            "  --cmd                               pgBackRest command\n"
            "                                      [default=/path/to/pgbackrest]\n",
            "dynamic default");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "db-include"),
            "  --db-include                        restore only specified databases\n"
            "                                      [current=db1, db2]\n",
            "list current");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "link-map"),
            "  --link-map                          modify the destination of a symlink\n"
            "                                      [current=/link1=/dest1, /link2=/dest2]\n",
            "hash current");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "repo-cipher-pass"),
            "  --repo-cipher-pass                  repository cipher passphrase\n"
            "                                      [current=<redacted>]\n",
            "secure current");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "repo-cipher-type"),
            "  --repo-cipher-type                  cipher used to encrypt the repository\n"
            "                                      [current=<multi>, default=none]\n",
            "multiple current with default");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "repo-host"),
            "  --repo-host                         repository host when operating remotely\n"
            "                                      [current=<multi>]\n",
            "multiple current");
        TEST_RESULT_STR_Z(
            testHelpOption(commandHelp, "pg-version-force"),
            "  --pg-version-force                  force PostgreSQL version\n",
            "maintainer option");
        TEST_RESULT_STR(testHelpOption(commandHelp, "job-retry"), NULL, "internal option not listed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("errors with specified options");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(testCfgLoad(argList), "parse too many options");
        TEST_ERROR(helpRender(), ParamInvalidError, "only one option allowed for option help");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, BOGUS_STR);
        TEST_RESULT_VOID(testCfgLoad(argList), "parse bogus option");
        TEST_ERROR(helpRender(), OptionInvalidError, "option 'BOGUS' is not valid for command 'archive-push'");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, CFGCMD_HELP);
        strLstAddZ(argList, CFGCMD_ARCHIVE_PUSH);
        strLstAddZ(argList, CFGOPT_PROCESS);
        TEST_RESULT_VOID(testCfgLoad(argList), "parse option invalid for command");
        TEST_ERROR(helpRender(), OptionInvalidError, "option 'process' is not valid for command 'archive-push'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("default and current option value");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR_Z(testHelpBlockLast(helpRender(), 1), "default: 1MiB\n", "check text");

        // Set a current value
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "32k");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR_Z(testHelpBlockLast(helpRender(), 1), "current: 32k\ndefault: 1MiB\n", "check text, current value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("deprecated host option names");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo1-s3-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(
            testHelpBlockLast(helpRender(), 1), "deprecated names: repo-azure-host, repo-s3-host\n", "check text");

        // Set a current value for deprecated option name
        hrnCfgArgRawZ(argList, cfgOptRepoType, "s3");
        strLstAddZ(argList, "--repo1-s3-host=s3-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(
            testHelpBlockLast(helpRender(), 2), "current: s3-host\n\ndeprecated names: repo-azure-host, repo-s3-host\n",
            "check text, current value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cipher pass redacted");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo-cipher-pass");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(testHelpBlockLast(helpRender(), 1), "current: <redacted>\n", "check text");
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("deprecated and new option name produce same results");

        #define TEST_HELP_HARDLINK                                                                                                 \
            "default: n\n"                                                                                                         \
            "\n"                                                                                                                   \
            "deprecated name: hardlink\n"

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "repo-hardlink");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for backup command, repo-hardlink option");
        TEST_RESULT_STR_Z(testHelpBlockLast(helpRender(), 2), TEST_HELP_HARDLINK, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "hardlink");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for backup command, deprecated hardlink option");
        TEST_RESULT_STR_Z(testHelpBlockLast(helpRender(), 2), TEST_HELP_HARDLINK, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check admonition");

        const char *const optionHelp = zNewFmt(
            "%s - 'backup' command - 'repo-retention-archive' option help\n"
            "\n"
            "Number of backups worth of continuous WAL to retain.\n"
            "\n"
            "NOTE: WAL segments required to make a backup consistent are always retained\n"
            "until the backup is expired regardless of how this option is configured.\n"
            "\n"
            "If this value is not set and repo-retention-full-type is count (default), then\n"
            "the archive to expire will default to the repo-retention-full (or\n"
            "repo-retention-diff) value corresponding to the repo-retention-archive-type if\n"
            "set to full (or diff). This will ensure that WAL is only expired for backups\n"
            "that are already expired. If repo-retention-full-type is time, then this value\n"
            "will default to removing archives that are earlier than the oldest full backup\n"
            "retained after satisfying the repo-retention-full setting.\n"
            "\n"
            "This option must be set if repo-retention-archive-type is set to incr. If disk\n"
            "space is at a premium, then this setting, in conjunction with\n"
            "repo-retention-archive-type, can be used to aggressively expire WAL segments.\n"
            "However, doing so negates the ability to perform PITR from the backups with\n"
            "expired WAL and is therefore not recommended.\n"
            "\n"
            "deprecated name: retention-archive\n",
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "repo-retention-archive");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for backup command, repo-retention-archive option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "check admonition text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple current values (with one missing)");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 1, "backup.example.net");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 3, "dr.test.org");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "repo-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command, repo-host option");
        TEST_RESULT_STR_Z(
            testHelpBlockLast(helpRender(), 2),
            "current:\n"
            "  repo1: backup.example.net\n"
            "  repo3: dr.test.org\n"
            "\n"
            "deprecated name: backup-host\n",
            "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple current values (with one unset)");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 1, "s3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 3, "dr.test.org");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "repo-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command, repo-host option");
        TEST_RESULT_STR_Z(
            testHelpBlockLast(helpRender(), 2),
            "current:\n"
            "  repo3: dr.test.org\n"
            "\n"
            "deprecated name: backup-host\n",
            "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple default values");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 1, "s3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 2, "azure");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 3, "gcs");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "repo-storage-upload-chunk-size");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command, repo-storage-upload-chunk-size option");

        // There is currently no way for some defaults to be NULL and others not but it could happen in the future
        cfgOptionIdxSet(cfgOptRepoStorageUploadChunkSize, 2, cfgSourceDefault, NULL);

        TEST_RESULT_STR_Z(
            testHelpBlockLast(helpRender(), 1),
            "default:\n"
            "  repo1: 5MiB\n"
            "  repo2: 4MiB\n",
            "check text");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdHelp() to file"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        TEST_RESULT_VOID(testCfgLoad(argList), "parse help from empty command line");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);

        THROW_ON_SYS_ERROR(freopen(TEST_PATH "/stdout.help", "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdHelp();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        Storage *storage = storagePosixNewP(TEST_PATH_STR);
        TEST_STORAGE_GET(storage, TEST_PATH "/stdout.help", generalHelp, .comment = "check text sent to file");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
