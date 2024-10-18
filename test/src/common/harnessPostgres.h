/***********************************************************************************************************************************
Harness for PostgreSQL Interface
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_POSTGRES_H
#define TEST_COMMON_HARNESS_POSTGRES_H

#include "postgres/interface.h"
#include "postgres/version.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Control file size used to create pg_control
***********************************************************************************************************************************/
#define HRN_PG_CONTROL_SIZE                                         8192

/***********************************************************************************************************************************
Default wal segment size
***********************************************************************************************************************************/
#define HRN_PG_WAL_SEGMENT_SIZE_DEFAULT                             ((unsigned int)(16 * 1024 * 1024))

/***********************************************************************************************************************************
System id constants by version
***********************************************************************************************************************************/
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
#define HRN_PG_SYSTEMID_15                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_15)
#define HRN_PG_SYSTEMID_15_Z                                        "10000000000000150000"
#define HRN_PG_SYSTEMID_16                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_16)
#define HRN_PG_SYSTEMID_16_Z                                        "10000000000000160000"
#define HRN_PG_SYSTEMID_17                                          (10000000000000000000ULL + (uint64_t)PG_VERSION_17)
#define HRN_PG_SYSTEMID_17_Z                                        "10000000000000170000"

/***********************************************************************************************************************************
Put a control file to storage
***********************************************************************************************************************************/
#define HRN_PG_CONTROL_PUT(storageParam, versionParam, ...)                                                                        \
    HRN_STORAGE_PUT(                                                                                                               \
        storageParam, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,                                                                        \
        hrnPgControlToBuffer(0, 0, (PgControl){.version = versionParam, __VA_ARGS__}))

#define HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(storageParam, versionParam, controlVersionParam, ...)                                  \
    HRN_STORAGE_PUT(                                                                                                               \
        storageParam, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,                                                                        \
        hrnPgControlToBuffer(controlVersionParam, 0, (PgControl){.version = versionParam, __VA_ARGS__}))

#define HRN_PG_CONTROL_OVERRIDE_CRC_PUT(storageParam, versionParam, crcParam, ...)                                                 \
    HRN_STORAGE_PUT(                                                                                                               \
        storageParam, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,                                                                        \
        hrnPgControlToBuffer(0, crcParam, (PgControl){.version = versionParam, __VA_ARGS__}))

/***********************************************************************************************************************************
Copy WAL info to buffer
***********************************************************************************************************************************/
#define HRN_PG_WAL_TO_BUFFER(walBufferParam, versionParam, ...)                                                                    \
    hrnPgWalToBuffer(walBufferParam, 0, (PgWal){.version = versionParam, __VA_ARGS__})

#define HRN_PG_WAL_OVERRIDE_TO_BUFFER(walBufferParam, versionParam, magicParam, ...)                                               \
    hrnPgWalToBuffer(walBufferParam, magicParam, (PgWal){.version = versionParam, __VA_ARGS__})

/***********************************************************************************************************************************
Update control file time
***********************************************************************************************************************************/
#define HRN_PG_CONTROL_TIME(storageParam, timeParam, ...)                                                                          \
    HRN_STORAGE_TIME(storageParam, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, timeParam)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get the catalog version for a PostgreSQL version
unsigned int hrnPgCatalogVersion(unsigned int pgVersion);

// Create pg_control
Buffer *hrnPgControlToBuffer(unsigned int controlVersion, unsigned int crc, PgControl pgControl);

// Get system id by version
FN_INLINE_ALWAYS uint64_t
hrnPgSystemId(const unsigned int pgVersion)
{
    return 10000000000000000000ULL + (uint64_t)pgVersion;
}

// Create WAL for testing
void hrnPgWalToBuffer(Buffer *walBuffer, unsigned int magic, PgWal pgWal);

#endif
