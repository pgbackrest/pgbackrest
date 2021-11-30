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
System id constants by version
***********************************************************************************************************************************/
#define HRN_PG_SYSTEMID_83                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_83)
#define HRN_PG_SYSTEMID_83_Z                                        "10000000000000080300"
#define HRN_PG_SYSTEMID_84                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_84)
#define HRN_PG_SYSTEMID_84_Z                                        "10000000000000080400"
#define HRN_PG_SYSTEMID_90                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_90)
#define HRN_PG_SYSTEMID_90_Z                                        "10000000000000090000"
#define HRN_PG_SYSTEMID_91                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_91)
#define HRN_PG_SYSTEMID_91_Z                                        "10000000000000090100"
#define HRN_PG_SYSTEMID_92                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_92)
#define HRN_PG_SYSTEMID_92_Z                                        "10000000000000090200"
#define HRN_PG_SYSTEMID_93                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_93)
#define HRN_PG_SYSTEMID_93_Z                                        "10000000000000090300"
#define HRN_PG_SYSTEMID_94                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_94)
#define HRN_PG_SYSTEMID_94_Z                                        "10000000000000090400"
#define HRN_PG_SYSTEMID_95                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_95)
#define HRN_PG_SYSTEMID_95_Z                                        "10000000000000090500"
#define HRN_PG_SYSTEMID_96                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_96)
#define HRN_PG_SYSTEMID_96_Z                                        "10000000000000090600"
#define HRN_PG_SYSTEMID_10                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_10)
#define HRN_PG_SYSTEMID_10_Z                                        "10000000000000100000"
#define HRN_PG_SYSTEMID_10_1_Z                                      "10000000000000100001"
#define HRN_PG_SYSTEMID_11                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_11)
#define HRN_PG_SYSTEMID_11_Z                                        "10000000000000110000"
#define HRN_PG_SYSTEMID_11_1_Z                                      "10000000000000110001"
#define HRN_PG_SYSTEMID_12                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_12)
#define HRN_PG_SYSTEMID_12_Z                                        "10000000000000120000"
#define HRN_PG_SYSTEMID_13                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_13)
#define HRN_PG_SYSTEMID_13_Z                                        "10000000000000130000"
#define HRN_PG_SYSTEMID_14                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_14)
#define HRN_PG_SYSTEMID_14_Z                                        "10000000000000140000"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get the catalog version for a PostgreSQL version
unsigned int hrnPgCatalogVersion(unsigned int pgVersion);

// Create pg_control
Buffer *hrnPgControlToBuffer(PgControl pgControl);

// Write pg_control to file
void hrnPgControlToFile(const Storage *storage, PgControl pgControl);

// Get system id by version
__attribute__((always_inline)) static inline uint64_t
hrnPgSystemId(const unsigned int pgVersion)
{
    return 10000000000000000000ULL + (uint64_t)pgVersion;
}

// Create WAL for testing
void hrnPgWalToBuffer(PgWal pgWal, Buffer *walBuffer);

#endif
