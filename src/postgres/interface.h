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
#define PG_FILE_PGCONTROL                                           "pg_control"
#define PG_FILE_PGFILENODEMAP                                       "pg_filenode.map"
#define PG_FILE_PGINTERNALINIT                                      "pg_internal.init"
#define PG_FILE_PGVERSION                                           "PG_VERSION"
    STRING_DECLARE(PG_FILE_PGVERSION_STR);
#define PG_FILE_POSTGRESQLAUTOCONF                                  "postgresql.auto.conf"
    STRING_DECLARE(PG_FILE_POSTGRESQLAUTOCONF_STR);
#define PG_FILE_POSTGRESQLAUTOCONFTMP                               "postgresql.auto.conf.tmp"
#define PG_FILE_POSTMASTEROPTS                                      "postmaster.opts"
#define PG_FILE_POSTMASTERPID                                       "postmaster.pid"
    STRING_DECLARE(PG_FILE_POSTMASTERPID_STR);
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
Name of default PostgreSQL database used for running all queries and commands
***********************************************************************************************************************************/
#define PG_DB_POSTGRES                                              "postgres"
    STRING_DECLARE(PG_DB_POSTGRES_STR);

/***********************************************************************************************************************************
Define default page size

Page size can only be changed at compile time and is not known to be well-tested, so only the default page size is supported.
***********************************************************************************************************************************/
#define PG_PAGE_SIZE_DEFAULT                                        ((unsigned int)(8 * 1024))

/***********************************************************************************************************************************
Define the minimum oid that can be used for a user object

Everything below this number should have been created at initdb time.
***********************************************************************************************************************************/
#define PG_USER_OBJECT_MIN_ID                                       16384

/***********************************************************************************************************************************
Define default segment size and pages per segment

Segment size can only be changed at compile time and is not known to be well-tested, so only the default segment size is supported.
***********************************************************************************************************************************/
#define PG_SEGMENT_SIZE_DEFAULT                                     ((unsigned int)(1 * 1024 * 1024 * 1024))
#define PG_SEGMENT_PAGE_DEFAULT                                     (PG_SEGMENT_SIZE_DEFAULT / PG_PAGE_SIZE_DEFAULT)

/***********************************************************************************************************************************
WAL header size. It doesn't seem worth tracking the exact size of the WAL header across versions of PostgreSQL so just set it to
something far larger needed but <= the minimum read size on just about any system.
***********************************************************************************************************************************/
#define PG_WAL_HEADER_SIZE                                          ((unsigned int)(512))

/***********************************************************************************************************************************
Define default wal segment size

Before PostgreSQL 11 WAL segment size could only be changed at compile time and is not known to be well-tested, so only the default
WAL segment size is supported for versions below 11.
***********************************************************************************************************************************/
#define PG_WAL_SEGMENT_SIZE_DEFAULT                                 ((unsigned int)(16 * 1024 * 1024))

/***********************************************************************************************************************************
PostgreSQL Control File Info
***********************************************************************************************************************************/
typedef struct PgControl
{
    unsigned int version;
    uint64_t systemId;

    unsigned int pageSize;
    unsigned int walSegmentSize;

    bool pageChecksum;
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
// Get the catalog version for a PostgreSQL version
uint32_t pgCatalogVersion(unsigned int pgVersion);

// Get info from pg_control
PgControl pgControlFromFile(const Storage *storage);
PgControl pgControlFromBuffer(const Buffer *controlFile);

// Get the control version for a PostgreSQL version
uint32_t pgControlVersion(unsigned int pgVersion);

// Convert version string to version number and vice versa
unsigned int pgVersionFromStr(const String *version);
String *pgVersionToStr(unsigned int version);

// Get info from WAL header
PgWal pgWalFromFile(const String *walFile, const Storage *storage);
PgWal pgWalFromBuffer(const Buffer *walBuffer);

// Get the tablespace identifier used to distinguish versions in a tablespace directory, e.g. PG_9.0_201008051
String *pgTablespaceId(unsigned int pgVersion);

// Convert a string to an lsn and vice versa
uint64_t pgLsnFromStr(const String *lsn);
String *pgLsnToStr(uint64_t lsn);

// Convert a timeline and lsn to a wal segment and vice versa
String *pgLsnToWalSegment(uint32_t timeline, uint64_t lsn, unsigned int walSegmentSize);
uint64_t pgLsnFromWalSegment(const String *walSegment, unsigned int walSegmentSize);

// Convert a timeline and lsn range to a list of wal segments
StringList *pgLsnRangeToWalSegmentList(
    unsigned int pgVersion, uint32_t timeline, uint64_t lsnStart, uint64_t lsnStop, unsigned int walSegmentSize);

// Get name used for lsn in functions (this was changed in PostgreSQL 10 for consistency since lots of names were changing)
const String *pgLsnName(unsigned int pgVersion);

// Calculate the checksum for a page. Page cannot be const because the page header is temporarily modified during processing.
uint16_t pgPageChecksum(unsigned char *page, uint32_t blockNo);

const String *pgWalName(unsigned int pgVersion);

// Get wal path (this was changed in PostgreSQL 10 to avoid including "log" in the name)
const String *pgWalPath(unsigned int pgVersion);

// Get transaction commit log path (this was changed in PostgreSQL 10 to avoid including "log" in the name)
const String *pgXactPath(unsigned int pgVersion);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    // Create pg_control for testing
    Buffer *pgControlTestToBuffer(PgControl pgControl);

    // Create WAL for testing
    void pgWalTestToBuffer(PgWal pgWal, Buffer *walBuffer);
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *pgControlToLog(const PgControl *pgControl);
String *pgWalToLog(const PgWal *pgWal);

#define FUNCTION_LOG_PG_CONTROL_TYPE                                                                                               \
    PgControl
#define FUNCTION_LOG_PG_CONTROL_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, pgControlToLog, buffer, bufferSize)

#define FUNCTION_LOG_PG_WAL_TYPE                                                                                                   \
    PgWal
#define FUNCTION_LOG_PG_WAL_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, pgWalToLog, buffer, bufferSize)

#endif
