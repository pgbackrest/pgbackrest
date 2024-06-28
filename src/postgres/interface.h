/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_H
#define POSTGRES_INTERFACE_H

#include <stdint.h>
#include <sys/types.h>

#include "common/debug.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Defines for various Postgres paths and files
***********************************************************************************************************************************/
#define PG_FILE_BACKUPLABEL                                         "backup_label"
#define PG_FILE_BACKUPLABELOLD                                      "backup_label.old"
#define PG_FILE_BACKUPMANIFEST                                      "backup_manifest"
#define PG_FILE_BACKUPMANIFEST_TMP                                  "backup_manifest.tmp"
#define PG_FILE_PGCONTROL                                           "pg_control"
#define PG_FILE_PGFILENODEMAP                                       "pg_filenode.map"
#define PG_FILE_PGINTERNALINIT                                      "pg_internal.init"
#define PG_FILE_PGVERSION                                           "PG_VERSION"
STRING_DECLARE(PG_FILE_PGVERSION_STR);
#define PG_FILE_POSTGRESQLAUTOCONF                                  "postgresql.auto.conf"
STRING_DECLARE(PG_FILE_POSTGRESQLAUTOCONF_STR);
#define PG_FILE_POSTGRESQLAUTOCONFTMP                               "postgresql.auto.conf.tmp"
#define PG_FILE_POSTMTROPTS                                         "postmas""ter.opts"
#define PG_FILE_POSTMTRPID                                          "postmas""ter.pid"
STRING_DECLARE(PG_FILE_POSTMTRPID_STR);
#define PG_FILE_RECOVERYCONF                                        "recovery.conf"
STRING_DECLARE(PG_FILE_RECOVERYCONF_STR);
#define PG_FILE_RECOVERYDONE                                        "recovery.done"
STRING_DECLARE(PG_FILE_RECOVERYDONE_STR);
#define PG_FILE_RECOVERYSIGNAL                                      "recovery.signal"
STRING_DECLARE(PG_FILE_RECOVERYSIGNAL_STR);
#define PG_FILE_STANDBYSIGNAL                                       "standby.signal"
STRING_DECLARE(PG_FILE_STANDBYSIGNAL_STR);
#define PG_FILE_TABLESPACEMAP                                       "tablespace_map"

#define PG_PATH_ARCHIVE_STATUS                                      "archive_status"
#define PG_PATH_BASE                                                "base"
#define PG_PATH_GLOBAL                                              "global"
STRING_DECLARE(PG_PATH_GLOBAL_STR);
#define PG_PATH_PGMULTIXACT                                         "pg_multixact"
#define PG_PATH_PGDYNSHMEM                                          "pg_dynshmem"
#define PG_PATH_PGNOTIFY                                            "pg_notify"
#define PG_PATH_PGREPLSLOT                                          "pg_replslot"
#define PG_PATH_PGSERIAL                                            "pg_serial"
#define PG_PATH_PGSNAPSHOTS                                         "pg_snapshots"
#define PG_PATH_PGSTATTMP                                           "pg_stat_tmp"
#define PG_PATH_PGSUBTRANS                                          "pg_subtrans"
#define PG_PATH_PGTBLSPC                                            "pg_tblspc"

#define PG_PREFIX_PGSQLTMP                                          "pgsql_tmp"

#define PG_NAME_WAL                                                 "wal"
STRING_DECLARE(PG_NAME_WAL_STR);
#define PG_NAME_XLOG                                                "xlog"
STRING_DECLARE(PG_NAME_XLOG_STR);

/***********************************************************************************************************************************
Define allowed page sizes
***********************************************************************************************************************************/
typedef enum
{
    pgPageSize1 = 1 * 1024,
    pgPageSize2 = 2 * 1024,
    pgPageSize4 = 4 * 1024,
    pgPageSize8 = 8 * 1024,
    pgPageSize16 = 16 * 1024,
    pgPageSize32 = 32 * 1024,
} PgPageSize;

/***********************************************************************************************************************************
Define default segment size and pages per segment

Segment size can only be changed at compile time and is not known to be well-tested, so only the default segment size is supported.
***********************************************************************************************************************************/
#define PG_SEGMENT_SIZE_DEFAULT                                     ((unsigned int)(1 * 1024 * 1024 * 1024))

/***********************************************************************************************************************************
WAL header size. It doesn't seem worth tracking the exact size of the WAL header across versions of PostgreSQL so just set it to
something far larger needed but <= the minimum read size on just about any system.
***********************************************************************************************************************************/
#define PG_WAL_HEADER_SIZE                                          ((unsigned int)(512))

/***********************************************************************************************************************************
Checkpoint written into pg_control on restore. This will prevent PostgreSQL from starting if backup_label is not present.
***********************************************************************************************************************************/
#define PG_CONTROL_CHECKPOINT_INVALID                               0xDEAD

