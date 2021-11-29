/***********************************************************************************************************************************
Database Client

Implements the required PostgreSQL queries and commands.  Notice that there is no general purpose query function -- all queries are
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
Db *dbNew(PgClient *client, ProtocolClient *remoteClient, const Storage *storage, const String *applicationName);

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
    unsigned int pgVersion;                                         // Version as reported by the database
} DbPub;

// Archive mode loaded from the archive_mode GUC
__attribute__((always_inline)) static inline const String *
dbArchiveMode(const Db *const this)
{
    return THIS_PUB(Db)->archiveMode;
}

// Archive command loaded from the archive_command GUC
__attribute__((always_inline)) static inline const String *
dbArchiveCommand(const Db *const this)
{
    return THIS_PUB(Db)->archiveCommand;
}

// Control data
__attribute__((always_inline)) static inline PgControl
dbPgControl(const Db *const this)
{
    return THIS_PUB(Db)->pgControl;
}

// Data path loaded from the data_directory GUC
__attribute__((always_inline)) static inline const String *
dbPgDataPath(const Db *const this)
{
    return THIS_PUB(Db)->pgDataPath;
}

// Version loaded from the server_version_num GUC
__attribute__((always_inline)) static inline unsigned int
dbPgVersion(const Db *const this)
{
    return THIS_PUB(Db)->pgVersion;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the db connection
void dbOpen(Db *this);

// Start backup and return timeline, starting lsn, and wal segment info
typedef struct DbBackupStartResult
{
    uint32_t timeline;                                              // Current timeline
    String *lsn;
    String *walSegmentName;
    String *walSegmentCheck;                                        // Segment used to check archiving, may be NULL
} DbBackupStartResult;

DbBackupStartResult dbBackupStart(Db *this, bool startFast, bool stopAuto, bool archiveCheck);

// Stop backup and return starting lsn, wal segment name, backup label, and tablespace map
typedef struct DbBackupStopResult
{
    String *lsn;
    String *walSegmentName;
    String *backupLabel;
    String *tablespaceMap;
} DbBackupStopResult;

DbBackupStopResult dbBackupStop(Db *this);

// Is this cluster a standby?
bool dbIsStandby(Db *this);

// Get list of databases in the cluster: select oid, datname, datlastsysoid from pg_database
VariantList *dbList(Db *this);

// Waits for replay on the standby to equal the target LSN
void dbReplayWait(Db *this, const String *targetLsn, uint32_t targetTimeline, TimeMSec timeout);

// Epoch time on the PostgreSQL host in ms
TimeMSec dbTimeMSec(Db *this);

// Get list of tablespaces in the cluster: select oid, datname, datlastsysoid from pg_database
VariantList *dbTablespaceList(Db *this);

// Switch the WAL segment and return the segment that should have been archived
String *dbWalSwitch(Db *this);
void dbClose(Db *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline Db *
dbMove(Db *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
dbFree(Db *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *dbToLog(const Db *this);

#define FUNCTION_LOG_DB_TYPE                                                                                                       \
    Db *
#define FUNCTION_LOG_DB_FORMAT(value, buffer, bufferSize)                                                                          \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, dbToLog, buffer, bufferSize)

#endif
