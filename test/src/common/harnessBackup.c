/***********************************************************************************************************************************
Harness for Creating Test Backups
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/backup.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessBackup.h"
#include "common/harnessDebug.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessStorage.h"
#include "common/harnessTest.intern.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct HrnBackupLocal
{
    MemContext *memContext;                                         // Script mem context

    // Script that defines how shim functions operate
    HrnBackupScript script[1024];
    unsigned int scriptSize;
    unsigned int scriptIdx;
} hrnBackupLocal;

/**********************************************************************************************************************************/
static void
hrnBackupScriptAdd(const HrnBackupScript *const script, const unsigned int scriptSize)
{
    if (scriptSize == 0)
        THROW(AssertError, "backup script must have entries");

    if (hrnBackupLocal.scriptSize == 0)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(hrnBackupLocal, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                hrnBackupLocal.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();

        hrnBackupLocal.scriptIdx = 0;
    }

    // Copy records into local storage
    MEM_CONTEXT_BEGIN(hrnBackupLocal.memContext)
    {
        for (unsigned int scriptIdx = 0; scriptIdx < scriptSize; scriptIdx++)
        {
            ASSERT(script[scriptIdx].op != 0);
            ASSERT(script[scriptIdx].file != NULL);

            hrnBackupLocal.script[hrnBackupLocal.scriptSize] = script[scriptIdx];
            hrnBackupLocal.script[hrnBackupLocal.scriptSize].file = strDup(script[scriptIdx].file);
            hrnBackupLocal.script[hrnBackupLocal.scriptSize].exec = script[scriptIdx].exec == 0 ? 1 : script[scriptIdx].exec;

            if (script[scriptIdx].content != NULL)
                hrnBackupLocal.script[hrnBackupLocal.scriptSize].content = bufDup(script[scriptIdx].content);

            hrnBackupLocal.scriptSize++;
        }
    }
    MEM_CONTEXT_END();
}

void
hrnBackupScriptSet(const HrnBackupScript *const script, const unsigned int scriptSize)
{
    if (hrnBackupLocal.scriptSize != 0)
        THROW(AssertError, "previous backup script has not yet completed");

    hrnBackupScriptAdd(script, scriptSize);
}

