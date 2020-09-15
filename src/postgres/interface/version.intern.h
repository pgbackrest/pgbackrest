/***********************************************************************************************************************************
PostgreSQL Version Interface

Macros for building version-specific functions that interface with the types in version.vendor.h.  Due to the way PostgreSQL types
evolve over time, this seems to be the easiest way to extract information from them.

These macros should be kept as simple as possible, with most of the logic contained in postgres/interface.c.

Each version of PostgreSQL will need a vXXX.c file to contain the version-specific functions created by these macros.
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_VERSIONINTERN_H
#define POSTGRES_INTERFACE_VERSIONINTERN_H

#include "common/debug.h"
#include "postgres/interface/version.h"
#include "postgres/version.h"

#include "postgres/interface/version.vendor.h"

/***********************************************************************************************************************************
Get the catalog version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_CATALOG_VERSION(version)                                                                                      \
    uint32_t                                                                                                                       \
    pgInterfaceCatalogVersion##version(void)                                                                                       \
    {                                                                                                                              \
        return CATALOG_VERSION_NO;                                                                                                 \
    }

#endif

/***********************************************************************************************************************************
Determine if the supplied pg_control is for this version of PostgreSQL
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#ifdef PG_CONTROL_VERSION_DUPLICATE

#define PG_INTERFACE_CONTROL_IS(version)                                                                                           \
    bool                                                                                                                           \
    pgInterfaceControlIs##version(const unsigned char *controlFile)                                                                \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
                                                                                                                                   \
        return                                                                                                                     \
            ((ControlFileData *)controlFile)->pg_control_version == PG_CONTROL_VERSION &&                                          \
            ((ControlFileData *)controlFile)->catalog_version_no == CATALOG_VERSION_NO;                                            \
    }

#else

#define PG_INTERFACE_CONTROL_IS(version)                                                                                           \
    bool                                                                                                                           \
    pgInterfaceControlIs##version(const unsigned char *controlFile)                                                                \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
                                                                                                                                   \
        return ((ControlFileData *)controlFile)->pg_control_version == PG_CONTROL_VERSION;                                         \
    }

#endif // PG_CONTROL_VERSION_DUPLICATE

#endif

/***********************************************************************************************************************************
Read the version specific pg_control into a general data structure
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_93

#define PG_INTERFACE_CONTROL(version)                                                                                              \
    PgControl                                                                                                                      \
    pgInterfaceControl##version(const unsigned char *controlFile)                                                                  \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
        ASSERT(pgInterfaceControlIs##version(controlFile));                                                                        \
                                                                                                                                   \
        return (PgControl)                                                                                                         \
        {                                                                                                                          \
            .systemId = ((ControlFileData *)controlFile)->system_identifier,                                                       \
            .catalogVersion = ((ControlFileData *)controlFile)->catalog_version_no,                                                \
            .pageSize = ((ControlFileData *)controlFile)->blcksz,                                                                  \
            .walSegmentSize = ((ControlFileData *)controlFile)->xlog_seg_size,                                                     \
            .pageChecksum = ((ControlFileData *)controlFile)->data_checksum_version != 0,                                          \
        };                                                                                                                         \
    }

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_CONTROL(version)                                                                                              \
    PgControl                                                                                                                      \
    pgInterfaceControl##version(const unsigned char *controlFile)                                                                  \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
        ASSERT(pgInterfaceControlIs##version(controlFile));                                                                        \
                                                                                                                                   \
        return (PgControl)                                                                                                         \
        {                                                                                                                          \
            .systemId = ((ControlFileData *)controlFile)->system_identifier,                                                       \
            .catalogVersion = ((ControlFileData *)controlFile)->catalog_version_no,                                                \
            .pageSize = ((ControlFileData *)controlFile)->blcksz,                                                                  \
            .walSegmentSize = ((ControlFileData *)controlFile)->xlog_seg_size,                                                     \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Get the control version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_CONTROL_VERSION(version)                                                                                      \
    uint32_t                                                                                                                       \
    pgInterfaceControlVersion##version(void)                                                                                       \
    {                                                                                                                              \
        return PG_CONTROL_VERSION;                                                                                                 \
    }

#endif

/***********************************************************************************************************************************
Create a pg_control file for testing
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_93

#define PG_INTERFACE_CONTROL_TEST(version)                                                                                         \
    void                                                                                                                           \
    pgInterfaceControlTest##version(PgControl pgControl, unsigned char *buffer)                                                    \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = PG_CONTROL_VERSION,                                                                              \
            .catalog_version_no = CATALOG_VERSION_NO,                                                                              \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
            .data_checksum_version = pgControl.pageChecksum,                                                                       \
        };                                                                                                                         \
    }

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_CONTROL_TEST(version)                                                                                         \
    void                                                                                                                           \
    pgInterfaceControlTest##version(PgControl pgControl, unsigned char *buffer)                                                    \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
        ASSERT(!pgControl.pageChecksum);                                                                                           \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = PG_CONTROL_VERSION,                                                                              \
            .catalog_version_no = CATALOG_VERSION_NO,                                                                              \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Determine if the supplied WAL is for this version of PostgreSQL
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_WAL_IS(version)                                                                                               \
    bool                                                                                                                           \
    pgInterfaceWalIs##version(const unsigned char *walFile)                                                                        \
    {                                                                                                                              \
        ASSERT(walFile != NULL);                                                                                                   \
                                                                                                                                   \
        return ((XLogPageHeaderData *)walFile)->xlp_magic == XLOG_PAGE_MAGIC;                                                      \
    }

#endif

/***********************************************************************************************************************************
Read the version specific WAL header into a general data structure
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_WAL(version)                                                                                                  \
    PgWal                                                                                                                          \
    pgInterfaceWal##version(const unsigned char *walFile)                                                                          \
    {                                                                                                                              \
        ASSERT(walFile != NULL);                                                                                                   \
        ASSERT(pgInterfaceWalIs##version(walFile));                                                                                \
                                                                                                                                   \
        return (PgWal)                                                                                                             \
        {                                                                                                                          \
            .systemId = ((XLogLongPageHeaderData *)walFile)->xlp_sysid,                                                            \
            .size = ((XLogLongPageHeaderData *)walFile)->xlp_seg_size,                                                             \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Create a WAL file file for testing
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_83

#define PG_INTERFACE_WAL_TEST(version)                                                                                             \
    void                                                                                                                           \
    pgInterfaceWalTest##version(PgWal pgWal, unsigned char *buffer)                                                                \
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
#define PG_INTERFACE_BASE(version)                                                                                                 \
    PG_INTERFACE_CATALOG_VERSION(version)                                                                                          \
    PG_INTERFACE_CONTROL_IS(version)                                                                                               \
    PG_INTERFACE_CONTROL(version)                                                                                                  \
    PG_INTERFACE_CONTROL_VERSION(version)                                                                                          \
    PG_INTERFACE_WAL_IS(version)                                                                                                   \
    PG_INTERFACE_WAL(version)

#ifdef DEBUG

#define PG_INTERFACE(version)                                                                                                      \
    PG_INTERFACE_BASE(version)                                                                                                     \
    PG_INTERFACE_CONTROL_TEST(version)                                                                                             \
    PG_INTERFACE_WAL_TEST(version)

#else

#define PG_INTERFACE(version)                                                                                                      \
    PG_INTERFACE_BASE(version)

#endif

#endif
