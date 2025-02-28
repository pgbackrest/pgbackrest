/***********************************************************************************************************************************
Test Help Command
***********************************************************************************************************************************/
#include "config/load.h"
#include "config/parse.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "common/harnessStorage.h"

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
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create help data
    const BldCfg bldCfg = bldCfgParse(storagePosixNewP(HRN_PATH_REPO_STR));
    const Buffer *const helpData = bldHlpRenderHelpAutoCCmp(
        bldCfg, bldHlpParse(storagePosixNewP(HRN_PATH_REPO_STR), bldCfg, false));

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
        "    verify          verify contents of the repository\n"
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
        TEST_RESULT_STR_Z(helpRender(helpData), versionOnly, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "version");
        hrnCfgArgRawZ(argList, cfgOptOutput, "text");
        TEST_RESULT_VOID(testCfgLoad(argList), "version text output from version command");
        TEST_RESULT_STR_Z(helpRender(helpData), versionOnly, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "version");
        hrnCfgArgRawZ(argList, cfgOptOutput, "num");
        TEST_RESULT_VOID(testCfgLoad(argList), "version num output from version command");
        TEST_RESULT_STR_Z(helpRender(helpData), zNewFmt("%d", PROJECT_VERSION_NUM), "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "--version");
        TEST_RESULT_VOID(testCfgLoad(argList), "version from version option");
        TEST_RESULT_STR_Z(helpRender(helpData), versionOnly, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("general invocation");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from empty command line");
        TEST_RESULT_STR_Z(helpRender(helpData), generalHelp, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from help command");
        TEST_RESULT_STR_Z(helpRender(helpData), generalHelp, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "--version");
        strLstAddZ(argList, "--help");
        TEST_RESULT_VOID(testCfgLoad(argList), "help from help option");
        TEST_RESULT_STR_Z(helpRender(helpData), generalHelp, "check text");

        // This test is broken up into multiple strings because C99 does not require compilers to support const strings > 4095 bytes
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restore command");

        const char *commandHelp = zNewFmt(
            "%s%s%s",
            helpVersion,
            " - 'restore' command help\n"
            "\n"
            "Restore a database cluster.\n"
            "\n"
            "The restore command automatically defaults to selecting the latest backup from\n"
            "the first repository where backups exist (see Quick Start - Restore a Backup).\n"
            "The order in which the repositories are checked is dictated by the\n"
            "pgbackrest.conf (e.g. repo1 will be checked before repo2). To select from a\n"
            "specific repository, the --repo option can be passed (e.g. --repo=1). The --set\n"
            "option can be passed if a backup other than the latest is desired.\n"
            "\n"
            "When PITR of --type=time or --type=lsn is specified, then the target time or\n"
            "target lsn must be specified with the --target option. If a backup is not\n"
            "specified via the --set option, then the configured repositories will be\n"
            "checked, in order, for a backup that contains the requested time or lsn. If no\n"
            "matching backup is found, the latest backup from the first repository\n"
            "containing backups will be used for --type=time while no backup will be\n"
            "selected for --type=lsn. For other types of PITR, e.g. xid, the --set option\n"
            "must be provided if the target is prior to the latest backup. See Point-in-Time\n"
            "Recovery for more details and examples.\n"
            "\n"
            "Replication slots are not included per recommendation of PostgreSQL. See\n"
            "Backing Up The Data Directory in the PostgreSQL documentation for more\n"
            "information.\n"
            "\n"
            "Command Options:\n"
            "\n"
            "  --archive-mode                      preserve or disable archiving on restored\n"
            "                                      cluster [default=preserve]\n"
            "  --db-exclude                        restore excluding the specified databases\n"
            "  --db-include                        restore only specified databases\n"
            "                                      [current=db1, db2]\n"
            "  --force                             force a restore [default=n]\n"
            "  --link-all                          restore all symlinks [default=n]\n"
            "  --link-map                          modify the destination of a symlink\n"
            "                                      [current=/link1=/dest1, /link2=/dest2]\n"
            "  --recovery-option                   set an option in postgresql.auto.conf or\n"
            "                                      recovery.conf\n"
            "  --set                               backup set to restore [default=latest]\n"
            "  --tablespace-map                    restore a tablespace into the specified\n"
            "                                      directory\n"
            "  --tablespace-map-all                restore all tablespaces into the\n"
            "                                      specified directory\n"
            "  --target                            recovery target\n"
            "  --target-action                     action to take when recovery target is\n"
            "                                      reached [default=pause]\n"
            "  --target-exclusive                  stop just before the recovery target is\n"
            "                                      reached [default=n]\n"
            "  --target-timeline                   recover along a timeline\n"
            "  --type                              recovery type [default=default]\n"
            "\n"
            "General Options:\n"
            "\n"
            "  --buffer-size                       buffer size for I/O operations\n"
            "                                      [current=32768, default=1MiB]\n"
            "  --cmd                               pgBackRest command\n"
            "                                      [default=/path/to/pgbackrest]\n"
            "  --cmd-ssh                           SSH client command [default=ssh]\n"
            "  --compress-level-network            network compression level [default=1]\n"
            "  --config                            pgBackRest configuration file\n"
            "                                      [default=/etc/pgbackrest/pgbackrest.conf]\n"
            "  --config-include-path               path to additional pgBackRest\n"
            "                                      configuration files\n"
            "                                      [default=/etc/pgbackrest/conf.d]\n"
            "  --config-path                       base path of pgBackRest configuration\n"
            "                                      files [default=/etc/pgbackrest]\n"
            "  --delta                             restore or backup using checksums\n"
            "                                      [default=n]\n"
            "  --io-timeout                        I/O timeout [default=1m]\n"
            "  --lock-path                         path where lock files are stored\n"
            "                                      [default=/tmp/pgbackrest]\n"
            "  --neutral-umask                     use a neutral umask [default=y]\n"
            "  --process-max                       max processes to use for\n"
            "                                      compress/transfer [default=1]\n"
            "  --protocol-timeout                  protocol timeout [default=31m]\n"
            "  --sck-keep-alive                    keep-alive enable [default=y]\n"
            "  --stanza                            defines the stanza\n"
            "  --tcp-keep-alive-count              keep-alive count\n"
            "  --tcp-keep-alive-idle               keep-alive idle time\n"
            "  --tcp-keep-alive-interval           keep-alive interval time\n"
            "\n"
            "Log Options:\n"
            "\n"
            "  --log-level-console                 level for console logging [default=warn]\n"
            "  --log-level-file                    level for file logging [default=info]\n"
            "  --log-level-stderr                  level for stderr logging [default=off]\n"
            "  --log-path                          path where log files are stored\n"
            "                                      [default=/var/log/pgbackrest]\n"
            "  --log-subprocess                    enable logging in subprocesses [default=n]\n"
            "  --log-timestamp                     enable timestamp in logging [default=y]\n"
            "\n",
            "Maintainer Options:\n"
            "\n"
            "  --pg-version-force                  force PostgreSQL version\n"
            "\n"
            "Repository Options:\n"
            "\n"
            "  --repo                              set repository\n"
            "  --repo-azure-account                azure repository account\n"
            "  --repo-azure-container              azure repository container\n"
            "  --repo-azure-endpoint               azure repository endpoint\n"
            "                                      [default=blob.core.windows.net]\n"
            "  --repo-azure-key                    azure repository key\n"
            "  --repo-azure-key-type               azure repository key type [default=shared]\n"
            "  --repo-azure-uri-style              azure URI Style [default=host]\n"
            "  --repo-cipher-pass                  repository cipher passphrase\n"
            "                                      [current=<redacted>]\n"
            "  --repo-cipher-type                  cipher used to encrypt the repository\n"
            "                                      [current=<multi>, default=none]\n"
            "  --repo-gcs-bucket                   GCS repository bucket\n"
            "  --repo-gcs-endpoint                 GCS repository endpoint\n"
            "                                      [default=storage.googleapis.com]\n"
            "  --repo-gcs-key                      GCS repository key\n"
            "  --repo-gcs-key-type                 GCS repository key type [default=service]\n"
            "  --repo-gcs-user-project             GCS project ID\n"
            "  --repo-host                         repository host when operating remotely\n"
            "                                      [current=<multi>]\n"
            "  --repo-host-ca-file                 repository host certificate authority file\n"
            "  --repo-host-ca-path                 repository host certificate authority path\n"
            "  --repo-host-cert-file               repository host certificate file\n"
            "  --repo-host-cmd                     repository host pgBackRest command\n"
            "                                      [default=/path/to/pgbackrest]\n"
            "  --repo-host-config                  pgBackRest repository host configuration\n"
            "                                      file\n"
            "                                      [default=/etc/pgbackrest/pgbackrest.conf]\n"
            "  --repo-host-config-include-path     pgBackRest repository host configuration\n"
            "                                      include path\n"
            "                                      [default=/etc/pgbackrest/conf.d]\n"
            "  --repo-host-config-path             pgBackRest repository host configuration\n"
            "                                      path [default=/etc/pgbackrest]\n"
            "  --repo-host-key-file                repository host key file\n"
            "  --repo-host-port                    repository host port when repo-host is set\n"
            "  --repo-host-type                    repository host protocol type\n"
            "                                      [default=ssh]\n"
            "  --repo-host-user                    repository host user when repo-host is\n"
            "                                      set [default=pgbackrest]\n"
            "  --repo-path                         path where backups and archive are stored\n"
            "                                      [default=/var/lib/pgbackrest]\n"
            "  --repo-s3-bucket                    S3 repository bucket\n"
            "  --repo-s3-endpoint                  S3 repository endpoint\n"
            "  --repo-s3-key                       S3 repository access key\n"
            "  --repo-s3-key-secret                S3 repository secret access key\n"
            "  --repo-s3-key-type                  S3 repository key type [default=shared]\n"
            "  --repo-s3-kms-key-id                S3 repository KMS key\n"
            "  --repo-s3-region                    S3 repository region\n"
            "  --repo-s3-requester-pays            S3 repository requester pays [default=n]\n"
            "  --repo-s3-role                      S3 repository role\n"
            "  --repo-s3-sse-customer-key          S3 repository SSE customer key\n"
            "  --repo-s3-token                     S3 repository security token\n"
            "  --repo-s3-uri-style                 S3 URI Style [default=host]\n"
            "  --repo-sftp-host                    SFTP repository host\n"
            "  --repo-sftp-host-fingerprint        SFTP repository host fingerprint\n"
            "  --repo-sftp-host-key-check-type     SFTP host key check type [default=strict]\n"
            "  --repo-sftp-host-key-hash-type      SFTP repository host key hash type\n"
            "  --repo-sftp-host-port               SFTP repository host port [default=22]\n"
            "  --repo-sftp-host-user               SFTP repository host user\n"
            "  --repo-sftp-known-host              SFTP known hosts file\n"
            "  --repo-sftp-private-key-file        SFTP private key file\n"
            "  --repo-sftp-private-key-passphrase  SFTP private key passphrase\n"
            "  --repo-sftp-public-key-file         SFTP public key file\n"
            "  --repo-storage-ca-file              repository storage CA file\n"
            "  --repo-storage-ca-path              repository storage CA path\n"
            "  --repo-storage-host                 repository storage host\n"
            "  --repo-storage-port                 repository storage port [default=443]\n"
            "  --repo-storage-tag                  repository storage tag(s)\n"
            "  --repo-storage-upload-chunk-size    repository storage upload chunk size\n"
            "  --repo-storage-verify-tls           repository storage certificate verify\n"
            "                                      [default=y]\n"
            "  --repo-target-time                  target time for repository\n"
            "  --repo-type                         type of storage used for the repository\n"
            "                                      [default=posix]\n"
            "\n"
            "Stanza Options:\n"
            "\n"
            "  --pg-path                           PostgreSQL data directory\n"
            "\n"
            "Use 'pgbackrest help restore [option]' for more information.\n");

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
        TEST_RESULT_STR_Z(helpRender(helpData), commandHelp, "check text");
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("errors with specified options");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(testCfgLoad(argList), "parse too many options");
        TEST_ERROR(helpRender(helpData), ParamInvalidError, "only one option allowed for option help");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, BOGUS_STR);
        TEST_RESULT_VOID(testCfgLoad(argList), "parse bogus option");
        TEST_ERROR(helpRender(helpData), OptionInvalidError, "option 'BOGUS' is not valid for command 'archive-push'");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, CFGCMD_HELP);
        strLstAddZ(argList, CFGCMD_ARCHIVE_PUSH);
        strLstAddZ(argList, CFGOPT_PROCESS);
        TEST_RESULT_VOID(testCfgLoad(argList), "parse option invalid for command");
        TEST_ERROR(helpRender(helpData), OptionInvalidError, "option 'process' is not valid for command 'archive-push'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("default and current option value");

        const char *optionHelp = zNewFmt(
            "%s - 'archive-push' command - 'buffer-size' option help\n"
            "\n"
            "Buffer size for I/O operations.\n"
            "\n"
            "Buffer size used for copy, compress, encrypt, and other operations. The number\n"
            "of buffers used depends on options and each operation may use additional\n"
            "memory, e.g. gz compression may use an additional 256KiB of memory.\n"
            "\n"
            "Allowed values are 16KiB, 32KiB, 64KiB, 128KiB, 256KiB, 512KiB, 1MiB, 2MiB,\n"
            "4MiB, 8MiB, and 16MiB.\n",
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "buffer-size");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR(helpRender(helpData), strNewFmt("%s\ndefault: 1MiB\n", optionHelp), "check text");

        // Set a current value
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "32k");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, buffer-size option");
        TEST_RESULT_STR(
            helpRender(helpData), strNewFmt("%s\ncurrent: 32k\ndefault: 1MiB\n", optionHelp), "check text, current value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("deprecated host option names");

        #define HELP_OPTION                                                                                                        \
            "%s - 'archive-push' command - 'repo-storage-host' option help\n"                                                      \
            "\n"                                                                                                                   \
            "Repository storage host.\n"                                                                                           \
            "\n"                                                                                                                   \
            "Connect to a host other than the storage (e.g. S3, Azure) endpoint. This is\n"                                        \
            "typically used for testing.\n"                                                                                        \
            "\n"

        #define HELP_OPTION_DEPRECATED_NAMES                                                                                       \
            "deprecated names: repo-azure-host, repo-s3-host\n"

        optionHelp = zNewFmt(
            HELP_OPTION
            HELP_OPTION_DEPRECATED_NAMES,
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo1-s3-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");

        optionHelp = zNewFmt(
            HELP_OPTION
            "current: s3-host\n"
            "\n"
            HELP_OPTION_DEPRECATED_NAMES,
            helpVersion);

        // Set a current value for deprecated option name
        hrnCfgArgRawZ(argList, cfgOptRepoType, "s3");
        strLstAddZ(argList, "--repo1-s3-host=s3-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text, current value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cipher pass redacted");

        optionHelp = zNewFmt(
            "%s - 'archive-push' command - 'repo-cipher-pass' option help\n"
            "\n"
            "Repository cipher passphrase.\n"
            "\n"
            "Passphrase used to encrypt/decrypt files of the repository.\n"
            "\n"
            "current: <redacted>\n",
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        strLstAddZ(argList, "archive-push");
        strLstAddZ(argList, "repo-cipher-pass");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for archive-push command, repo1-s3-host option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("deprecated and new option name produce same results");

        optionHelp = zNewFmt(
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
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "repo-hardlink");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for backup command, repo-hardlink option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "hardlink");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for backup command, deprecated hardlink option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check admonition");

        optionHelp = zNewFmt(
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
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check admonition text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple current values (with one missing)");

        optionHelp = zNewFmt(
            "%s - 'restore' command - 'repo-host' option help\n"
            "\n"
            "Repository host when operating remotely.\n"
            "\n"
            "When backing up and archiving to a locally mounted filesystem this setting is\n"
            "not required.\n"
            "\n"
            "current:\n"
            "  repo1: backup.example.net\n"
            "  repo3: dr.test.org\n"
            "\n"
            "deprecated name: backup-host\n",
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 1, "backup.example.net");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 3, "dr.test.org");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "repo-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command, repo-host option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple current values (with one unset)");

        optionHelp = zNewFmt(
            "%s - 'restore' command - 'repo-host' option help\n"
            "\n"
            "Repository host when operating remotely.\n"
            "\n"
            "When backing up and archiving to a locally mounted filesystem this setting is\n"
            "not required.\n"
            "\n"
            "current:\n"
            "  repo3: dr.test.org\n"
            "\n"
            "deprecated name: backup-host\n",
            helpVersion);

        argList = strLstNew();
        strLstAddZ(argList, "/path/to/pgbackrest");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 1, "s3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 3, "dr.test.org");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "restore");
        strLstAddZ(argList, "repo-host");
        TEST_RESULT_VOID(testCfgLoad(argList), "help for restore command, repo-host option");
        TEST_RESULT_STR_Z(helpRender(helpData), optionHelp, "check text");
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
        cmdHelp(helpData);

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        Storage *storage = storagePosixNewP(TEST_PATH_STR);
        TEST_STORAGE_GET(storage, TEST_PATH "/stdout.help", generalHelp, .comment = "check text sent to file");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
