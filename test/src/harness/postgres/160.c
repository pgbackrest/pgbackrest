/***********************************************************************************************************************************
Harness for PostgreSQL Interface (see PG_VERSION for version)
***********************************************************************************************************************************/
#include <build.h>

#define PG_VERSION                                                  PG_VERSION_16

#include "harness/postgres/version.intern.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

HRN_PG_INTERFACE(160);

#pragma GCC diagnostic pop
