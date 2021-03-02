/***********************************************************************************************************************************
Test Help Command
***********************************************************************************************************************************/
#include "config/parse.h"
#include "storage/posix/storage.h"
#include "storage/storage.h"
#include "version.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Program name a version are used multiple times
    const char *helpVersion = PROJECT_NAME " " PROJECT_VERSION;

    // General help text is used in more than one test
    const char *generalHelp = strZ(strNewFmt(
        "%s - General help\n"
        "\n"
        "Usage:\n"
        "    pgbackrest [options] [command]\n"
        "\n"
        "Commands:\n"
        "    archive-get     Get a WAL segment from the archive.\n"
        "    archive-push    Push a WAL segment to the archive.\n"
        "    backup          Backup a database cluster.\n"
        "    check           Check the configuration.\n"
        "    expire          Expire backups that exceed retention.\n"
        "    help            Get help.\n"
        "    info            Retrieve information about backups.\n"
        "    repo-get        Get files from a repository.\n"
        "    repo-ls         List files in a repository.\n"
        "    restore         Restore a database cluster.\n"
        "    stanza-create   Create the required stanza data.\n"
        "    stanza-delete   Delete a stanza.\n"
        "    stanza-upgrade  Upgrade a stanza.\n"
        "    start           Allow pgBackRest processes to run.\n"
        "    stop            Stop pgBackRest processes from running.\n"
        "    version         Get version.\n"
        "\n"
        "Use 'pgbackrest help [command]' for more information.\n",
        helpVersion));

    // *****************************************************************************************************************************
    if (testBegin("helpRenderText()"))
    {
        TEST_RESULT_STR_Z(helpRenderText(strNew("this is a short sentence"), 0, false, 80), "this is a short sentence", "one line");

        TEST_RESULT_STR_Z(
            helpRenderText(strNew("this is a short sentence"), 4, false, 14),
            "this is a\n"
            "    short\n"
            "    sentence",
            "three lines, no indent first");

        TEST_RESULT_STR_Z(
            helpRenderText(strNew("This is a short paragraph.\n\nHere is another one."), 2, true, 16),
            "  This is a\n"
            "  short\n"
            "  paragraph.\n"
            "\n"
            "  Here is\n"
            "  another one.",
            "two paragraphs, indent first");
    }

    // *****************************************************************************************************************************
    if (testBegin("helpRenderValue()"))
    {
        TEST_RESULT_STR_Z(helpRenderValue(varNewBool(true), cfgOptTypeBoolean), "y", "boolean y");
        TEST_RESULT_STR_Z(helpRenderValue(varNewBool(false), cfgOptTypeBoolean), "n", "boolean n");
        TEST_RESULT_STR_Z(helpRenderValue(varNewStrZ("test-string"), cfgOptTypeString), "test-string", "string");
        TEST_RESULT_STR_Z(helpRenderValue(varNewInt64(1234), cfgOptTypeInteger), "1234", "int");
        TEST_RESULT_STR_Z(helpRenderValue(varNewInt64(1234000), cfgOptTypeTime), "1234", "time");
    }

    // *****************************************************************************************************************************
    if (testBegin("helpRender()"))
    {
        StringList *argList = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help from empty command line");
        TEST_RESULT_STR_Z(helpRender(), generalHelp, "    check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help from help command");
        TEST_RESULT_STR_Z(helpRender(), generalHelp, "    check text");

        // -------------------------------------------------------------------------------------------------------------------------
        const char *commandHelp = strZ(strNewFmt(
            "%s%s",
            helpVersion,
            " - 'version' command help\n"
            "\n"
            "Get version.\n"
            "\n"
            "Displays installed pgBackRest version.\n"));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "version");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for version command");
        TEST_RESULT_STR_Z(helpRender(), commandHelp, "    check text");

        // This test is broken up into multiple strings because C99 does not require compilers to support const strings > 4095 bytes
        // -------------------------------------------------------------------------------------------------------------------------
        commandHelp = strZ(strNewFmt(
            "%s%s%s",
            helpVersion,
            " - 'restore' command help\n"
            "\n"
            "Restore a database cluster.\n"
            "\n"
            "This command is generally run manually, but there are instances where it might\n"
            "be automated.\n"
            "\n"
            "Command Options:\n"
            "\n"
            "  --archive-mode                   preserve or disable archiving on restored\n"
            "                                   cluster [default=preserve]\n"
            "  --db-include                     restore only specified databases\n"
            "                                   [current=db1, db2]\n"
            "  --force                          force a restore [default=n]\n"
            "  --link-all                       restore all symlinks [default=n]\n"
            "  --link-map                       modify the destination of a symlink\n"
            "                                   [current=/link1=/dest1, /link2=/dest2]\n"
            "  --recovery-option                set an option in recovery.conf\n"
            "  --set                            backup set to restore [default=latest]\n"
            "  --tablespace-map                 restore a tablespace into the specified\n"
            "                                   directory\n"
            "  --tablespace-map-all             restore all tablespaces into the specified\n"
            "                                   directory\n"
            "  --target                         recovery target\n"
            "  --target-action                  action to take when recovery target is\n"
            "                                   reached [default=pause]\n"
            "  --target-exclusive               stop just before the recovery target is\n"
            "                                   reached [default=n]\n"
            "  --target-timeline                recover along a timeline\n"
            "  --type                           recovery type [default=default]\n"
            "\n"
            "General Options:\n"
            "\n"
            "  --buffer-size                    buffer size for file operations\n"
            "                                   [current=32768, default=1048576]\n"
            "  --cmd-ssh                        path to ssh client executable [default=ssh]\n"
            "  --compress-level-network         network compression level [default=3]\n"
            "  --config                         pgBackRest configuration file\n"
            "                                   [default=/etc/pgbackrest/pgbackrest.conf]\n"
            "  --config-include-path            path to additional pgBackRest configuration\n"
            "                                   files [default=/etc/pgbackrest/conf.d]\n"
            "  --config-path                    base path of pgBackRest configuration files\n"
            "                                   [default=/etc/pgbackrest]\n"
            "  --delta                          restore or backup using checksums [default=n]\n"
            "  --io-timeout                     i/O timeout [default=60]\n"
            "  --lock-path                      path where lock files are stored\n"
            "                                   [default=/tmp/pgbackrest]\n"
            "  --neutral-umask                  use a neutral umask [default=y]\n"
            "  --process-max                    max processes to use for compress/transfer\n"
            "                                   [default=1]\n"
            "  --protocol-timeout               protocol timeout [default=1830]\n"
            "  --sck-keep-alive                 keep-alive enable [default=y]\n"
            "  --stanza                         defines the stanza\n"
            "  --tcp-keep-alive-count           keep-alive count\n"
            "  --tcp-keep-alive-idle            keep-alive idle time\n"
            "  --tcp-keep-alive-interval        keep-alive interval time\n"
            "\n"
            "Log Options:\n"
            "\n"
            "  --log-level-console              level for console logging [default=warn]\n"
            "  --log-level-file                 level for file logging [default=info]\n"
            "  --log-level-stderr               level for stderr logging [default=warn]\n"
            "  --log-path                       path where log files are stored\n"
            "                                   [default=/var/log/pgbackrest]\n"
            "  --log-subprocess                 enable logging in subprocesses [default=n]\n"
            "  --log-timestamp                  enable timestamp in logging [default=y]\n"
            "\n",
            "Repository Options:\n"
            "\n"
            "  --repo-azure-account             azure repository account\n"
            "  --repo-azure-container           azure repository container\n"
            "  --repo-azure-endpoint            azure repository endpoint\n"
            "                                   [default=blob.core.windows.net]\n"
            "  --repo-azure-key                 azure repository key\n"
            "  --repo-azure-key-type            azure repository key type [default=shared]\n"
            "  --repo-cipher-pass               repository cipher passphrase\n"
            "                                   [current=<redacted>]\n"
            "  --repo-cipher-type               cipher used to encrypt the repository\n"
            "                                   [current=aes-256-cbc, default=none]\n"
            "  --repo-host                      repository host when operating remotely via\n"
            "                                   SSH [current=backup.example.net]\n"
            "  --repo-host-cmd                  pgBackRest exe path on the repository host\n"
            "                                   [default=/path/to/pgbackrest]\n"
            "  --repo-host-config               pgBackRest repository host configuration\n"
            "                                   file\n"
            "                                   [default=/etc/pgbackrest/pgbackrest.conf]\n"
            "  --repo-host-config-include-path  pgBackRest repository host configuration\n"
            "                                   include path [default=/etc/pgbackrest/conf.d]\n"
            "  --repo-host-config-path          pgBackRest repository host configuration\n"
            "                                   path [default=/etc/pgbackrest]\n"
            "  --repo-host-port                 repository host port when repo-host is set\n"
            "  --repo-host-user                 repository host user when repo-host is set\n"
            "                                   [default=pgbackrest]\n"
            "  --repo-path                      path where backups and archive are stored\n"
            "                                   [default=/var/lib/pgbackrest]\n"
            "  --repo-s3-bucket                 S3 repository bucket\n"
            "  --repo-s3-endpoint               S3 repository endpoint\n"
            "  --repo-s3-key                    S3 repository access key\n"
            "  --repo-s3-key-secret             S3 repository secret access key\n"
            "  --repo-s3-key-type               S3 repository key type [default=shared]\n"
            "  --repo-s3-region                 S3 repository region\n"
            "  --repo-s3-role                   S3 repository role\n"
            "  --repo-s3-token                  S3 repository security token\n"
            "  --repo-s3-uri-style              S3 URI Style [default=host]\n"
            "  --repo-storage-ca-file           repository storage CA file\n"
            "  --repo-storage-ca-path           repository storage CA path\n"
            "  --repo-storage-host              repository storage host\n"
            "  --repo-storage-port              repository storage port [default=443]\n"
            "  --repo-storage-verify-tls        repository storage certificate verify\n"
            "                                   [default=y]\n"
            "  --repo-type                      type of storage used for the repository\n"
            "                                   [default=posix]\n"
            "\n"
            "Stanza Options:\n"
            "\n"
            "  --pg-path                        postgreSQL data directory\n"
            "\n"
            "Use 'pgbackrest help restore [option]' for more information.\n"));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "--buffer-size=32768");
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "supersecret", true);
        strLstAddZ(argList, "--repo1-host=backup.example.net");
        strLstAddZ(argList, "--link-map=/link1=/dest1");
        strLstAddZ(argList, "--link-map=/link2=/dest2");
        strLstAddZ(argList, "--db-include=db1");
        strLstAddZ(argList, "--db-include=db2");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for restore command");
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");
        TEST_RESULT_STR_Z(helpRender(), commandHelp, "    check text");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "parse too many options");
        TEST_ERROR(helpRender(), ParamInvalidError, "only one option allowed for option help");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, BOGUS_STR);
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "parse bogus option");
        TEST_ERROR(helpRender(), OptionInvalidError, "option 'BOGUS' is not valid for command 'archive-push'");

        // -------------------------------------------------------------------------------------------------------------------------
        const char *optionHelp = strZ(strNewFmt(
            "%s - 'archive-push' command - 'buffer-size' option help\n"
            "\n"
            "Buffer size for file operations.\n"
            "\n"
            "Set the buffer size used for copy, compress, and uncompress functions. A\n"
            "maximum of 3 buffers will be in use at a time per process. An additional\n"
            "maximum of 256K per process may be used for zlib buffers.\n"
            "\n"
            "Size can be entered in bytes (default) or KB, MB, GB, TB, or PB where the\n"
            "multiplier is a power of 1024. For example, the case-insensitive value 32k (or\n"
            "32KB) can be used instead of 32768.\n"
            "\n"
            "Allowed values, in bytes, are 16384, 32768, 65536, 131072, 262144, 524288,\n"
            "1048576, 2097152, 4194304, 8388608, and 16777216.\n",
            helpVersion));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR(helpRender(), strNewFmt("%s\ndefault: 1048576\n", optionHelp), "    check text");

        strLstAddZ(argList, "--buffer-size=32768");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR(helpRender(), strNewFmt("%s\ncurrent: 32768\ndefault: 1048576\n", optionHelp), "    check text");

        // -------------------------------------------------------------------------------------------------------------------------
        #define HELP_OPTION                                                                                                        \
            "%s - 'archive-push' command - 'repo-storage-host' option help\n"                                                      \
            "\n"                                                                                                                   \
            "Repository storage host.\n"                                                                                           \
            "\n"                                                                                                                   \
            "Connect to a host other than the storage (e.g. S3, Azure) end point. This is\n"                                       \
            "typically used for testing.\n"                                                                                        \
            "\n"

        #define HELP_OPTION_DEPRECATED_NAMES                                                                                       \
            "deprecated names: repo-azure-host, repo-s3-host\n"

        optionHelp = strZ(strNewFmt(
            HELP_OPTION
            HELP_OPTION_DEPRECATED_NAMES,
            helpVersion));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo1-s3-host");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check text");

        optionHelp = strZ(strNewFmt(
            HELP_OPTION
            "current: s3-host\n"
            "\n"
            HELP_OPTION_DEPRECATED_NAMES,
            helpVersion));

        strLstAddZ(argList, "--repo1-type=s3");
        strLstAddZ(argList, "--repo1-s3-host=s3-host");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check text");

        // -------------------------------------------------------------------------------------------------------------------------
        optionHelp = strZ(strNewFmt(
            "%s - 'archive-push' command - 'repo-cipher-pass' option help\n"
            "\n"
            "Repository cipher passphrase.\n"
            "\n"
            "Passphrase used to encrypt/decrypt files of the repository.\n"
            "\n"
            "current: <redacted>\n",
            helpVersion));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "supersecret", true);
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo-cipher-pass");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for archive-push command, repo1-s3-host option");
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check text");

        // -------------------------------------------------------------------------------------------------------------------------
        optionHelp = strZ(strNewFmt(
            "%s - 'backup' command - 'repo-hardlink' option help\n"
            "\n"
            "Hardlink files between backups in the repository.\n"
            "\n"
            "Enable hard-linking of files in differential and incremental backups to their\n"
            "full backups. This gives the appearance that each backup is a full backup at\n"
            "the file-system level. Be careful, though, because modifying files that are\n"
            "hard-linked can affect all the backups in the set.\n"
            "\n"
            "default: n\n"
            "\n"
            "deprecated name: hardlink\n",
            helpVersion));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "repo-hardlink");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for backup command, repo-hardlink option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "hardlink");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for backup command, deprecated hardlink option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check text");

        // Check admonition
        // -------------------------------------------------------------------------------------------------------------------------
        optionHelp = strZ(strNewFmt(
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
            helpVersion));

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "repo-retention-archive");
        TEST_RESULT_VOID(
            harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "help for backup command, repo-retention-archive option");
        TEST_RESULT_STR_Z(helpRender(), optionHelp, "    check admonition text");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdHelp()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        TEST_RESULT_VOID(harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList)), "parse help from empty command line");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.help", testPath());

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdHelp();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        Storage *storage = storagePosixNewP(strNew(testPath()));
        TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(storage, stdoutFile))), generalHelp, "    check text");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