/***********************************************************************************************************************************
PostgreSQL Control File Info
***********************************************************************************************************************************/
typedef struct PgControl
{
    unsigned int version;
    uint64_t systemId;
    unsigned int catalogVersion;

    uint64_t checkpoint;                                            // Last checkpoint LSN
    time_t checkpointTime;                                          // Last checkpoint time
    uint32_t timeline;                                              // Current timeline

    PgPageSize pageSize;
    unsigned int walSegmentSize;

    unsigned int pageChecksumVersion;                               // Page checksum version (0 if no checksum, 1 if checksum)
} PgControl;

/***********************************************************************************************************************************
PostgreSQL WAL Info
***********************************************************************************************************************************/
typedef struct PgWal
{
    unsigned int version;
    unsigned int size;
    uint64_t systemId;
} PgWal;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is this a template database?
FN_EXTERN bool pgDbIsTemplate(const String *name);

// Is this a system database, i.e. template or postgres?
FN_EXTERN bool pgDbIsSystem(const String *name);

// Does this database have a system id, i.e. less than the minimum assignable user id?
FN_EXTERN bool pgDbIsSystemId(unsigned int id);

// Get info from pg_control
FN_EXTERN Buffer *pgControlBufferFromFile(const Storage *storage, const String *pgVersionForce);
FN_EXTERN PgControl pgControlFromFile(const Storage *storage, const String *pgVersionForce);

// Invalidate checkpoint record in pg_control
FN_EXTERN void pgControlCheckpointInvalidate(Buffer *buffer, const String *pgVersionForce);

// Get the control version for a PostgreSQL version
FN_EXTERN uint32_t pgControlVersion(unsigned int pgVersion);

// Convert version string to version number and vice versa
FN_EXTERN unsigned int pgVersionFromStr(const String *version);
FN_EXTERN String *pgVersionToStr(unsigned int version);

// Get info from WAL header
FN_EXTERN PgWal pgWalFromFile(const String *walFile, const Storage *storage, const String *pgVersionForce);
FN_EXTERN PgWal pgWalFromBuffer(const Buffer *walBuffer, const String *pgVersionForce);

// Get the tablespace identifier used to distinguish versions in a tablespace directory, e.g. PG_15_202209061
FN_EXTERN String *pgTablespaceId(unsigned int pgVersion, unsigned int pgCatalogVersion);

// Convert a string to an lsn and vice versa
FN_EXTERN uint64_t pgLsnFromStr(const String *lsn);
FN_EXTERN String *pgLsnToStr(uint64_t lsn);

// Convert a timeline and lsn to a wal segment and vice versa
FN_EXTERN String *pgLsnToWalSegment(uint32_t timeline, uint64_t lsn, unsigned int walSegmentSize);

// Get timeline from WAL segment name
FN_EXTERN uint32_t pgTimelineFromWalSegment(const String *walSegment);

// Convert a timeline and lsn range to a list of wal segments
FN_EXTERN StringList *pgLsnRangeToWalSegmentList(
    uint32_t timeline, uint64_t lsnStart, uint64_t lsnStop, unsigned int walSegmentSize);

// Get name used for lsn in functions (this was changed in PostgreSQL 10 for consistency since lots of names were changing)
FN_EXTERN const String *pgLsnName(unsigned int pgVersion);

// Calculate the checksum for a page. Page cannot be const because the page header is temporarily modified during processing.
FN_EXTERN uint16_t pgPageChecksum(unsigned char *page, uint32_t blockNo, PgPageSize pageSize);

// Returns true if page size is valid, false otherwise
FN_EXTERN bool pgPageSizeValid(PgPageSize pageSize);

// Throws an error if page size is not valid
FN_EXTERN void pgPageSizeCheck(PgPageSize pageSize);

FN_EXTERN const String *pgWalName(unsigned int pgVersion);

// Get wal path (this was changed in PostgreSQL 10 to avoid including "log" in the name)
FN_EXTERN const String *pgWalPath(unsigned int pgVersion);

// Get transaction commit log path (this was changed in PostgreSQL 10 to avoid including "log" in the name)
FN_EXTERN const String *pgXactPath(unsigned int pgVersion);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void pgControlToLog(const PgControl *pgControl, StringStatic *debugLog);
FN_EXTERN void pgWalToLog(const PgWal *pgWal, StringStatic *debugLog);

#define FUNCTION_LOG_PG_CONTROL_TYPE                                                                                               \
    PgControl
#define FUNCTION_LOG_PG_CONTROL_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(&value, pgControlToLog, buffer, bufferSize)

#define FUNCTION_LOG_PG_WAL_TYPE                                                                                                   \
    PgWal
#define FUNCTION_LOG_PG_WAL_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_OBJECT_FORMAT(&value, pgWalToLog, buffer, bufferSize)

#endif
