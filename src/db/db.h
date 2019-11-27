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
void dbOpen(Db *this);

// Start backup and return starting lsn and wal segment name
typedef struct DbBackupStartResult
{
    String *lsn;
    String *walSegmentName;
} DbBackupStartResult;

DbBackupStartResult dbBackupStart(Db *this, bool startFast);

// Stop backup and return starting lsn, wal segment name, backup label, and tablspace map
typedef struct DbBackupStopResult
{
    String *lsn;
    String *walSegmentName;
    String *backupLabel;
    String *tablespaceMap;
} DbBackupStopResult;

DbBackupStopResult dbBackupStop(Db *this);

bool dbIsStandby(Db *this);
String *dbWalSwitch(Db *this);
void dbClose(Db *this);

Db *dbMove(Db *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *dbPgDataPath(const Db *this);
unsigned int dbPgVersion(const Db *this);
const String *dbArchiveMode(const Db *this);
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
