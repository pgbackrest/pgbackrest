/***********************************************************************************************************************************
Harness for Creating Test Backups
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/backup.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/lock.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "storage/helper.h"

#include "common/harnessBackup.h"
#include "common/harnessDebug.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessStorage.h"
#include "common/harnessTest.h"

/**********************************************************************************************************************************/
void
hrnCmdBackup(void)
{
    FUNCTION_HARNESS_VOID();

    lockInit(STR(testPath()), cfgOptionStr(cfgOptExecId), cfgOptionStr(cfgOptStanza), lockTypeBackup);
    lockAcquireP();

    TRY_BEGIN()
    {
        cmdBackup();
    }
    FINALLY()
    {
        lockRelease(true);
    }
    TRY_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnBackupPqScript(const unsigned int pgVersion, const time_t backupTimeStart, HrnBackupPqScriptParam param)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *const pg1Path = zNewFmt("%s/pg1", testPath());
        const char *const pg2Path = zNewFmt("%s/pg2", testPath());

        // If no timeline specified then use timeline 1
        param.timeline = param.timeline == 0 ? 1 : param.timeline;

        // Read pg_control to get info about the cluster
        PgControl pgControl = pgControlFromFile(storagePg(), param.pgVersionForce);

        // Set archive timeout really small to save time on errors
        cfgOptionSet(cfgOptArchiveTimeout, cfgSourceParam, varNewInt64(100));

        // Set LSN and WAL start/stop
        uint64_t lsnStart = ((uint64_t)backupTimeStart & 0xFFFFFF00) << 28;
        uint64_t lsnStop =
            lsnStart + ((uint64_t)(param.walTotal == 0 ? 0 : param.walTotal - 1) * (uint64_t)pgControl.walSegmentSize) +
            (uint64_t)(pgControl.walSegmentSize / 2);

        const char *walSegmentPrior = strZ(
            pgLsnToWalSegment(param.timeline, lsnStart - pgControl.walSegmentSize, pgControl.walSegmentSize));
        const char *lsnStartStr = strZ(pgLsnToStr(lsnStart));
        const char *walSegmentStart = strZ(pgLsnToWalSegment(param.timeline, lsnStart, pgControl.walSegmentSize));
        const char *lsnStopStr = strZ(pgLsnToStr(lsnStop));
        const char *walSegmentStop = strZ(pgLsnToWalSegment(param.timeline, lsnStop, pgControl.walSegmentSize));

        // Save pg_control with updated info
        pgControl.checkpoint = lsnStart;
        pgControl.timeline = param.timeline;

        HRN_STORAGE_PUT(
            storagePgIdxWrite(0), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, hrnPgControlToBuffer(0, 0, pgControl),
            .timeModified = backupTimeStart);

        // Update pg_control on primary with the backup time
        HRN_PG_CONTROL_TIME(storagePgIdxWrite(0), backupTimeStart);

        // Write WAL segments to the archive
        // -------------------------------------------------------------------------------------------------------------------------
        if (!param.noPriorWal)
        {
            InfoArchive *infoArchive = infoArchiveLoadFile(
                storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, param.cipherType == 0 ? cipherTypeNone : param.cipherType,
                param.cipherPass == NULL ? NULL : STR(param.cipherPass));
            const String *archiveId = infoArchiveId(infoArchive);
            StringList *walSegmentList = pgLsnRangeToWalSegmentList(
                param.timeline, lsnStart - pgControl.walSegmentSize, param.noWal ? lsnStart - pgControl.walSegmentSize : lsnStop,
                pgControl.walSegmentSize);

            Buffer *walBuffer = bufNew((size_t)pgControl.walSegmentSize);
            bufUsedSet(walBuffer, bufSize(walBuffer));
            memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
            HRN_PG_WAL_TO_BUFFER(walBuffer, pgControl.version, .systemId = pgControl.systemId);
            const String *walChecksum = strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, walBuffer));

            for (unsigned int walSegmentIdx = 0; walSegmentIdx < strLstSize(walSegmentList); walSegmentIdx++)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    StorageWrite *write = storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s-%s%s", strZ(archiveId), strZ(strLstGet(walSegmentList, walSegmentIdx)),
                            strZ(walChecksum), strZ(compressExtStr(param.walCompressType))));

                    if (param.walCompressType != compressTypeNone)
                        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), compressFilterP(param.walCompressType, 1));

                    storagePutP(write, walBuffer);
                }
                MEM_CONTEXT_TEMP_END();
            }
        }

        // Generate pq script
        // -------------------------------------------------------------------------------------------------------------------------
        // Connect to primary
        if (pgVersion <= PG_VERSION_95)
            HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", pgVersion, pg1Path, false, NULL, NULL));
        else
            HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", pgVersion, pg1Path, false, NULL, NULL));

        // Connect to standby
        if (param.backupStandby)
        {
            // Save standby pg_control with updated info
            HRN_STORAGE_PUT(storagePgIdxWrite(1), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, hrnPgControlToBuffer(0, 0, pgControl));

            if (pgVersion <= PG_VERSION_95)
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_OPEN_GE_93(2, "dbname='postgres' port=5433", pgVersion, pg2Path, true, NULL, NULL));
            else
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_OPEN_GE_96(2, "dbname='postgres' port=5433", pgVersion, pg2Path, true, NULL, NULL));
        }

        // Get start time
        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TIME_QUERY(1, (int64_t)backupTimeStart * 1000));

        // Advisory lock
        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_ADVISORY_LOCK(1, true));

        // Check if backup is in progress (only for exclusive backup)
        if (pgVersion <= PG_VERSION_95)
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_IN_BACKUP(1, false));

        // Perform archive check
        if (!param.noArchiveCheck)
        {
            const char *const walSegment = param.walSwitch ? walSegmentStart : walSegmentPrior;

            if (pgVersion <= PG_VERSION_96)
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_CURRENT_WAL_LE_96(1, walSegment));
            else
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_CURRENT_WAL_GE_10(1, walSegment));
        }

        // Start backup
        if (pgVersion <= PG_VERSION_95)
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_START_BACKUP_LE_95(1, param.startFast, lsnStartStr, walSegmentStart));
        else if (pgVersion <= PG_VERSION_96)
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_START_BACKUP_96(1, param.startFast, lsnStartStr, walSegmentStart));
        else
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_START_BACKUP_GE_10(1, param.startFast, lsnStartStr, walSegmentStart));

        // Switch WAL segment so it can be checked
        if (param.walSwitch)
        {
            HRN_PQ_SCRIPT_ADD(
                HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(1, "X/X"),
                HRN_PQ_SCRIPT_WAL_SWITCH(1, "wal", walSegmentStart));
        }

        // Get database list
        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_DATABASE_LIST_1(1, "test1"));

        // Get tablespace list
        if (param.tablespace)
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TABLESPACE_LIST_1(1, 32768, "tblspc32768"));
        else
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TABLESPACE_LIST_0(1));

        // Wait for standby to sync
        if (param.backupStandby)
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_REPLAY_WAIT_96(2, lsnStartStr));

        // Continue if WAL check succeeds
        if (!param.noPriorWal)
        {
            // Get copy start time
            HRN_PQ_SCRIPT_ADD(
                HRN_PQ_SCRIPT_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
                HRN_PQ_SCRIPT_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000));

            // Continue if there is no error after start
            if (!param.errorAfterStart)
            {
                // Ping to check standby mode
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));

                if (param.backupStandby)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));

                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));

                if (param.backupStandby)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));

                // Stop backup
                if (pgVersion <= PG_VERSION_95)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(1, lsnStopStr, walSegmentStop));
                else if (pgVersion <= PG_VERSION_96)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_96(1, lsnStopStr, walSegmentStop, false));
                else
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_GE_10(1, lsnStopStr, walSegmentStop, true));

                // Get stop time
                HRN_PQ_SCRIPT_ADD(
                    HRN_PQ_SCRIPT_TIME_QUERY(1, ((int64_t)backupTimeStart + (param.noArchiveCheck ? 52427 : 2)) * 1000));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();
}
