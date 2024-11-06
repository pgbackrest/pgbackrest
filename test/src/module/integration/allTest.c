/***********************************************************************************************************************************
Real Integration Test
***********************************************************************************************************************************/
#include "common/crypto/common.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "postgres/interface.h"
#include "postgres/version.h"

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
    {.pg = "9.4", .repo =  "pg2", .tls = 0, .stg = "azure", .enc = 1, .cmp =  "lz4", .rt = 1, .bnd = 1, .bi = 0},
    {.pg = "9.5", .repo = "repo", .tls = 1, .stg =    "s3", .enc = 0, .cmp =  "bz2", .rt = 1, .bnd = 1, .bi = 1},
    {.pg = "9.6", .repo = "repo", .tls = 0, .stg = "posix", .enc = 0, .cmp = "none", .rt = 2, .bnd = 1, .bi = 1},
    {.pg =  "10", .repo =  "pg2", .tls = 0, .stg =  "sftp", .enc = 1, .cmp =   "gz", .rt = 1, .bnd = 1, .bi = 0},
    {.pg =  "11", .repo = "repo", .tls = 1, .stg =   "gcs", .enc = 0, .cmp =  "zst", .rt = 2, .bnd = 0, .bi = 0},
    {.pg =  "12", .repo = "repo", .tls = 0, .stg =    "s3", .enc = 1, .cmp =  "lz4", .rt = 1, .bnd = 1, .bi = 1},
    {.pg =  "13", .repo =  "pg2", .tls = 1, .stg =  "sftp", .enc = 0, .cmp =  "zst", .rt = 1, .bnd = 1, .bi = 1},
    {.pg =  "14", .repo = "repo", .tls = 0, .stg =   "gcs", .enc = 0, .cmp =  "lz4", .rt = 1, .bnd = 1, .bi = 0},
    {.pg =  "15", .repo =  "pg2", .tls = 0, .stg = "azure", .enc = 0, .cmp = "none", .rt = 2, .bnd = 1, .bi = 1},
    {.pg =  "16", .repo = "repo", .tls = 0, .stg = "posix", .enc = 0, .cmp = "none", .rt = 1, .bnd = 0, .bi = 0},
    {.pg =  "17", .repo = "repo", .tls = 0, .stg = "posix", .enc = 0, .cmp = "none", .rt = 1, .bnd = 0, .bi = 0},
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

#define UPDATE_SPOOL_PATH() \
    hrnHostConfigUpdateP(.archiveAsync = BOOL_TRUE_VAR, .spoolPathPostfix = VARSTR(STRDEF(STRINGIFY(__COUNTER__))))

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
            TEST_HOST_BR(
                pg2, CFGCMD_RESTORE, .option = zNewFmt("--type=standby --tablespace-map=ts1='%s'", strZ(hrnHostPgTsPath(pg2))));
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
            hrnHostDataStorage(pg1), zNewFmt("%s/" HRN_STANZA "-archive-push-async.log", strZ(hrnHostLogPath(pg1))));

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

            // Include stop auto here so backups for <= 9.5 will stop the prior backup
            HRN_HOST_SQL_EXEC(pg1, "update status set message = '" TEST_STATUS_INCR "'");
            TEST_HOST_BR(repo, CFGCMD_BACKUP, .option = "--type=incr --delta --stop-auto");
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
        TEST_TITLE("primary restore (default target)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Set unique spool path
            UPDATE_SPOOL_PATH();

            // Restore
            TEST_HOST_BR(pg1, CFGCMD_RESTORE, .option = zNewFmt("--force --repo=%u", hrnHostRepoTotal()));
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

            // Set unique spool path
            UPDATE_SPOOL_PATH();

            // Restore
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

            // Set unique spool path
            UPDATE_SPOOL_PATH();

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

        // Store the time so it can be used in a later test
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

            // Set unique spool path
            UPDATE_SPOOL_PATH();

            // Restore
            TEST_HOST_BR(
                pg1, CFGCMD_RESTORE,
                .option = zNewFmt("--delta --type=time --target='%s'%s", targetTime, targetTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_TIME);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary restore (time xid, exclusive)");
        {
            // Stop the cluster
            HRN_HOST_PG_STOP(pg1);

            // Set unique spool path
            UPDATE_SPOOL_PATH();

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

            // Set unique spool path
            UPDATE_SPOOL_PATH();

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

            // Set unique spool path
            UPDATE_SPOOL_PATH();

            TEST_HOST_BR(pg1, CFGCMD_RESTORE, .option = zNewFmt("--delta --type=standby --target-timeline=%s", xidTimeline));
            HRN_HOST_PG_START(pg1);

            // Check that backup recovered to the expected target
            TEST_HOST_SQL_ONE_STR_Z(pg1, "select message from status", TEST_STATUS_TIMELINE);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        if (hrnHostNonVersionSpecific())
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
