/***********************************************************************************************************************************
Real Integration Test
***********************************************************************************************************************************/
#include <utime.h>

#include "common/crypto/common.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

#include "common/harnessErrorRetry.h"
#include "common/harnessHost.h"
#include "common/harnessPostgres.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test definition
***********************************************************************************************************************************/
static HrnHostTestDefine testMatrix[] =
{
    // {uncrustify_off - struct alignment}
    {.pg = "9.5", .repo = "repo", .tls = 1, .stg =    "s3", .enc = 0, .cmp =  "bz2", .rt = 1, .bnd = 1, .bi = 1, .fi = 1},
    {.pg = "9.6", .repo = "repo", .tls = 0, .stg = "azure", .enc = 0, .cmp = "none", .rt = 2, .bnd = 1, .bi = 1, .fi = 0},
    {.pg =  "10", .repo =  "pg2", .tls = 0, .stg =  "sftp", .enc = 1, .cmp =   "gz", .rt = 1, .bnd = 1, .bi = 0, .fi = 1},
    {.pg =  "11", .repo = "repo", .tls = 1, .stg =   "gcs", .enc = 0, .cmp =  "zst", .rt = 2, .bnd = 0, .bi = 0, .fi = 1},
    {.pg =  "12", .repo = "repo", .tls = 0, .stg =    "s3", .enc = 1, .cmp =  "lz4", .rt = 1, .bnd = 1, .bi = 1, .fi = 0},
    {.pg =  "13", .repo =  "pg2", .tls = 1, .stg = "posix", .enc = 0, .cmp = "none", .rt = 1, .bnd = 0, .bi = 0, .fi = 0},
    {.pg =  "14", .repo = "repo", .tls = 0, .stg =   "gcs", .enc = 0, .cmp =  "lz4", .rt = 1, .bnd = 1, .bi = 0, .fi = 1},
    {.pg =  "15", .repo =  "pg2", .tls = 0, .stg = "azure", .enc = 1, .cmp = "none", .rt = 2, .bnd = 1, .bi = 1, .fi = 0},
    {.pg =  "16", .repo = "repo", .tls = 0, .stg =  "sftp", .enc = 0, .cmp =  "zst", .rt = 1, .bnd = 1, .bi = 1, .fi = 1},
    {.pg =  "17", .repo = "repo", .tls = 0, .stg = "posix", .enc = 0, .cmp = "none", .rt = 1, .bnd = 0, .bi = 0, .fi = 0},
    // {uncrustify_on}
};

/***********************************************************************************************************************************
Test statuses
***********************************************************************************************************************************/
#define TEST_STATUS_FULL                                            "full"
#define TEST_STATUS_INCR                                            "incr"
#define TEST_STATUS_NAME                                            "name"
#define TEST_STATUS_STANDBY                                         "standby"
#define TEST_STATUS_TIME                                            "time"
#define TEST_STATUS_TIMELINE                                        "timeline"
#define TEST_STATUS_XID                                             "xid"

