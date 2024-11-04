/***********************************************************************************************************************************
Harness for PostgreSQL Interface

Macros to create harness functions per PostgreSQL version.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H
#define TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H

#include "postgres/interface/crc32.h"
#include "postgres/interface/version.vendor.h"

#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Get the catalog version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_95

#define HRN_PG_INTERFACE_CATALOG_VERSION(version)                                                                                  \
    uint32_t                                                                                                                       \
    hrnPgInterfaceCatalogVersion##version(void)                                                                                    \
    {                                                                                                                              \
        return CATALOG_VERSION_NO;                                                                                                 \
    }

#endif

/***********************************************************************************************************************************
Create a pg_control file
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_95

#define HRN_PG_INTERFACE_CONTROL_TEST(version)                                                                                     \
    void                                                                                                                           \
    hrnPgInterfaceControl##version(                                                                                                \
        const unsigned int controlVersion, const unsigned int crc, const PgControl pgControl, unsigned char *const buffer)         \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = controlVersion == 0 ? PG_CONTROL_VERSION : controlVersion,                                       \
            .catalog_version_no = pgControl.catalogVersion,                                                                        \
            .checkPoint = pgControl.checkpoint,                                                                                    \
            .checkPointCopy =                                                                                                      \
            {                                                                                                                      \
                .time = (pg_time_t)pgControl.checkpointTime,                                                                       \
                .ThisTimeLineID = pgControl.timeline,                                                                              \
            },                                                                                                                     \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
            .data_checksum_version = pgControl.pageChecksumVersion,                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        ((ControlFileData *)buffer)->crc = crc == 0 ? crc32cOne(buffer, offsetof(ControlFileData, crc)) : crc;                     \
    }

#endif

/***********************************************************************************************************************************
Create a WAL file
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_95

#define HRN_PG_INTERFACE_WAL_TEST(version)                                                                                         \
    void                                                                                                                           \
    hrnPgInterfaceWal##version(const unsigned int magic, const PgWal pgWal, unsigned char *const buffer)                           \
    {                                                                                                                              \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_magic = magic == 0 ? XLOG_PAGE_MAGIC : (uint16)magic;                          \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_info = XLP_LONG_HEADER;                                                        \
        ((XLogLongPageHeaderData *)buffer)->xlp_sysid = pgWal.systemId;                                                            \
        ((XLogLongPageHeaderData *)buffer)->xlp_seg_size = pgWal.size;                                                             \
    }

#endif

/***********************************************************************************************************************************
Call all macros with a single macro to make the vXXX.c files as simple as possible
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE(version)                                                                                                  \
    HRN_PG_INTERFACE_CATALOG_VERSION(version)                                                                                      \
    HRN_PG_INTERFACE_CONTROL_TEST(version)                                                                                         \
    HRN_PG_INTERFACE_WAL_TEST(version)

#endif
