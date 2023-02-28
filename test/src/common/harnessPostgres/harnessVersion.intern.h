/***********************************************************************************************************************************
Harness for PostgreSQL Interface

Macros to create harness functions per PostgreSQL version.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H
#define TEST_COMMON_HARNESS_POSTGRES_VERSIONINTERN_H

#include "postgres/interface/version.vendor.h"

#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Get the catalog version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_93

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
    hrnPgInterfaceControl##version(const HrnPgControl hrnPgControl, unsigned char *const buffer)                                   \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = hrnPgControl.systemId,                                                                            \
            .pg_control_version = hrnPgControl.controlVersion,                                                                     \
            .catalog_version_no = hrnPgControl.catalogVersion,                                                                     \
            .checkPoint = hrnPgControl.checkpoint,                                                                                 \
            .checkPointCopy =                                                                                                      \
            {                                                                                                                      \
                .ThisTimeLineID = hrnPgControl.timeline,                                                                           \
            },                                                                                                                     \
            .blcksz = hrnPgControl.pageSize,                                                                                       \
            .xlog_seg_size = hrnPgControl.walSegmentSize,                                                                          \
            .data_checksum_version = hrnPgControl.pageChecksum,                                                                    \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Create a WAL file
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_93

#define HRN_PG_INTERFACE_WAL_TEST(version)                                                                                         \
    void                                                                                                                           \
    hrnPgInterfaceWal##version(const HrnPgWal hrnPgWal, unsigned char *const buffer)                                               \
    {                                                                                                                              \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_magic = hrnPgWal.magic == 0 ? XLOG_PAGE_MAGIC : (uint16)hrnPgWal.magic;        \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_info = XLP_LONG_HEADER;                                                        \
        ((XLogLongPageHeaderData *)buffer)->xlp_sysid = hrnPgWal.systemId;                                                         \
        ((XLogLongPageHeaderData *)buffer)->xlp_seg_size = hrnPgWal.size;                                                          \
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