#define TEST_RESTORE_POINT                                          "pgbackrest"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Enable error retry detail. This works because we are not trying to check exact error messages.
    hrnErrorRetryDetailEnable();

    // *****************************************************************************************************************************
    if (testBegin("integration"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("build hosts");

        HRN_HOST_BUILD(testMatrix);

        HrnHost *const pg1 = hrnHostPg1();
        HrnHost *const pg2 = hrnHostPg2();
        HrnHost *const repo = hrnHostRepo();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create pg cluster");
        {
            HRN_HOST_PG_CREATE(pg1);
            TEST_HOST_BR(repo, CFGCMD_STANZA_CREATE);

            // Create tablespace
            HRN_STORAGE_PATH_CREATE(hrnHostDataStorage(pg1), strZ(hrnHostPgTsPath(pg1)), .mode = 0700);
            HRN_HOST_SQL_EXEC(pg1, zNewFmt("create tablespace ts1 location '%s'", strZ(hrnHostPgTsPath(pg1))));

            // Init status table
            HRN_HOST_SQL_EXEC(pg1, "create table status (message text not null) tablespace ts1");
            HRN_HOST_WAL_SWITCH(pg1);
            HRN_HOST_SQL_EXEC(pg1, "insert into status values ('" TEST_STATUS_FULL "')");
        }

        // Get ts1 tablespace oid
        const unsigned int ts1Oid = pckReadU32P(hrnHostSqlValue(pg1, "select oid from pg_tablespace where spcname = 'ts1'"));
        TEST_LOG_FMT("ts1 tablespace oid = %u", ts1Oid);

        // When full/incr is enabled, set some modified timestamps in the past so full/incr will find some files
        if (hrnHostFullIncr())
        {
            const StringList *const fileList = storageListP(hrnHostPgStorage(pg1), STRDEF("base/1"));
            const time_t modified = time(NULL) - SEC_PER_DAY * 2;

            for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
            {
                const char *const pathFull = strZ(
                    storagePathP(hrnHostPgStorage(pg1), strNewFmt("base/1/%s", strZ(strLstGet(fileList, fileIdx)))));

                THROW_ON_SYS_ERROR_FMT(
                    utime(pathFull, &((struct utimbuf){.actime = modified, .modtime = modified})) == -1, FileInfoError,
                    "unable to set time for '%s'", pathFull);
            }
        }

        // Get the tablespace path to use for this version. We could use our internally stored catalog number but during the beta
        // period this number will be changing and would need to be updated. Make this less fragile by just reading the path.
        const String *const tablespacePath = strLstGet(
            storageListP(hrnHostPgStorage(pg1), strNewFmt(PG_PATH_PGTBLSPC "/%u", ts1Oid)), 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check hosts (skip pg2 for now)");
        {
            if (pg1 != repo)
                TEST_HOST_BR(pg1, CFGCMD_CHECK);

            if (pg2 != repo)
                TEST_HOST_BR(repo, CFGCMD_CHECK);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary full backup");
        {
            // Full backup to repo 1
            TEST_HOST_BR(repo, CFGCMD_BACKUP, .option = "--type=full --buffer-size=16KiB");

            // Full backup to repo 2
            if (hrnHostRepoTotal() == 2)
                TEST_HOST_BR(repo, CFGCMD_BACKUP, .option = "--type=full --repo=2");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby restore");
        {
            // If the repo is running on pg2 then switch the user to root to test restoring as root. Use this configuration so the
            // repo is local and root does not need to be configured for TLS or SSH.
            const char *user = NULL;

            if (pg2 == repo)
            {
                user = "root";

                // Create the pg/ts directory so it will not be created with root permissions
                HRN_STORAGE_PATH_CREATE(hrnHostDataStorage(pg2), strZ(hrnHostPgTsPath(pg2)), .mode = 0700);

                // Create the restore log file so it will not be created with root permissions
                HRN_STORAGE_PUT_EMPTY(hrnHostDataStorage(pg2), zNewFmt("%s/test-restore.log", strZ(hrnHostLogPath(pg2))));
            }

            // Run restore
            const char *option = zNewFmt("--type=standby --tablespace-map=ts1='%s'", strZ(hrnHostPgTsPath(pg2)));
            TEST_HOST_BR(pg2, CFGCMD_RESTORE, .option = option, .user = user);

            // If the repo is running on pg2 then perform additional ownership tests
            if (pg2 == repo)
            {
                // Update some ownership to root
                hrnHostExecP(
                    pg2,
                    strNewFmt(
                        "chown %s:%s %s &&"
                        "chown %s:%s %s/" PG_PATH_GLOBAL " &&"
                        "chown %s:%s %s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
                        user, user, strZ(hrnHostPgDataPath(pg2)), user, user, strZ(hrnHostPgDataPath(pg2)), user, user,
                        strZ(hrnHostPgDataPath(pg2))),
                    .user = STRDEF(user));

                // Expect an error when running without root since permissions cannot be updated
                TEST_HOST_BR(
                    pg2, CFGCMD_RESTORE, .option = zNewFmt("--delta %s", option), .user = NULL,
                    .resultExpect = errorTypeCode(&FileOpenError));

                // Running as root fixes the ownership
                TEST_HOST_BR(pg2, CFGCMD_RESTORE, .option = zNewFmt("--delta %s", option), .user = user);
            }

            HRN_HOST_PG_START(pg2);

            // Promote the standby to create a new timeline that can be used to test timeline verification. Once the new timeline
            // has been created restore again to get the standby back on the same timeline as the primary.
            HRN_HOST_PG_PROMOTE(pg2);
            TEST_STORAGE_EXISTS(
                hrnHostRepo1Storage(repo),
                zNewFmt("archive/" HRN_STANZA "/%s-1/00000002.history", strZ(pgVersionToStr(hrnHostPgVersion()))), .timeout = 5000);

            HRN_HOST_PG_STOP(pg2);
            TEST_HOST_BR(pg2, CFGCMD_RESTORE, .option = zNewFmt("%s --delta --target-timeline=current", option));
            HRN_HOST_PG_START(pg2);

            // Check standby
            TEST_HOST_BR(pg2, CFGCMD_CHECK);

            // Check that backup recovered completely
            TEST_HOST_SQL_ONE_STR_Z(pg2, "select message from status", TEST_STATUS_FULL);

            // Update message for standby
            HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_STANDBY "'");

            // Check that standby is streaming from the primary
            TEST_HOST_SQL_ONE_STR_Z(
                pg1,
                zNewFmt(
                    "select client_addr || '-' || state from pg_stat_replication where client_addr = '%s'",
                    strZ(hrnHostIp(pg2))),
                zNewFmt("%s/32-streaming", strZ(hrnHostIp(pg2))));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby full backup and expire");
        {
            // Check backups before the backup so we know how many will exist after
            const InfoBackup *infoBackup = infoBackupLoadFile(
                hrnHostRepo1Storage(repo), STRDEF("backup/" HRN_STANZA "/backup.info"), hrnHostCipherType(), hrnHostCipherPass());
            TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 1, "backup total = 1");

            TEST_HOST_BR(repo, CFGCMD_BACKUP, .option = "--type=full --backup-standby --repo1-retention-full=1 --no-expire-auto");

            // Expire was disabled so the backup total has increased
            infoBackup = infoBackupLoadFile(
                hrnHostRepo1Storage(repo), STRDEF("backup/" HRN_STANZA "/backup.info"), hrnHostCipherType(), hrnHostCipherPass());
            TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "backup total = 2");

            // Now force an expire
            TEST_HOST_BR(repo, CFGCMD_EXPIRE, .option = "--repo1-retention-full=1");

            // Backup has been expired
            infoBackup = infoBackupLoadFile(
                hrnHostRepo1Storage(repo), STRDEF("backup/" HRN_STANZA "/backup.info"), hrnHostCipherType(), hrnHostCipherPass());
            TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 1, "backup total = 1");

            // Stop the standby since restores to primary will break it
            HRN_HOST_PG_STOP(pg2);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("exercise async archiving");

        hrnHostConfigUpdateP(.archiveAsync = BOOL_TRUE_VAR);

        HRN_HOST_SQL_EXEC(pg1, "create table wal_activity (id int)");
        HRN_HOST_WAL_SWITCH(pg1);

        HRN_HOST_SQL_EXEC(pg1, "insert into wal_activity values (1)");
        HRN_HOST_WAL_SWITCH(pg1);
        HRN_HOST_SQL_EXEC(pg1, "insert into wal_activity values (2)");
        HRN_HOST_WAL_SWITCH(pg1);
        HRN_HOST_SQL_EXEC(pg1, "insert into wal_activity values (3)");
        HRN_HOST_WAL_SWITCH(pg1);
        HRN_HOST_SQL_EXEC(pg1, "insert into wal_activity values (4)");
        HRN_HOST_WAL_SWITCH(pg1);

        TEST_STORAGE_EXISTS(
            hrnHostDataStorage(pg1), zNewFmt("%s/" HRN_STANZA "-archive-push-async.log", strZ(hrnHostLogPath(pg1))),
            .timeout = 5000);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("setup time target");

        // If the tests are running quickly then the time target might end up the same as the end time of the prior full backup.
        // That means restore auto-select will not pick it as a candidate and restore the last backup instead causing the restore
        // compare to fail. So, sleep one second.
        sleepMSec(1000);

        HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_TIME "'");
        HRN_HOST_WAL_SWITCH(pg1);

        const char *const targetTime = strZ(pckReadStrP(hrnHostSqlValue(pg1, "select current_timestamp::text")));
        TEST_LOG_FMT("time target = %s", targetTime);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary incr delta backup");
        {
            // Create a database that can be excluded from restores
            HRN_HOST_SQL_EXEC(pg1, "create database exclude_me with tablespace ts1");

            // Check that backup fails for <= 9.5 when another backup is already running
            if (hrnHostPgVersion() <= PG_VERSION_95)
            {
                HRN_HOST_SQL_EXEC(pg1, "perform pg_start_backup('test backup that will be restarted', true)");
                TEST_HOST_BR(repo, CFGCMD_BACKUP, .resultExpect = errorTypeCode(&DbQueryError));
            }

            // Backup from pg1 to show that backups can be done from the primary when a repo host exists, that a primary backup
            // works after a standby backup, and that disabling bundling and block incremental works. Include stop auto here so
            // backups for <= 9.5 will stop the prior backup
            HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_INCR "'");
            TEST_HOST_BR(pg1, CFGCMD_BACKUP, .option = "--type=incr --delta --stop-auto");

            // Check that expire works remotely (increase full retention to make it a noop)
            TEST_HOST_BR(pg1, CFGCMD_EXPIRE, .option = "--repo1-retention-full=99");
        }

        // Get exclude_me database oid
        const unsigned int excludeMeOid = pckReadU32P(
            hrnHostSqlValue(pg1, "select oid from pg_database where datname = 'exclude_me'"));
        TEST_LOG_FMT("exclude_me database oid = %u", excludeMeOid);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("setup xid target");

        hrnHostSqlBegin(pg1);
        HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_XID "'");
        HRN_HOST_WAL_SWITCH(pg1);

        const char *const targetXid = strZ(pckReadStrP(hrnHostSqlValue(pg1, "select txid_current()::text")));
        TEST_LOG_FMT("xid target = %s", targetXid);
        hrnHostSqlCommit(pg1);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("setup name target");

        HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_NAME "'");
        HRN_HOST_WAL_SWITCH(pg1);

        HRN_HOST_SQL_EXEC(pg1, "perform pg_create_restore_point('" TEST_RESTORE_POINT "')");
        TEST_LOG("name target = " TEST_RESTORE_POINT);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore fails on timeline verification");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Restore fails because timeline 2 was created before the backup selected for restore. Specify target timeline latest
            // because PostgreSQL < 12 defaults to current.
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE, .option = "--delta --target-timeline=latest", .resultExpect = errorTypeCode(&DbMismatchError));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (default target)");
        {
            // Restore on current timeline to skip the invalid timeline unrelated to the backup
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt("--force --target-timeline=current --repo=%u", hrnHostRepoTotal()));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_NAME);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (immediate target)");
        {
            // Expect failure when pg is running
            TEST_HOST_BR(pg1, CFGCMD_RESTORE, .resultExpect = errorTypeCode(&PgRunningError));

            // Stop the cluster and try again
            HRN_HOST_PG_STOP(pg1);

            // Restore immediate and promote -- this avoids checking the invalid timeline since immediate recovery is always along
            // the current timeline. The promotion will create a new timeline so subsequent tests will pass timeline verification.
            TEST_HOST_BR(pg1, CFGCMD_RESTORE, .option = "--delta --type=immediate --target-action=promote --db-exclude=exclude_me");
            HRN_HOST_PG_START(pg1);

            // Test that the exclude_me database has a zeroed pg_filenode.map
            const Buffer *const pgFileNodeMap = storageGetP(
                storageNewReadP(
                    hrnHostPgStorage(pg1),
                    strNewFmt(PG_PATH_PGTBLSPC "/%u/%s/%u/" PG_FILE_PGFILENODEMAP, ts1Oid, strZ(tablespacePath), excludeMeOid)));

            Buffer *const pgFileNodeMapZero = bufNew(bufUsed(pgFileNodeMap));
            memset(bufPtr(pgFileNodeMapZero), 0, bufSize(pgFileNodeMapZero));
            bufUsedSet(pgFileNodeMapZero, bufSize(pgFileNodeMapZero));

            TEST_RESULT_BOOL(bufEq(pgFileNodeMap, pgFileNodeMapZero), true, "exclude_me db " PG_FILE_PGFILENODEMAP " is zeroed");

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_INCR);

            // It should be possible to drop the exclude_me database even though it was not restored
            HRN_HOST_SQL_EXEC(pg1, "drop database exclude_me");
        }

        // From here on restores need to specify the current timeline for > 11 for recovery to be reliable
        const char *const targetTimeline = hrnHostPgVersion() <= PG_VERSION_11 ? "" : " --target-timeline=current";

        // Be careful about moving the order of this restore since the timeline created is used in a later test
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (xid target)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Restore
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt(
                    "--delta --type=xid --target=%s --target-action=promote --repo=%u%s", targetXid, hrnHostRepoTotal(),
                    targetTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_XID);

            // Update status to test following a specified timeline
            HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_TIMELINE "'");
            HRN_HOST_WAL_SWITCH(pg1);
        }

        // Store the timeline so it can be used in a later test
        const char *const xidTimeline = strZ(
            pckReadStrP(
                hrnHostSqlValue(
                    pg1,
                    zNewFmt(
                        "select trim(leading '0' from substring(pg_%sfile_name('1/1'), 1, 8))",
                        strZ(pgWalName(hrnHostPgVersion()))))));
        TEST_LOG_FMT("xid timeline = %s", xidTimeline);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (time target, auto-select backup)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Restore
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt("--delta --type=time --target='%s'%s", targetTime, targetTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_TIME);
        }

        // Store the time where the entire timeline captured in xidTimeline above exists so it can be used in a later test. We need
        // to capture this here (instead of above) to be sure that all the timeline WAL has been archived.
        const char *const xidTime = strZ(pckReadStrP(hrnHostSqlValue(pg1, "select current_timestamp::text")));
        TEST_LOG_FMT("xid time = %s", xidTime);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (time xid, exclusive)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Restore
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt(
                    "--delta --type=xid --target='%s' --target-exclusive --repo=%u%s", targetXid, hrnHostRepoTotal(),
                    targetTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_INCR);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (name)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Restore
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE, .option = zNewFmt("--delta --type=name --target='" TEST_RESTORE_POINT "'%s", targetTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_NAME);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (default, timeline created by type = xid)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // If repo is versioned then delete the repo to test repo-target-time
            if (hrnHostRepoVersioning())
            {
                // Stop pgbackrest
                TEST_HOST_BR(repo, CFGCMD_STOP);

                // Delete stanza
                TEST_HOST_BR(repo, CFGCMD_STANZA_DELETE);
            }

            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt(
                    "--delta --type=standby --target-timeline=%s%s", xidTimeline,
                    hrnHostRepoVersioning() ? zNewFmt(" --repo=1 --repo-target-time=\"%s\"", xidTime) : ""));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_TIMELINE);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        if (!hrnHostRepoVersioning() && hrnHostNonVersionSpecific())
        {
            TEST_TITLE("stanza-delete --force with pgbackrest stopped");

            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Stop pgbackrest
            TEST_HOST_BR(pg1, CFGCMD_STOP);
            TEST_HOST_BR(repo, CFGCMD_STOP);

            // Delete stanza
            TEST_HOST_BR(repo, CFGCMD_STANZA_DELETE, .option = "--force");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
