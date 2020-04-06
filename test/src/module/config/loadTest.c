/***********************************************************************************************************************************
Test Configuration Load
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/log.h"
#include "protocol/helper.h"
#include "version.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadLogSetting()"))
    {
        cfgInit();

        TEST_RESULT_VOID(cfgLoadLogSetting(), "load log settings all defaults");

        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelOff, "stderr logging is off");
        TEST_RESULT_INT(logLevelFile, logLevelOff, "file logging is off");
        TEST_RESULT_BOOL(logTimestamp, true, "timestamp logging is on");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadUpdateOption()"))
    {
        String *exe = strNew("/path/to/pgbackrest");
        String *exeOther = strNew("/other/path/to/pgbackrest");

        cfgInit();
        cfgCommandSet(cfgCmdBackup, cfgCmdRoleDefault);
        cfgExeSet(exe);

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionValidSet(cfgOptPgHost, true);

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "hosts are not set so don't update commands");

        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, varNewStrZ("repo-host"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command is updated");
        TEST_RESULT_STR(cfgOptionStr(cfgOptRepoHostCmd), exe, "    check repo1-host-cmd");

        cfgOptionSet(cfgOptRepoHostCmd, cfgSourceParam, varNewStr(exeOther));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command was already set");
        TEST_RESULT_STR(cfgOptionStr(cfgOptRepoHostCmd), exeOther, "    check repo1-host-cmd");

        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptPgHostCmd, true);
        cfgOptionSet(cfgOptPgHost, cfgSourceParam, varNewStrZ("pg1-host"));

        cfgOptionValidSet(cfgOptPgHost + 1, true);
        cfgOptionSet(cfgOptPgHost + 1, cfgSourceParam, varNewStrZ("pg2-host"));
        cfgOptionValidSet(cfgOptPgHostCmd + 1, true);
        cfgOptionSet(cfgOptPgHostCmd + 1, cfgSourceParam, varNewStr(exeOther));

        cfgOptionValidSet(cfgOptPgHost + cfgOptionIndexTotal(cfgOptPgHost) - 1, true);
        cfgOptionSet(cfgOptPgHost + cfgOptionIndexTotal(cfgOptPgHost) - 1, cfgSourceParam, varNewStrZ("pgX-host"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "pg remote command is updated");
        TEST_RESULT_STR(cfgOptionStr(cfgOptPgHostCmd), exe, "    check pg1-host-cmd");
        TEST_RESULT_STR(cfgOptionStr(cfgOptPgHostCmd + 1), exeOther, "    check pg2-host-cmd is already set");
        TEST_RESULT_STR(cfgOptionStr(cfgOptPgHostCmd + 2), NULL, "    check pg3-host-cmd is not set");
        TEST_RESULT_STR(cfgOptionStr(cfgOptPgHostCmd + cfgOptionIndexTotal(cfgOptPgHost) - 1), exe, "    check pgX-host-cmd");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();

        cfgOptionValidSet(cfgOptDbTimeout, true);
        cfgOptionSet(cfgOptDbTimeout, cfgSourceParam, varNewDbl(100));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "pg timeout set but not protocol timeout");

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceDefault, varNewDbl(101));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "protocol timeout > pg timeout");

        cfgOptionSet(cfgOptDbTimeout, cfgSourceParam, varNewDbl(100000));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "protocol timeout set automatically");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 100030, "    check protocol timeout");

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(50.5));
        TEST_ERROR(
            cfgLoadUpdateOption(), OptionInvalidValueError,
            "'50.5' is not valid for 'protocol-timeout' option\n"
                "HINT 'protocol-timeout' option (50.5) should be greater than 'db-timeout' option (100000).");

        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(45));
        cfgOptionSet(cfgOptDbTimeout, cfgSourceDefault, varNewDbl(3600));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "set default pg timeout to be less than protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 45, "    check protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptDbTimeout), 15, "    check db timeout");

        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(11));
        cfgOptionSet(cfgOptDbTimeout, cfgSourceDefault, varNewDbl(3600));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "set default pg timeout to be less than test protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 11, "    check protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptDbTimeout), 5.5, "    check db timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdBackup, cfgCmdRoleDefault);
        cfgExeSet(exe);

        cfgOptionValidSet(cfgOptPgHost, true);
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "only repo-host is valid");

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, varNewStrZ("repo-host"));
        cfgOptionValidSet(cfgOptPgHost + 4, true);
        cfgOptionSet(cfgOptPgHost + 4, cfgSourceParam, varNewStrZ("pg5-host"));
        TEST_ERROR(cfgLoadUpdateOption(), ConfigError, "pg and repo hosts cannot both be configured as remote");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("process-max"));

        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdHelp, argList), "load help config -- no retention warning");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    command is help");

        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));

        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        strLstAdd(argList, strNew("--repo1-retention-full=1"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config no retention warning");
        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 1, "    repo1-retention-archive set");

        // Munge repo-type for coverage.  This will go away when there are multiple repos.
        cfgOptionSet(cfgOptRepoType, cfgSourceParam, NULL);
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "load config no repo-type");

        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=incr"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
                "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full'"
                " to the maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=incr' but option"
                " 'repo1-retention-archive' is not set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=diff' but neither option"
                " 'repo1-retention-archive' nor option 'repo1-retention-diff' is set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        strLstAdd(argList, strNew("--repo1-retention-diff=2"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.");
        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 2, "    repo1-retention-archive set to retention-diff");

        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));
        strLstAdd(argList, strNew("--repo1-retention-archive=3"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option 'repo1-retention-diff' is not set for 'repo1-retention-archive-type=diff'\n"
            "            HINT: to retain differential backups indefinitely (without warning), set option 'repo1-retention-diff'"
                " to the maximum.");

        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));
        strLstAdd(argList, strNew("--repo1-retention-archive=3"));
        strLstAdd(argList, strNew("--repo1-retention-diff=2"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdExpire, argList), "load config with success");

        // -------------------------------------------------------------------------------------------------------------------------
        setenv("PGBACKREST_REPO1_S3_KEY", "mykey", true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", "mysecretkey", true);

        // Invalid bucket name with verification enabled fails
        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=bogus.bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));

        TEST_ERROR(
            harnessCfgLoad(cfgCmdArchiveGet, argList), OptionInvalidValueError,
            "'bogus.bucket' is not valid for option 'repo1-s3-bucket'"
                "\nHINT: RFC-2818 forbids dots in wildcard matches."
                "\nHINT: TLS/SSL verification cannot proceed with this bucket name."
                "\nHINT: remove dots from the bucket name.");

        // Invalid bucket name with verification disabled succeeds
        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=bogus.bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--no-repo1-s3-verify-ssl"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdArchiveGet, argList), "invalid bucket with no verification");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3Bucket), "bogus.bucket", "    check bucket value");

        // Valid bucket name
        argList = strLstNew();
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=cool-bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdArchiveGet, argList), "valid bucket name");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3Bucket), "cool-bucket", "    check bucket value");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        unsetenv("PGBACKREST_REPO1_S3_KEY");
        unsetenv("PGBACKREST_REPO1_S3_KEY_SECRET");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compress-type=none when compress=n");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdArchivePush, argList), "load config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "none", "    compress-type=none");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 0, "    compress-level=0");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compress-type=gz when compress=y");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_COMPRESS_LEVEL "=9");

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdArchivePush, argList), "load config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "gz", "    compress-type=gz");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 9, "    compress-level=9");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn when compress-type and compress both set");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_COMPRESS_TYPE "=gz");

        TEST_RESULT_VOID(harnessCfgLoad(cfgCmdArchivePush, argList), "load config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "gz", "    compress-type=gz");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 6, "    compress-level=6");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        harnessLogResult(
            "P00   WARN: 'compress' and 'compress-type' options should not both be set\n"
            "            HINT: 'compress-type' is preferred and 'compress' is deprecated.");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadLogFile()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path"));
        strLstAdd(argList, strNewFmt("--lock-path=%s/lock", testDataPath()));
        strLstAdd(argList, strNew("--log-path=/bogus"));
        strLstAdd(argList, strNew("--log-level-file=info"));
        strLstAdd(argList, strNew("backup"));
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for backup");
        lockRelease(true);

        // On the error case is tested here, success is tested in cfgLoad()
        TEST_RESULT_VOID(cfgLoadLogFile(), "attempt to open bogus log file");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptLogLevelFile), "off", "log-level-file should now be off");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoad()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("dry-run valid, --no-dry-run");

        StringList *argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAdd(argList, strNewFmt("--" CFGOPT_LOCK_PATH "=%s/lock", testDataPath()));
        strLstAddZ(argList, CFGCMD_EXPIRE);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config");
        TEST_RESULT_VOID(storageRepoWrite(), "  check writable storage");
        lockRelease(true);

        TEST_TITLE("dry-run valid, dry-run");

        strLstAddZ(argList, "--" CFGOPT_DRY_RUN);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config");
        TEST_ERROR(
            storageRepoWrite(), AssertError, "unable to get writable storage in dry-run mode or before dry-run is initialized");
        lockRelease(true);

        // Command does not have umask and disables keep-alives
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAddZ(argList, "--no-" CFGOPT_SCK_KEEP_ALIVE);
        strLstAdd(argList, strNew("info"));

        socketLocal = (struct SocketLocal){.init = false};

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config and don't set umask");
        TEST_RESULT_BOOL(socketLocal.init, true, "   check socketLocal.init");
        TEST_RESULT_BOOL(socketLocal.keepAlive, false, "   check socketLocal.keepAlive");

        // Set a distinct umask value and test that the umask is reset by configLoad since default for neutral-umask=y
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("archive-get"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for neutral-umask");
        TEST_RESULT_INT(umask(0111), 0000, "    umask was reset");

        // Set a distinct umask value and test that the umask is not reset by configLoad with option --no-neutral-umask
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-neutral-umask"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("archive-get"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for no-neutral-umask");
        TEST_RESULT_INT(umask(0), 0111, "    umask was not reset");

        // No command
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "no command");

        // Help command only
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("help"));

        ioBufferSizeSet(333);
        socketLocal = (struct SocketLocal){.init = false};

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command");
        TEST_RESULT_UINT(ioBufferSize(), 333, "buffer size not updated by help command");
        TEST_RESULT_BOOL(socketLocal.init, false, "socketLocal not updated by help command");

        // Help command for backup
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("--repo1-retention-full=2"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command for backup");
        TEST_RESULT_UINT(ioBufferSize(), 4 * 1024 * 1024, "buffer size set to option default");

        // Command takes lock and opens log file and uses custom tcp settings
        // -------------------------------------------------------------------------------------------------------------------------
        socketLocal = (struct SocketLocal){.init = false};
        struct stat statLog;

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));
        strLstAdd(argList, strNewFmt("--lock-path=%s/lock", testDataPath()));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=warn"));
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_COUNT "=11");
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_IDLE "=2222");
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_INTERVAL "=888");
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "lock and open log file");
        TEST_RESULT_INT(lstat(strPtr(strNewFmt("%s/db-backup.log", testPath())), &statLog), 0, "   check log file exists");
        TEST_RESULT_BOOL(socketLocal.init, true, "   check socketLocal.init");
        TEST_RESULT_BOOL(socketLocal.keepAlive, true, "   check socketLocal.keepAlive");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveCount, 11, "   check socketLocal.tcpKeepAliveCount");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveIdle, 2222, "   check socketLocal.tcpKeepAliveIdle");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveInterval, 888, "   check socketLocal.tcpKeepAliveInterval");

        lockRelease(true);

        // Local command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to");
        strLstAdd(argList, strNew("--process=1"));
        strLstAdd(argList, strNew("--host-id=1"));
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAdd(argList, strNew("--log-level-file=warn"));
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/db-backup-local-001.log", testPath())), &statLog), 0, "   check log file exists");

        // Remote command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAdd(argList, strNew("--process=0"));
        strLstAddZ(argList, CFGCMD_INFO ":" CONFIG_COMMAND_ROLE_REMOTE);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/all-info-remote-000.log", testPath())), &statLog), 0, "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote command without archive-async option");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAdd(argList, strNew("--process=1"));
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_REMOTE);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/test-archive-get-remote-001.log", testPath())), &statLog), 0, "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("local command with archive-async option");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAdd(argList, strNew("--process=1"));
        strLstAddZ(argList, CFGCMD_ARCHIVE_PUSH ":" CONFIG_COMMAND_ROLE_LOCAL);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/test-archive-push-async-local-001.log", testPath())), &statLog), 0,
            "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive-get command with async role");

        argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        strLstAdd(argList, strNewFmt("--" CFGOPT_LOG_PATH "=%s", testPath()));
        strLstAdd(argList, strNewFmt("--lock-path=%s/lock", testDataPath()));
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_ASYNC);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/test-archive-get-async.log", testPath())), &statLog), 0, "   check log file exists");

        lockRelease(true);
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