/**********************************************************************************************************************************/
static void
backupProcessScript(const bool after)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BOOL, after);
    FUNCTION_HARNESS_END();

    // If any file changes are scripted then make them
    if (hrnBackupLocal.scriptSize != 0)
    {
        bool done = true;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            Storage *const storageTest = storagePosixNewP(strNewZ(testPath()), .write = true);

            for (unsigned int scriptIdx = 0; scriptIdx < hrnBackupLocal.scriptSize; scriptIdx++)
            {
                // Do not perform ops that have already run
                if (hrnBackupLocal.script[scriptIdx].exec != 0)
                {
                    // Perform ops for this exec
                    if (hrnBackupLocal.script[scriptIdx].exec == 1)
                    {
                        if (hrnBackupLocal.script[scriptIdx].after == after)
                        {
                            switch (hrnBackupLocal.script[scriptIdx].op)
                            {
                                // Remove file
                                case hrnBackupScriptOpRemove:
                                    storageRemoveP(storageTest, hrnBackupLocal.script[scriptIdx].file);
                                    break;

                                // Update file
                                case hrnBackupScriptOpUpdate:
                                    storagePutP(
                                        storageNewWriteP(
                                            storageTest, hrnBackupLocal.script[scriptIdx].file,
                                            .timeModified = hrnBackupLocal.script[scriptIdx].time),
                                        hrnBackupLocal.script[scriptIdx].content == NULL ?
                                            BUFSTRDEF("") : hrnBackupLocal.script[scriptIdx].content);
                                    break;

                                default:
                                    THROW_FMT(
                                        AssertError, "unknown backup script op '%s'",
                                        strZ(strIdToStr(hrnBackupLocal.script[scriptIdx].op)));
                            }

                            hrnBackupLocal.script[scriptIdx].exec = 0;
                        }
                        // Preserve op for after exec
                        else
                            done = false;
                    }
                    // Decrement exec count (and preserve op for next exec)
                    else
                    {
                        // Only decrement when the after exec has run
                        if (after)
                            hrnBackupLocal.script[scriptIdx].exec--;

                        done = false;
                    }
                }
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Free script if all ops have been completed
        if (done)
        {
            memContextFree(hrnBackupLocal.memContext);
            hrnBackupLocal.scriptSize = 0;
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
backupProcess(
    const BackupData *const backupData, Manifest *const manifest, const bool preliminary, const String *const cipherPassBackup,
    const uint64_t copySizePrelim, const uint64_t copySizeFinal)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BACKUP_DATA, backupData);
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRING, cipherPassBackup);
    FUNCTION_HARNESS_END();

    backupProcessScript(false);
    backupProcess_SHIMMED(backupData, manifest, preliminary, cipherPassBackup, copySizePrelim, copySizeFinal);
    backupProcessScript(true);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnCmdBackup(void)
{
    FUNCTION_HARNESS_VOID();

    lockInit(STR(testPath()), cfgOptionStr(cfgOptExecId));
    cmdLockAcquireP();

    TRY_BEGIN()
    {
        cmdBackup();
    }
    FINALLY()
    {
        cmdLockReleaseP();
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
        pgControl.checkpointTime = backupTimeStart - 60;
        pgControl.timeline = param.timeline;

        HRN_STORAGE_PUT(
            storagePgIdxWrite(0), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, hrnPgControlToBuffer(0, 0, pgControl),
            .timeModified = backupTimeStart);

        // Determine if tablespaces are present
        const bool tablespace = !strLstEmpty(storageListP(storagePgIdx(0), STRDEF(PG_PATH_PGTBLSPC)));

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
            {
                if (param.backupStandbyError)
                {
                    HRN_PQ_SCRIPT_ADD(
                        {.session = 2, .function = HRN_PQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
                        {.session = 2, .function = HRN_PQ_STATUS, .resultInt = CONNECTION_BAD},
                        {.session = 2, .function = HRN_PQ_ERRORMESSAGE, .resultZ = "error"});

                    param.backupStandby = false;
                }
                else
                {
                    HRN_PQ_SCRIPT_ADD(
                        HRN_PQ_SCRIPT_OPEN_GE_96(2, "dbname='postgres' port=5433", pgVersion, pg2Path, true, NULL, NULL));
                }
            }
        }

        // Get start time
        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TIME_QUERY(1, (int64_t)backupTimeStart * 1000));

        // First phase of full/incr backup
        const bool backupfullIncr =
            param.fullIncr || (cfgOptionBool(cfgOptBackupFullIncr) && cfgOptionStrId(cfgOptType) == backupTypeFull);

        if (backupfullIncr)
        {
            // Tablespace check
            if (tablespace)
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TABLESPACE_LIST_1(1, 32768, "tblspc32768"));
            else
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_TABLESPACE_LIST_0(1));

            if (!param.fullIncrNoOp)
            {
                // Wait for standby to sync
                if (param.backupStandby)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_REPLAY_WAIT_96(2, lsnStartStr));

                // Ping to check standby mode
                HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));

                if (param.backupStandby)
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));
            }
        }

        // Get advisory lock and check if backup is in progress (only for exclusive backup)
        if (pgVersion <= PG_VERSION_95)
        {
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_ADVISORY_LOCK(1, true));
            HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_IN_BACKUP(1, false));
        }

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
        if (tablespace)
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
                // If full/incr then the first ping has already been done
                if (!backupfullIncr || param.fullIncrNoOp)
                {
                    // Ping to check standby mode
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));

                    if (param.backupStandby)
                        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));
                }

                // Continue if there is no error after copy start
                if (!param.errorAfterCopyStart)
                {
                    HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));

                    if (param.backupStandby)
                        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));

                    // Stop backup
                    if (pgVersion <= PG_VERSION_95)
                        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(1, lsnStopStr, walSegmentStop));
                    else if (pgVersion <= PG_VERSION_96)
                        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_96(1, lsnStopStr, walSegmentStop, tablespace));
                    else
                        HRN_PQ_SCRIPT_ADD(HRN_PQ_SCRIPT_STOP_BACKUP_GE_10(1, lsnStopStr, walSegmentStop, tablespace));

                    // Get stop time
                    HRN_PQ_SCRIPT_ADD(
                        HRN_PQ_SCRIPT_TIME_QUERY(1, ((int64_t)backupTimeStart + (param.noArchiveCheck ? 52427 : 2)) * 1000));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();
}
