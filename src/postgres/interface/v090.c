/***********************************************************************************************************************************
PostgreSQL 9.0 Interface

See postgres/interface/version.intern.h for documentation.
***********************************************************************************************************************************/
#include "build.auto.h"

#define PG_VERSION                                                  PG_VERSION_90

#define CheckPoint CheckPoint_90
#define ControlFileData ControlFileData_90
#define DBState DBState_90
#define DB_STARTUP DB_STARTUP_90
#define DB_SHUTDOWNED DB_SHUTDOWNED_90
#define DB_SHUTDOWNED_IN_RECOVERY DB_SHUTDOWNED_IN_RECOVERY_90
#define DB_SHUTDOWNING DB_SHUTDOWNING_90
#define DB_IN_CRASH_RECOVERY DB_IN_CRASH_RECOVERY_90
#define DB_IN_ARCHIVE_RECOVERY DB_IN_ARCHIVE_RECOVERY_90
#define DB_IN_PRODUCTION DB_IN_PRODUCTION_90
#define XLogLongPageHeaderData XLogLongPageHeaderData_90
#define XLogPageHeaderData XLogPageHeaderData_90
#define XLogRecPtr XLogRecPtr_90

#include "postgres/interface/version.intern.h"

PG_INTERFACE(090);

#undef PG_VERSION

#undef CheckPoint
#undef ControlFileData
#undef DBState
#undef DB_STARTUP
#undef DB_SHUTDOWNED
#undef DB_SHUTDOWNED_IN_RECOVERY
#undef DB_SHUTDOWNING
#undef DB_IN_CRASH_RECOVERY
#undef DB_IN_ARCHIVE_RECOVERY
#undef DB_IN_PRODUCTION
#undef XLogPageHeaderData
#undef XLogLongPageHeaderData
#undef XLogRecPtr

#undef CATALOG_VERSION_NO
#undef PG_CONTROL_VERSION
#undef XLOG_PAGE_MAGIC

#undef PG_INTERFACE_CONTROL_IS
#undef PG_INTERFACE_CONTROL
#undef PG_INTERFACE_CONTROL_VERSION
#undef PG_INTERFACE_WAL_IS
#undef PG_INTERFACE_WAL
#undef PG_INTERFACE

#define PG_VERSION                                                  PG_VERSION_11

#define CheckPoint CheckPoint_11
#define ControlFileData ControlFileData_11
#define DBState DBState_11
#define DB_STARTUP DB_STARTUP_11
#define DB_SHUTDOWNED DB_SHUTDOWNED_11
#define DB_SHUTDOWNED_IN_RECOVERY DB_SHUTDOWNED_IN_RECOVERY_11
#define DB_SHUTDOWNING DB_SHUTDOWNING_11
#define DB_IN_CRASH_RECOVERY DB_IN_CRASH_RECOVERY_11
#define DB_IN_ARCHIVE_RECOVERY DB_IN_ARCHIVE_RECOVERY_11
#define DB_IN_PRODUCTION DB_IN_PRODUCTION_11
#define XLogPageHeaderData XLogPageHeaderData_11
#define XLogLongPageHeaderData XLogLongPageHeaderData_11
#define XLogRecPtr XLogRecPtr_11

#include "postgres/interface/version.intern.h"

PG_INTERFACE(110);
