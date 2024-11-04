/***********************************************************************************************************************************
Harness for PostgreSQL Interface (see PG_VERSION for version)
***********************************************************************************************************************************/
#include "build.auto.h"

#define PG_VERSION                                                  PG_VERSION_11

#include "common/harnessPostgres/harnessVersion.intern.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

HRN_PG_INTERFACE(110);

#pragma GCC diagnostic pop
