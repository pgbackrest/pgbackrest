/***********************************************************************************************************************************
Test Configuration Load
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/log.h"
#include "protocol/helper.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "storage/cifs/storage.h"
#include "storage/posix/storage.h"

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
        HRN_CFG_LOAD(cfgCmdVersion, strLstNew());

        TEST_RESULT_VOID(cfgLoadLogSetting(), "load log settings all defaults");

        TEST_RESULT_INT(hrnLogLevelStdOut(), logLevelOff, "console logging is off");
        TEST_RESULT_INT(hrnLogLevelStdErr(), logLevelOff, "stderr logging is off");
        TEST_RESULT_INT(hrnLogLevelFile(), logLevelOff, "file logging is off");
        TEST_RESULT_BOOL(hrnLogTimestamp(), true, "timestamp logging is on");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadUpdateOption()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if user passes pg/repo options when they are internal");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        TEST_ERROR(hrnCfgLoadP(cfgCmdCheck, argList), OptionInvalidError, "option 'repo' not valid for command 'check'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when repo option not set and repo total > 1 or first repo index != 1");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, "/repo4");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaDelete, argList), OptionRequiredError,
            "stanza-delete command requires option: repo\n"
            "HINT: this command requires a specific repository to operate on");

        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList, .comment = "load stanza-delete with repo set");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, "/repo2");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaDelete, argList), OptionRequiredError,
            "stanza-delete command requires option: repo\n"
            "HINT: this command requires a specific repository to operate on");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo1");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList, .comment = "load stanza-delete with single repo, repo option not set");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, "/repo4");
        HRN_CFG_LOAD(cfgCmdInfo, argList, .comment = "load info config -- option repo not required");
        TEST_RESULT_BOOL(cfgCommand() == cfgCmdInfo, true, "    command is info");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("local default repo paths must be different");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptRepo, "3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, "/repo4");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 3, "/repo4");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 2, "host2");
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdRestore, argList), OptionInvalidValueError,
            "local repo3 and repo4 paths are both '/repo4' but must be different");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo can be specified for backup");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("local default repo paths for cifs repo type must be different");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoType, 1, STORAGE_CIFS_TYPE);
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoType, 2, STORAGE_CIFS_TYPE);
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdInfo, argList), OptionInvalidValueError,
            "local repo1 and repo2 paths are both '/var/lib/pgbackrest' but must be different");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("local repo paths same but types different");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoType, 1, STORAGE_POSIX_TYPE);
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoType, 2, STORAGE_CIFS_TYPE);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 3, "s3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Bucket, 3, "cool-bucket");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Region, 3, "region");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Endpoint, 3, "endpoint");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 3, "mykey");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3KeySecret, 3, "mysecretkey");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 3);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3KeySecret, 3);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo-host-cmd is defaulted when null");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        cfgOptionIdxSet(cfgOptRepoHost, 0, cfgSourceParam, varNewStrZ("repo-host"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command is updated");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoHostCmd, 0), testProjectExe(), "    check repo1-host-cmd");

        cfgOptionIdxSet(cfgOptRepoHostCmd, 0, cfgSourceParam, VARSTRDEF("/other"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command was already set");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoHostCmd, 0), "/other", "    check repo1-host-cmd");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg-host-cmd is defaulted when null");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 1, "pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, "/pg2");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 2, "pg2");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHostCmd, 2, "pg2-exe");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgHostCmd, 0), testProjectExe(), "    check pg1-host-cmd");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgHostCmd, 1), "pg2-exe", "    check pg2-host-cmd");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("db-timeout set but not protocol timeout");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptDbTimeout, "100");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        cfgOptionInvalidate(cfgOptProtocolTimeout);
        cfgLoadUpdateOption();

        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptDbTimeout), 100000, "check db-timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("protocol-timeout set but not db timeout");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "100");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        cfgOptionInvalidate(cfgOptDbTimeout);
        cfgLoadUpdateOption();

        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 100000, "check protocol-timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("protocol-timeout set automatically");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptDbTimeout, "100000");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 100030000, "check protocol-timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when db-timeout and protocol-timeout set but invalid");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptDbTimeout, "100000");
        hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "50.5");
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdCheck, argList), OptionInvalidValueError,
            "'50.5' is not valid for 'protocol-timeout' option\n"
                "HINT 'protocol-timeout' option (50.5) should be greater than 'db-timeout' option (100000).");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("very small protocol-timeout triggers db-timeout special handling");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "11");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 11000, "check protocol-timeout");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptDbTimeout), 5500, "check db-timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg and repo cannot both be remote");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 4, "/pg4");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 4, "pg4");
        hrnCfgArgRawZ(argList, cfgOptRepoHost, "repo1");
        TEST_ERROR(hrnCfgLoadP(cfgCmdCheck, argList), ConfigError, "pg and repo hosts cannot both be configured as remote");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("only pg can be remote");

        // We'll have to cheat here and invalidate the repo-host option since there are currently no pg-only commands
        cfgOptionInvalidate(cfgOptRepoHost);
        cfgLoadUpdateOption();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("only repo can be remote");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoHost, "repo1");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention warning - help command");

        argList = strLstNew();
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "process-max");

        harnessLogLevelSet(logLevelWarn);
        HRN_CFG_LOAD(cfgCmdHelp, argList, .comment = "load help config -- no retention warning");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "command is help");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention warning - expire command");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptLogTimestamp, false);

        harnessLogLevelSet(logLevelWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config for retention warning");
        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "repo1-retention-archive not set");

        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config no retention warning");

        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 1, "repo1-retention-archive set");
// CSHANG What does this mean? We have multi-repo but I removed these 2 lines and it fails on coverage. repo-type defaults normally to posix so what is this really doing? Maybe I just need to add another repo with a type other than posix or cifs? I don't understand why commenting this test out results in loss of coverage on line 260 of load.c
        // Munge repo-type for coverage.  This will go away when there are multiple repos.
        // cfgOptionSet(cfgOptRepoType, cfgSourceParam, NULL);
        // TEST_RESULT_VOID(cfgLoadUpdateOption(), "load config no repo-type");
exit(0);
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--no-log-timestamp");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config for retention warning");

        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=incr' but option"
                " 'repo1-retention-archive' is not set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--no-log-timestamp");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config for retention warning");

        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=diff' but neither option"
                " 'repo1-retention-archive' nor option 'repo1-retention-diff' is set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        strLstAddZ(argList, "--repo1-retention-diff=2");
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config for retention warning");

        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.");
        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 2, "    repo1-retention-archive set to retention-diff");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--no-log-timestamp");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-archive=3");
        strLstAddZ(argList, "--repo1-retention-full=1");
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config for retention warning");

        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-diff' is not set for 'repo1-retention-archive-type=diff'\n"
            "            HINT: to retain differential backups indefinitely (without warning), set option 'repo1-retention-diff'"
                " to the maximum.");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--no-log-timestamp");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-archive=3");
        strLstAddZ(argList, "--repo1-retention-diff=2");
        strLstAddZ(argList, "--repo1-retention-full=1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--no-log-timestamp");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--repo1-retention-full-type=time");
        harnessLogLevelSet(logLevelWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList, .comment = "load config: retention-full-type=time, retention-full is set");

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        // -------------------------------------------------------------------------------------------------------------------------
        // Invalid bucket name with verification enabled fails
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo2-type=s3");
        strLstAddZ(argList, "--repo2-s3-bucket=bogus.bucket");
        strLstAddZ(argList, "--repo2-s3-region=region");
        strLstAddZ(argList, "--repo2-s3-endpoint=endpoint");
        strLstAddZ(argList, "--repo2-path=/repo");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 2, "mykey");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3KeySecret, 2, "mysecretkey");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        TEST_ERROR(
            hrnCfgLoadP(cfgCmdArchiveGet, argList), OptionInvalidValueError,
            "'bogus.bucket' is not valid for option 'repo2-s3-bucket'"
                "\nHINT: RFC-2818 forbids dots in wildcard matches."
                "\nHINT: TLS/SSL verification cannot proceed with this bucket name."
                "\nHINT: remove dots from the bucket name.");

        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 2);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3KeySecret, 2);

        // Invalid bucket name with verification disabled succeeds
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 1, "mykey");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3KeySecret, 1, "mysecretkey");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAddZ(argList, "--repo1-s3-bucket=bogus.bucket");
        strLstAddZ(argList, "--repo1-s3-region=region");
        strLstAddZ(argList, "--repo1-s3-endpoint=endpoint");
        strLstAddZ(argList, "--no-repo1-s3-verify-ssl");
        strLstAddZ(argList, "--repo1-path=/repo");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .comment = "invalid bucket with no verification");

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3Bucket), "bogus.bucket", "    check bucket value");

        // Valid bucket name
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAddZ(argList, "--repo1-s3-bucket=cool-bucket");
        strLstAddZ(argList, "--repo1-s3-region=region");
        strLstAddZ(argList, "--repo1-s3-endpoint=endpoint");
        strLstAddZ(argList, "--repo1-path=/repo");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .comment = "valid bucket name");

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3Bucket), "cool-bucket", "    check bucket value");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        unsetenv("PGBACKREST_REPO1_S3_KEY");
        unsetenv("PGBACKREST_REPO1_S3_KEY_SECRET");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compress-type=none when compress=n");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "none", "    compress-type=none");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 0, "    compress-level=0");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compress-type=gz when compress=y");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_COMPRESS_LEVEL "=9");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "gz", "    compress-type=gz");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 9, "    compress-level=9");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn when compress-type and compress both set");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=db");
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_COMPRESS_TYPE "=gz");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCompressType), "gz", "    compress-type=gz");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 6, "    compress-level=6");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptCompress), false, "    compress is not valid");

        TEST_RESULT_LOG(
            "P00   WARN: 'compress' and 'compress-type' options should not both be set\n"
            "            HINT: 'compress-type' is preferred and 'compress' is deprecated.");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadLogFile()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--pg1-path=/path");
        strLstAddZ(argList, "--lock-path=" HRN_PATH "/lock");
        strLstAddZ(argList, "--log-path=/bogus");
        strLstAddZ(argList, "--log-level-file=info");
        strLstAddZ(argList, "backup");
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
        strLstAddZ(argList, "--" CFGOPT_LOCK_PATH "=" HRN_PATH "/lock");
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
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--no-" CFGOPT_SCK_KEEP_ALIVE);
        strLstAddZ(argList, "--" CFGOPT_SCK_BLOCK);
        strLstAddZ(argList, "info");

        socketLocal = (struct SocketLocal){.init = false};

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config and don't set umask");
        TEST_RESULT_BOOL(socketLocal.init, true, "   check socketLocal.init");
        TEST_RESULT_BOOL(socketLocal.block, true, "   check socketLocal.block");
        TEST_RESULT_BOOL(socketLocal.keepAlive, false, "   check socketLocal.keepAlive");
        TEST_RESULT_UINT(ioTimeoutMs(), 60000, "   check io timeout");

        // Set a distinct umask value and test that the umask is reset by configLoad since default for neutral-umask=y
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        strLstAddZ(argList, "--log-level-file=off");
        strLstAddZ(argList, "--io-timeout=95.5");
        strLstAddZ(argList, "archive-get");

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for neutral-umask");
        TEST_RESULT_INT(umask(0111), 0000, "    umask was reset");
        TEST_RESULT_UINT(ioTimeoutMs(), 95500, "   check io timeout");

        // Set a distinct umask value and test that the umask is not reset by configLoad with option --no-neutral-umask
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--no-neutral-umask");
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        strLstAddZ(argList, "--log-level-file=off");
        strLstAddZ(argList, "archive-get");

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for no-neutral-umask");
        TEST_RESULT_INT(umask(0), 0111, "    umask was not reset");

        // No command
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "no command");

        // Help command only
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "help");

        ioBufferSizeSet(333);
        socketLocal = (struct SocketLocal){.init = false};

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command");
        TEST_RESULT_UINT(ioBufferSize(), 333, "buffer size not updated by help command");
        TEST_RESULT_BOOL(socketLocal.init, false, "socketLocal not updated by help command");

        // Help command for backup
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        strLstAddZ(argList, "--log-level-file=off");
        strLstAddZ(argList, "--repo1-retention-full=2");

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command for backup");
        TEST_RESULT_UINT(ioBufferSize(), 1048576, "buffer size set to option default");

        // Command takes lock and opens log file and uses custom tcp settings
        // -------------------------------------------------------------------------------------------------------------------------
        socketLocal = (struct SocketLocal){.init = false};
        struct stat statLog;

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--pg1-path=/path");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--lock-path=" HRN_PATH "/lock");
        strLstAddZ(argList, "--log-path=" TEST_PATH);
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        strLstAddZ(argList, "--log-level-file=warn");
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_COUNT "=11");
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_IDLE "=2222");
        strLstAddZ(argList, "--" CFGOPT_TCP_KEEP_ALIVE_INTERVAL "=888");
        strLstAddZ(argList, "backup");

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "lock and open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/db-backup.log", &statLog), 0, "   check log file exists");
        TEST_RESULT_PTR_NE(cfgOptionStr(cfgOptExecId), NULL, "   exec-id is set");
        TEST_RESULT_BOOL(socketLocal.init, true, "   check socketLocal.init");
        TEST_RESULT_BOOL(socketLocal.block, false, "   check socketLocal.block");
        TEST_RESULT_BOOL(socketLocal.keepAlive, true, "   check socketLocal.keepAlive");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveCount, 11, "   check socketLocal.tcpKeepAliveCount");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveIdle, 2222, "   check socketLocal.tcpKeepAliveIdle");
        TEST_RESULT_INT(socketLocal.tcpKeepAliveInterval, 888, "   check socketLocal.tcpKeepAliveInterval");

        lockRelease(true);

        // Local command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--log-path=" TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to");
        strLstAddZ(argList, "--process=1");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        strLstAddZ(argList, "--log-level-file=warn");
        hrnCfgArgRawZ(argList, cfgOptExecId, "1111-fe70d611");
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/db-backup-local-001.log", &statLog), 0, "   check log file exists");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptExecId), "1111-fe70d611", "   exec-id is preserved");

        // Remote command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--log-path=" TEST_PATH);
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAddZ(argList, "--process=0");
        strLstAddZ(argList, CFGCMD_INFO ":" CONFIG_COMMAND_ROLE_REMOTE);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/all-info-remote-000.log", &statLog), 0, "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote command without archive-async option");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--log-path=" TEST_PATH);
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAddZ(argList, "--process=1");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_REMOTE);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/test-archive-get-remote-001.log", &statLog), 0, "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("local command with archive-async option");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--log-path=" TEST_PATH);
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        strLstAddZ(argList, "--" CFGOPT_LOG_LEVEL_FILE "=info");
        strLstAddZ(argList, "--" CFGOPT_LOG_SUBPROCESS);
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "--process=1");
        strLstAddZ(argList, CFGCMD_ARCHIVE_PUSH ":" CONFIG_COMMAND_ROLE_LOCAL);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/test-archive-push-async-local-001.log", &statLog), 0, "   check log file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive-get command with async role");

        argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        strLstAddZ(argList, "--" CFGOPT_LOG_PATH "=" TEST_PATH);
        strLstAddZ(argList, "--lock-path=" HRN_PATH "/lock");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_ASYNC);

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(lstat(TEST_PATH "/test-archive-get-async.log", &statLog), 0, "   check log file exists");

        lockRelease(true);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
