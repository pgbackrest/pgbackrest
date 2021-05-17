/***********************************************************************************************************************************
Harness for PostgreSQL Interface
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_POSTGRES_H
#define TEST_COMMON_HARNESS_POSTGRES_H

#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Control file size used to create pg_control
***********************************************************************************************************************************/
#define HRN_PG_CONTROL_SIZE                                         8192

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get the catalog version for a PostgreSQL version
unsigned int hrnPgCatalogVersion(unsigned int pgVersion);

// Create pg_control
Buffer *hrnPgControlToBuffer(PgControl pgControl);

// Create WAL for testing
void hrnPgWalToBuffer(PgWal pgWal, Buffer *walBuffer);

#endif
