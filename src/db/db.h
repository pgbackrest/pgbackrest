/***********************************************************************************************************************************
Database Client

Implements the required PostgreSQL queries and commands. Notice that there is no general purpose query function -- all queries are
expected to be embedded in this object.
***********************************************************************************************************************************/
#ifndef DB_DB_H
#define DB_DB_H

#include "common/type/object.h"
#include "postgres/client.h"
#include "postgres/interface.h"
#include "protocol/client.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Db Db;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Db *dbNew(PgClient *client, ProtocolClient *remoteClient, const Storage *storage, const String *applicationName);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct DbPub
{
    MemContext *memContext;                                         // Mem context
    PgControl pgControl;                                            // Control info
    const String *archiveMode;                                      // The archive_mode reported by the database
    const String *archiveCommand;                                   // The archive_command reported by the database
    const String *pgDataPath;                                       // Data directory reported by the database
    bool standby;                                                   // Is the cluster a standby?
    unsigned int pgVersion;                                         // Version as reported by the database
    TimeMSec checkpointTimeout;                                     // The checkpoint timeout reported by the database
    TimeMSec dbTimeout;                                             // Database timeout for statements/queries from PG client
} DbPub;

// Archive mode loaded from the archive_mode GUC
FN_INLINE_ALWAYS const String *
dbArchiveMode(const Db *const this)
{
    return THIS_PUB(Db)->archiveMode;
}

// Archive command loaded from the archive_command GUC
FN_INLINE_ALWAYS const String *
dbArchiveCommand(const Db *const this)
{
    return THIS_PUB(Db)->archiveCommand;
}

// Control data
FN_INLINE_ALWAYS PgControl
dbPgControl(const Db *const this)
{
    return THIS_PUB(Db)->pgControl;
}

// Data path loaded from the data_directory GUC
FN_INLINE_ALWAYS const String *
dbPgDataPath(const Db *const this)
{
    return THIS_PUB(Db)->pgDataPath;
}

// Is the cluster a standby?
FN_INLINE_ALWAYS bool
dbIsStandby(const Db *const this)
{
    return THIS_PUB(Db)->standby;
}

// Version loaded from the server_version_num GUC
FN_INLINE_ALWAYS unsigned int
dbPgVersion(const Db *const this)
{
    return THIS_PUB(Db)->pgVersion;
}

// Checkpoint timeout loaded from the checkpoint_timeout GUC
FN_INLINE_ALWAYS TimeMSec
dbCheckpointTimeout(const Db *const this)
{
    return THIS_PUB(Db)->checkpointTimeout;
}

// Database timeout from main/remote process
FN_INLINE_ALWAYS TimeMSec
dbDbTimeout(const Db *const this)
{
    return THIS_PUB(Db)->dbTimeout;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the db connection
FN_EXTERN void dbOpen(Db *this);

// Start backup and return starting lsn and wal segment name
typedef struct DbBackupStartResult
{
    String *lsn;
    String *walSegmentName;
    String *walSegmentCheck;                                        // Segment used to check archiving, may be NULL
} DbBackupStartResult;

FN_EXTERN DbBackupStartResult dbBackupStart(Db *this, bool startFast, bool stopAuto, bool archiveCheck);

// Stop backup and return starting lsn, wal segment name, backup label, and tablespace map
typedef struct DbBackupStopResult
{
    String *lsn;
    String *walSegmentName;
    String *backupLabel;
    String *tablespaceMap;
} DbBackupStopResult;

FN_EXTERN DbBackupStopResult dbBackupStop(Db *this);

// Get list of databases in the cluster: select oid, datname, datlastsysoid from pg_database
FN_EXTERN Pack *dbList(Db *this);

// Waits for replay on the standby to equal the target LSN
FN_EXTERN void dbReplayWait(Db *this, const String *targetLsn, uint32_t targetTimeline, TimeMSec timeout);

// Check that the cluster is alive and correctly configured during the backup
FN_EXTERN void dbPing(Db *this, bool force);

// Epoch time on the PostgreSQL host in ms
FN_EXTERN TimeMSec dbTimeMSec(Db *this);

// Get list of tablespaces in the cluster: select oid, datname, datlastsysoid from pg_database
FN_EXTERN Pack *dbTablespaceList(Db *this);

// Switch the WAL segment and return the segment that should have been archived
FN_EXTERN String *dbWalSwitch(Db *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS Db *
dbMove(Db *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
dbFree(Db *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void dbToLog(const Db *this, StringStatic *debugLog);

#define FUNCTION_LOG_DB_TYPE                                                                                                       \
    Db *
#define FUNCTION_LOG_DB_FORMAT(value, buffer, bufferSize)                                                                          \
    FUNCTION_LOG_OBJECT_FORMAT(value, dbToLog, buffer, bufferSize)

#endif
