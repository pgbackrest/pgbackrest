/***********************************************************************************************************************************
Database Client

Implements the required PostgreSQL queries and commands.  Notice that there is no general purpose query function -- all queries are
expected to be embedded in this object.
***********************************************************************************************************************************/
#ifndef DB_DB_H
#define DB_DB_H

#include "postgres/client.h"
#include "protocol/client.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define DB_TYPE                                                     Db
#define DB_PREFIX                                                   db

typedef struct Db Db;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Db *dbNew(PgClient *client, ProtocolClient *remoteClient, const String *applicationName);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the db connection
void dbOpen(Db *this);

// Start backup and return starting lsn and wal segment name
typedef struct DbBackupStartResult
{
    String *lsn;
    String *walSegmentName;
} DbBackupStartResult;

DbBackupStartResult dbBackupStart(Db *this, bool startFast, bool stopAuto);

// Stop backup and return starting lsn, wal segment name, backup label, and tablspace map
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
void dbReplayWait(Db *this, const String *targetLsn, TimeMSec timeout);

// Epoch time on the PostgreSQL host in ms
TimeMSec dbTimeMSec(Db *this);

// Get list of tablespaces in the cluster: select oid, datname, datlastsysoid from pg_database
VariantList *dbTablespaceList(Db *this);

// Switch the WAL segment and return the segment that should have been archived
String *dbWalSwitch(Db *this);
void dbClose(Db *this);

// Move to a new parent mem context
Db *dbMove(Db *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Data path loaded from the data_directory GUC
const String *dbPgDataPath(const Db *this);

// Version loaded from the server_version_num GUC
unsigned int dbPgVersion(const Db *this);

// Archive mode loaded from the archive_mode GUC
const String *dbArchiveMode(const Db *this);

// Archive command loaded from the archive_command GUC
const String *dbArchiveCommand(const Db *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void dbFree(Db *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *dbToLog(const Db *this);

#define FUNCTION_LOG_DB_TYPE                                                                                                       \
    Db *
#define FUNCTION_LOG_DB_FORMAT(value, buffer, bufferSize)                                                                          \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, dbToLog, buffer, bufferSize)

#endif
