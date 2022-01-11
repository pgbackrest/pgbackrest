/***********************************************************************************************************************************
Harness for PostgreSQL Interface

Macros to create harness functions per PostgreSQL version.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H
#define TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H

#include "postgres/version.h"
#include "postgres/interface/version.vendor.h"

#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Get the catalog version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

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

#elif PG_VERSION >= PG_VERSION_93

#define HRN_PG_INTERFACE_CONTROL_TEST(version)                                                                                     \
    void                                                                                                                           \
    hrnPgInterfaceControl##version(PgControl pgControl, unsigned char *buffer)                                                     \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = PG_CONTROL_VERSION,                                                                              \
            .catalog_version_no = pgControl.catalogVersion,                                                                        \
            .checkPoint = pgControl.checkpoint,                                                                                    \
            .checkPointCopy =                                                                                                      \
            {                                                                                                                      \
                .ThisTimeLineID = pgControl.timeline,                                                                              \
            },                                                                                                                     \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
            .data_checksum_version = pgControl.pageChecksum,                                                                       \
        };                                                                                                                         \
    }

#elif PG_VERSION >= PG_VERSION_90

#define HRN_PG_INTERFACE_CONTROL_TEST(version)                                                                                     \
    void                                                                                                                           \
    hrnPgInterfaceControl##version(PgControl pgControl, unsigned char *buffer)                                                     \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
        ASSERT(!pgControl.pageChecksum);                                                                                           \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = PG_CONTROL_VERSION,                                                                              \
            .catalog_version_no = pgControl.catalogVersion,                                                                        \
            .checkPoint =                                                                                                          \
            {                                                                                                                      \
                .xlogid = (uint32_t)(pgControl.checkpoint >> 32),                                                                  \
                .xrecoff = (uint32_t)(pgControl.checkpoint & 0xFFFFFFFF),                                                          \
            },                                                                                                                     \
            .checkPointCopy =                                                                                                      \
            {                                                                                                                      \
                .ThisTimeLineID = pgControl.timeline,                                                                              \
            },                                                                                                                     \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Create a WAL file
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

#define HRN_PG_INTERFACE_WAL_TEST(version)                                                                                         \
    void                                                                                                                           \
    hrnPgInterfaceWal##version(PgWal pgWal, unsigned char *buffer)                                                                 \
    {                                                                                                                              \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_magic = XLOG_PAGE_MAGIC;                                                       \
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
