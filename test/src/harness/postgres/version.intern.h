/***********************************************************************************************************************************
Harness for PostgreSQL Interface

Macros for building version-specific functions that write the pg_control and WAL a test needs, using the types in version.vendor.h.
This is the write side of the interface the code reads with postgres/interface/version.intern.h.

The header is included again for each version, so it declares only what varies by version. Everything else the macros use is
included by the module that expands them.
***********************************************************************************************************************************/
#include "postgres/interface/version.vendor.h"

/***********************************************************************************************************************************
Get the catalog version
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE_CATALOG_VERSION(version)                                                                                  \
    static uint32_t                                                                                                                \
    hrnPgInterfaceCatalogVersion##version(void)                                                                                    \
    {                                                                                                                              \
        return CATALOG_VERSION_NO;                                                                                                 \
    }

/***********************************************************************************************************************************
Create a pg_control file
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE_CONTROL(version)                                                                                          \
    static void                                                                                                                    \
    hrnPgInterfaceControl##version(                                                                                                \
        const unsigned int controlVersion, const unsigned int crc, const PgControl pgControl, uint8_t *const buffer)               \
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
                .ThisTimeLineID = pgControl.timeline,                                                                              \
            },                                                                                                                     \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
            .data_checksum_version = pgControl.pageChecksumVersion,                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        ((ControlFileData *)buffer)->crc = crc == 0 ? crc32cOne(buffer, offsetof(ControlFileData, crc)) : crc;                     \
    }

/***********************************************************************************************************************************
Create a WAL file
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE_WAL(version)                                                                                              \
    static void                                                                                                                    \
    hrnPgInterfaceWal##version(const unsigned int magic, const PgWal pgWal, uint8_t *const buffer)                                 \
    {                                                                                                                              \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_magic = magic == 0 ? XLOG_PAGE_MAGIC : (uint16)magic;                          \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_info = XLP_LONG_HEADER;                                                        \
        ((XLogLongPageHeaderData *)buffer)->xlp_sysid = pgWal.systemId;                                                            \
        ((XLogLongPageHeaderData *)buffer)->xlp_seg_size = pgWal.size;                                                             \
    }
