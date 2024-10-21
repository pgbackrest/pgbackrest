/***********************************************************************************************************************************
PostgreSQL Version Constants
***********************************************************************************************************************************/
#ifndef POSTGRES_VERSION_H
#define POSTGRES_VERSION_H

#include "postgres/version.auto.h"

/***********************************************************************************************************************************
PostgreSQL name
***********************************************************************************************************************************/
#define PG_NAME                                                     "PostgreSQL"

/***********************************************************************************************************************************
Version where various PostgreSQL capabilities were introduced
***********************************************************************************************************************************/
// parallel query supported
#define PG_VERSION_PARALLEL_QUERY                                   PG_VERSION_96

// xlog was renamed to wal
#define PG_VERSION_WAL_RENAME                                       PG_VERSION_10

// recovery settings are implemented as GUCs (recovery.conf is no longer valid)
#define PG_VERSION_RECOVERY_GUC                                     PG_VERSION_12

#endif
