/***********************************************************************************************************************************
Harness for PostgreSQL Interface (see PG_VERSION for version)
***********************************************************************************************************************************/
#include "build.auto.h"

#define PG_VERSION                                                  PG_VERSION_12
#define FORK_GPDB

#include "common/harnessPostgres/harnessVersion.intern.h"

HRN_PG_INTERFACE(120GPDB);
