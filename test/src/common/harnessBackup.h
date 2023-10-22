/***********************************************************************************************************************************
Harness for Creating Test Backups
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_BACKUP_H
#define TEST_COMMON_HARNESS_BACKUP_H

// Backup start time epoch. The idea is to not have backup times (and therefore labels) ever change. Each backup added should be
// separated by 100,000 seconds (1,000,000 after stanza-upgrade) but after the initial assignments this will only be possible at the
// beginning and the end, so new backups added in the middle will average the start times of the prior and next backup to get their
// start time. Backups added to the beginning of the test will need to subtract from the epoch.
#define BACKUP_EPOCH                                                1570000000

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Create test backup and handle all the required locking. The backup configuration must already be loaded.
void hrnCmdBackup(void);

// Generate pq scripts for versions of PostgreSQL
typedef struct HrnBackupPqScriptParam
{
    VAR_PARAM_HEADER;
    bool startFast;                                                 // Start backup fast
    bool backupStandby;                                             // Backup from standby
    bool errorAfterStart;                                           // Error after backup start
    bool noWal;                                                     // Don't write test WAL segments
    bool noPriorWal;                                                // Don't write prior test WAL segments
    bool noArchiveCheck;                                            // Do not check archive
    bool walSwitch;                                                 // WAL switch is required
    bool tablespace;                                                // Is there a tablespace?
    CompressType walCompressType;                                   // Compress type for the archive files
    CipherType cipherType;                                          // Cipher type
    const char *cipherPass;                                         // Cipher pass
    unsigned int walTotal;                                          // Total WAL to write
    unsigned int timeline;                                          // Timeline to use for WAL files
    const String *pgVersionForce;                                   // PG version to use when control/catalog not found
} HrnBackupPqScriptParam;

#define hrnBackupPqScriptP(pgVersion, backupStartTime, ...)                                                                        \
    hrnBackupPqScript(pgVersion, backupStartTime, (HrnBackupPqScriptParam){VAR_PARAM_INIT, __VA_ARGS__})

void hrnBackupPqScript(unsigned int pgVersion, time_t backupTimeStart, HrnBackupPqScriptParam param);

#endif
